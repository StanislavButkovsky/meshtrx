#include <Arduino.h>
#include <nvs_flash.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <esp_mac.h>
#include <esp_pm.h>
#include <esp_bt.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <ArduinoJson.h>

#include "packet.h"
#include "lora_radio.h"
#include "ble_service.h"
#include "oled_display.h"
#include "audio_codec.h"
#include "beacon.h"
#include "repeater.h"
#include "call_manager.h"
#include "battery.h"
#include "wifi_monitor.h"
#include <WiFi.h>

#include "debug.h"
#include "utils.h"
#include "test_console.h"

// === Пины ===
#define PIN_LED       35
#define PIN_USER_BTN  0

// === Очереди FreeRTOS ===
static QueueHandle_t txAudioQueue = nullptr;   // аудио пакеты для TX
static QueueHandle_t txTextQueue  = nullptr;   // текстовые пакеты для TX

// === Состояние ===
static volatile bool pttActive = false;
static volatile uint32_t pttStartedMs = 0;
uint8_t currentChannel = DEFAULT_CHANNEL;
static uint8_t audioSeqNum = 0;
static uint8_t textSeqNum = 0;
static uint8_t senderMac[2] = {0};  // последние 2 байта MAC
static int16_t lastRssi = 0;
static int8_t lastSnr = 0;

// === File Transfer v2 — state machine ===
enum FileTransferState {
  FILE_STATE_IDLE,
  FILE_STATE_UPLOADING,   // телефон загружает файл в ESP32 RAM
  FILE_STATE_SENDING,     // ESP32 отправляет по LoRa
  FILE_STATE_RECEIVING    // ESP32 принимает из LoRa
};
static volatile FileTransferState fileState = FILE_STATE_IDLE;
static uint16_t fileTimeoutSec = 60;  // настраиваемый, 30-180 сек
// Результат esp_pm_configure: без light sleep контроллер ест 45 мА вместо 2,5,
// и это главный расход батареи — состояние должно быть видно стенду.
static int pmConfigResult = -1;
#define FILE_RX_FIRST_CHUNK_TIMEOUT_MS 15000  // сессия без единого чанка

// RX буфер (приём из LoRa)
static uint8_t fileSessionId = 0;
static uint8_t* fileRxBuffer = nullptr;
static uint32_t fileRxSize = 0;
static uint16_t fileRxChunksTotal = 0;
static uint16_t fileRxChunksDone = 0;
static uint8_t fileRxType = 0;
static char fileRxName[20] = {0};
static uint8_t fileRxSender[2] = {0};
static uint32_t fileRxLastChunkMs = 0;
static volatile bool fileRxComplete = false;
static uint8_t fileRxBitmap[128]; // макс 1024 чанков
static uint16_t fileRxUniqueCount = 0;

// Кеш последнего ответа на FILE_END (ACK или NACK)
static uint8_t lastFileRespSessionId = 0xFF;
static uint32_t lastFileRespTimeMs = 0;
static uint8_t lastFileRespData[107]; // max: 7 + 50*2 = 107 байт
static size_t lastFileRespLen = 0;
#define FILE_RESP_CACHE_TTL_MS 30000  // помнить 30 сек

// TX буфер (загрузка от телефона → отправка по LoRa)
static uint8_t* fileTxBuffer = nullptr;
static uint32_t fileTxSize = 0;
static uint32_t fileTxOffset = 0;      // байт получено от BLE
static uint16_t fileTxChunksTotal = 0;
static uint8_t  fileTxType = 0;
static uint8_t  fileTxDest[2] = {0};
static char     fileTxName[20] = {0};
static uint8_t  fileTxSessionId = 0;
static uint8_t  fileTxNackRound = 0;
#define MAX_NACK_ROUNDS 10   // ограничитель — общий таймаут передачи, а не число раундов
// ACK/NACK от приёмника (заполняется в processLoRaPacket)
static volatile bool fileTxAckReceived = false;
static volatile uint8_t fileTxAckStatus = 0;
static uint16_t fileTxMissing[50];
static uint16_t fileTxMissingCount = 0;
static TaskHandle_t fileSendTaskHandle = nullptr;

// LED
static volatile bool fileTxActive = false;
static volatile uint32_t fileTxLedUntil = 0;

// Pending BLE upload status (для отправки из bleTask)
static volatile uint8_t pendingUploadStatus = 0xFF; // 0xFF = нет
static volatile uint8_t pendingUploadSession = 0;

// LED RX индикация
static volatile uint32_t rxLedUntil = 0;

// Флаг: fileSendTask просит loraTask перезапустить RX (после loraSend с Core 1)
static volatile bool loraNeedRxRestart = false;

// Дедупликация текстовых сообщений
#define TEXT_DEDUP_SIZE 16
#define TEXT_DEDUP_TTL_MS 30000
struct TextDedupEntry {
  uint8_t sender[2];
  uint8_t seq;
  uint32_t timestamp;
};
static TextDedupEntry textDedupCache[TEXT_DEDUP_SIZE];
static uint8_t textDedupHead = 0;

static bool textIsDuplicate(uint8_t* sender, uint8_t seq) {
  uint32_t now = millis();
  for (int i = 0; i < TEXT_DEDUP_SIZE; i++) {
    if (now - textDedupCache[i].timestamp > TEXT_DEDUP_TTL_MS) continue;
    if (textDedupCache[i].sender[0] == sender[0] &&
        textDedupCache[i].sender[1] == sender[1] &&
        textDedupCache[i].seq == seq) return true;
  }
  // Добавить
  textDedupCache[textDedupHead].sender[0] = sender[0];
  textDedupCache[textDedupHead].sender[1] = sender[1];
  textDedupCache[textDedupHead].seq = seq;
  textDedupCache[textDedupHead].timestamp = now;
  textDedupHead = (textDedupHead + 1) % TEXT_DEDUP_SIZE;
  return false;
}

// Кнопка USER
static uint32_t userBtnPressTime = 0;
static bool userBtnPressed = false;

// Батарея — кеш (читаем раз в 5 сек для экономии)
static float cachedBatV = 0.0f;
static uint32_t batReadTimer = 0;
#define BAT_READ_INTERVAL_MS 5000

// LoRa power mode — idle timer
static uint32_t lastLoraActivityMs = 0;
#define LORA_IDLE_TIMEOUT_MS  10000  // 10 сек без активности → duty cycle

static float getCachedBattery() {
  uint32_t now = millis();
  if (cachedBatV == 0.0f || now - batReadTimer >= BAT_READ_INTERVAL_MS) {
    cachedBatV = batteryReadVoltage();
    batReadTimer = now;
  }
  return cachedBatV;
}

// OLED — кеш для обновления только при изменениях
static int16_t prevRssi = -999;
static int8_t prevSnr = -99;
static bool prevBleConn = false;
static bool prevPtt = false;
static bool prevVox = false;
static float prevBatV = 0.0f;
static uint8_t prevChannel = 255;

// === Forward declarations ===
static void loraTaskFunc(void* param);
static void bleTaskFunc(void* param);
static void fileSendTask(void* param);
static void handleBleData(uint8_t* data, size_t len);
static void processLoRaPacket(uint8_t* data, int len, int16_t rssi, int8_t snr);
static void sendStatusUpdate();
static void pttStop(bool byTimeout);
static void pttStart();
static void loadSettings();
static void handleUserButton();

// ================================================================
// Power Management — auto light sleep + BLE modem sleep
// ================================================================
static uint32_t bootCount = 0;

// Диагностика перезагрузок: причина reset + сквозной счётчик boot'ов в NVS
static void logBootInfo() {
  esp_reset_reason_t r = esp_reset_reason();
  const char* name;
  switch (r) {
    case ESP_RST_POWERON:  name = "POWERON";  break;
    case ESP_RST_EXT:      name = "EXT";      break;
    case ESP_RST_SW:       name = "SW";       break;
    case ESP_RST_PANIC:    name = "PANIC";    break;
    case ESP_RST_INT_WDT:  name = "INT_WDT";  break;
    case ESP_RST_TASK_WDT: name = "TASK_WDT"; break;
    case ESP_RST_WDT:      name = "WDT";      break;
    case ESP_RST_DEEPSLEEP: name = "DEEPSLEEP"; break;
    case ESP_RST_BROWNOUT: name = "BROWNOUT"; break;
    default:               name = "UNKNOWN";  break;
  }
  Preferences prefs;
  prefs.begin("sys", false);
  bootCount = prefs.getUInt("boot_count", 0) + 1;
  prefs.putUInt("boot_count", bootCount);
  prefs.end();
  // Причина пробуждения печатается отдельно: после выключения кнопкой
  // устройство стартует как после сброса, и без этой строки не отличить
  // «разбудили кнопкой» от «дёрнули питание» — а вопросы будут именно такие.
  const char* woke = "";
  if (r == ESP_RST_DEEPSLEEP) {
    switch (esp_sleep_get_wakeup_cause()) {
      // Кнопка на плате может отозваться и как EXT0, и как GPIO — какой из
      // источников сработает первым, зависит от того, что включено в этой
      // сборке. Для человека это одно и то же: разбудили кнопкой.
      case ESP_SLEEP_WAKEUP_EXT0:
      case ESP_SLEEP_WAKEUP_GPIO: woke = " wake=BUTTON"; break;
      case ESP_SLEEP_WAKEUP_EXT1: woke = " wake=EXT1";   break;
      default:                    woke = " wake=OTHER";  break;
    }
  }
  LOG_F("EVT BOOT reason=%s code=%d count=%lu heap=%lu%s\n",
    name, (int)r, (unsigned long)bootCount, (unsigned long)ESP.getFreeHeap(), woke);
}

static void setupPowerManagement() {
  // Auto light sleep: CPU засыпает, когда все задачи заблокированы.
  // BLE остаётся активным — соединение не рвётся.
  esp_pm_config_t pm = {
    .max_freq_mhz = 240,
    .min_freq_mhz = 80,
    .light_sleep_enable = true
  };
  esp_err_t err = esp_pm_configure(&pm);
  pmConfigResult = err;
  if (err == ESP_OK) {
    LOG_D("[Power] Auto light sleep ENABLED (240/80 MHz)");
  } else {
    LOG_F("[Power] esp_pm_configure failed: 0x%x (работаем без light sleep)\n", err);
  }

  // GPIO wakeup: LoRa DIO1 + кнопка USER
  gpio_wakeup_enable(GPIO_NUM_14, GPIO_INTR_HIGH_LEVEL);  // LoRa DIO1
  gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);    // кнопка USER
  esp_sleep_enable_gpio_wakeup();

  // BLE wakeup — контроллер будит CPU на каждое соединение
  esp_sleep_enable_bt_wakeup();

  LOG_D("[Power] GPIO + BT wakeup configured");
}

// ================================================================
// SETUP
// ================================================================
#ifndef PIO_UNIT_TESTING
void setup() {
#ifndef NDEBUG
  Serial.begin(115200);
  delay(500);
#endif
  LOG_D("\n=== MeshTRX Starting ===");

  // Инициализация NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  // Диагностика: почему устройство стартовало и сколько раз всего
  logBootInfo();

  // LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  // Кнопка USER
  pinMode(PIN_USER_BTN, INPUT_PULLUP);

  // ADC для батареи
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);

  // Получить MAC для sender ID
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  senderMac[0] = mac[4];
  senderMac[1] = mac[5];

  // OLED
  oledInit();

  // Проверить режим ретранслятора
  repeaterInit();

  if (repeaterIsEnabled()) {
    // === РЕЖИМ РЕТРАНСЛЯТОРА ===
    LOG_D("[Main] === REPEATER MODE ===");

    loraInit();
    loadSettings();
    beaconInit();

    // BLE — чтобы можно было выключить ретранслятор через приложение
    bleInit();
    bleSetDataCallback(handleBleData);

    oledShowRepeater(currentChannel, loraGetFrequency(currentChannel),
                     loraGetTxPower(), loraIsDutyCycleEnabled(),
                     0, 0, 0, 0, "---", 0, 0, 0);

    // Запустить задачу ретранслятора
    xTaskCreatePinnedToCore(repeaterTask, "repeater", 8192, nullptr, 5, nullptr, 0);

    // Beacon задача (ретранслятор тоже пингует)
    xTaskCreatePinnedToCore(beaconTask, "beacon", 4096, nullptr, 2, nullptr, 1);

    // WiFi веб-монитор
    wifiMonitorInit();
    xTaskCreatePinnedToCore(wifiMonitorTask, "wifi", 4096, nullptr, 1, nullptr, 1);

    // Обработка кнопки USER в loop()
  } else {
    // === НОРМАЛЬНЫЙ РЕЖИМ ===
    LOG_D("[Main] === NORMAL MODE ===");

    // Отключить WiFi модем — не нужен без ретранслятора
    // WiFi.mode(WIFI_OFF) безопасен для BLE (в отличие от esp_wifi_deinit)
    WiFi.mode(WIFI_OFF);
    LOG_D("[Main] WiFi OFF");

    // Инициализация модулей
    loraInit();
    codecInit();
    beaconInit();
    callManagerInit();
    loadSettings();

    // BLE
    bleInit();
    bleSetDataCallback(handleBleData);

#ifdef TEST_CONSOLE
    testConsoleInit();
#endif

    // Power management — auto light sleep
    setupPowerManagement();

    // Очереди
    txAudioQueue = xQueueCreate(10, sizeof(LoRaAudioPacket));
    txTextQueue  = xQueueCreate(5, sizeof(LoRaTextPacket));

    // Начать приём LoRa
    loraStartReceive();

    // Обновить OLED
    oledShowMain(currentChannel, loraGetFrequency(currentChannel),
                 0, 0, loraGetTxPower(), false, loraIsDutyCycleEnabled(),
                 false, false, getCachedBattery());

    // FreeRTOS задачи
    xTaskCreatePinnedToCore(loraTaskFunc, "lora", 16384, nullptr, 5, &loraTaskHandle, 0);
    xTaskCreatePinnedToCore(bleTaskFunc, "ble", 4096, nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(beaconTask, "beacon", 4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(fileSendTask, "filesend", 8192, nullptr, 3, &fileSendTaskHandle, 1);
  }

  LOG_D("=== MeshTRX Ready ===");
}

// ================================================================
// LOOP — обработка кнопки USER + вызовы callTick
// ================================================================
void loop() {
#ifdef TEST_CONSOLE
  testConsoleTick();
#endif
  handleUserButton();
  oledSleepTick();

  if (!repeaterIsEnabled()) {
    callTick();
  }

  delay(500);  // фиксированно: кнопка и OLED реагируют одинаково с BLE и без
}
#endif // PIO_UNIT_TESTING

// ================================================================
// Загрузка настроек из NVS
// ================================================================
static void loadSettings() {
  Preferences prefs;
  prefs.begin("settings", true);

  int8_t txp = prefs.getChar("tx_power", TX_POWER_DBM);
  loraSetTxPower(txp);

  bool dc = prefs.getBool("duty_cycle", ENFORCE_DUTY_CYCLE);
  loraSetDutyCycle(dc);

  uint8_t ch = prefs.getUChar("channel", DEFAULT_CHANNEL);
  if (ch < NUM_CHANNELS) {
    currentChannel = ch;
    loraSetChannel(ch);
  }

  fileTimeoutSec = prefs.getUShort("file_timeout", 60);
  if (fileTimeoutSec < 30) fileTimeoutSec = 30;
  if (fileTimeoutSec > 180) fileTimeoutSec = 180;

  prefs.end();
  LOG_F("[Settings] Loaded from NVS (file_timeout=%ds)\n", fileTimeoutSec);
}

// ================================================================
// Кнопка USER (GPIO0)
// ================================================================

// Сколько держать кнопку, чтобы устройство выключилось. Три секунды в обычном
// режиме: меньше — выключалось бы от случайного нажатия в кармане, больше —
// человек решит, что кнопка не работает, и отпустит. В режиме ретранслятора
// порог выше: там удержания до трёх секунд уже заняты сбросом статистики и
// выходом из режима.
#define POWER_OFF_HOLD_MS           3000
#define POWER_OFF_HOLD_REPEATER_MS  8000

static uint32_t lastHoldHintMs = 0;

// Выключение устройства. Выключателя питания на плате нет, а держать рацию
// включённой круглые сутки никто не хочет: батарея садится за ночь. Глубокий
// сон — единственное, что здесь можно назвать выключением: ток падает до
// микроампер, а пробуждение по той же кнопке равносильно подаче питания,
// потому что выход из deep sleep на ESP32 это полный сброс.
static void powerOff() {
  LOG_D("[Power] Выключение по кнопке");
  oledShowBig("SLEEP", "press button to wake");
  delay(1500);

  // Гасим всё, что ест ток: объявления в эфире, радио с усилителем, экран
  // вместе с внешним питанием.
  bleStopAdvertising();
  loraSleepForPowerOff();
  digitalWrite(PIN_LED, LOW);
  oledOff();

  // Кнопку нужно дождаться отпущенной, иначе устройство проснётся тем же
  // нажатием, которым его выключили, и человек увидит, что «не выключается».
  while (digitalRead(PIN_USER_BTN) == LOW) delay(10);
  delay(50);   // дребезг контактов

  // Кнопка подтянута к питанию и замыкает на землю, поэтому будим по низкому
  // уровню. Механизм — ext0: у ESP32-S3 нет esp_deep_sleep_enable_gpio_wakeup
  // (SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP не объявлен), а ext0 поддерживается и
  // работает с любым RTC-выводом, каким GPIO0 и является. Подтяжку удерживаем
  // во сне — иначе вход повиснет в воздухе и устройство проснётся от наводки.
  rtc_gpio_pullup_en((gpio_num_t)PIN_USER_BTN);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_USER_BTN);
  esp_err_t werr = esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_USER_BTN, 0);
  if (werr != ESP_OK) {
    // Уснуть без будильника — значит превратить рацию в кирпич до отключения
    // питания. Лучше не засыпать вовсе и честно сказать об этом на экране.
    LOG_F("[Power] пробуждение по кнопке недоступно: 0x%x\n", werr);
    oledShowBig("ERROR", "sleep unavailable");
    delay(2000);
    esp_restart();
  }
  esp_deep_sleep_start();
}

static void handleUserButton() {
  bool pressed = (digitalRead(PIN_USER_BTN) == LOW);

  if (pressed && !userBtnPressed) {
    userBtnPressTime = millis();
    userBtnPressed = true;
  }

  if (pressed && userBtnPressed) {
    uint32_t held = millis() - userBtnPressTime;
    uint32_t limit = repeaterIsEnabled() ? POWER_OFF_HOLD_REPEATER_MS
                                         : POWER_OFF_HOLD_MS;
    // Обратный отсчёт на экране. Без него человек держит кнопку вслепую и
    // отпускает на второй секунде, решив, что ничего не происходит.
    if (held > 1200 && held < limit && millis() - lastHoldHintMs > 250) {
      lastHoldHintMs = millis();
      char hint[20];
      snprintf(hint, sizeof(hint), "off in %lu...",
               (unsigned long)((limit - held + 999) / 1000));
      oledWake();
      oledShowMessage("HOLD BUTTON", hint, 400);
    }
  }

  if (!pressed && userBtnPressed) {
    uint32_t held = millis() - userBtnPressTime;
    userBtnPressed = false;

    if (repeaterIsEnabled()) {
      if (held >= POWER_OFF_HOLD_REPEATER_MS) {
        powerOff();
      } else if (held > 3000) {
        oledWake();
        oledShowMessage("NORMAL MODE", "Restarting...", 1000);
        delay(1000);
        repeaterSetEnabled(false);
        esp_restart();
      } else if (held > 1000) {
        repeaterResetStats();
        oledShowMessage("STATS CLEARED", "", 1000);
      } else {
        // Короткое нажатие — включить экран
        oledWake();
      }
    } else {
      if (held >= POWER_OFF_HOLD_MS) {
        powerOff();
      } else if (held > 1000) {
        // Длинное нажатие (>1с) — показать PIN
        oledWake();
        char pinBuf[16];
        snprintf(pinBuf, sizeof(pinBuf), "PIN: %04lu", (unsigned long)bleGetPin());
        oledShowMessage(pinBuf, bleGetDeviceName().c_str(), 10000);
      } else {
        // Короткое нажатие — только включить экран
        oledWake();
      }
    }
  }
}

// ================================================================
// loraTask — приём и отправка LoRa
// ================================================================
static void loraTaskFunc(void* param) {
  LOG_D("[LoRa Task] Started on Core 0 (event-driven)");

  uint8_t rxBuf[222];
  LoRaAudioPacket txAudioPkt;
  LoRaTextPacket txTextPkt;

  while (true) {
    // BLE не подключён и не ретранслятор — радио в standby, устройство остаётся
    // доступным для подключения (advertising активен, ребутов нет).
    // В тестовом режиме радио держим в приёме — стенд работает без телефона.
    bool testActive = false;
#ifdef TEST_CONSOLE
    testActive = testConsoleIsActive();
#endif
    if (!bleConnected && !repeaterIsEnabled() && !testActive) {
      // Раньше здесь был полный standby, и устройство без телефона не слышало
      // эфир вообще: ни вызова, ни сообщения, ни файла — сеть для него
      // переставала существовать до следующего подключения по BLE.
      // Duty cycle слушает периодически и просыпается на длинную преамбулу,
      // а стоит это около 1 мА разницы против standby.
      // После недавнего обмена держим постоянный приём: собеседник ждёт ACK
      // и продолжение, а переключение режима посреди диалога рвёт его.
      if (loraGetPowerMode() != LORA_POWER_DUTY_CYCLE_RX &&
          millis() - lastLoraActivityMs > LORA_IDLE_TIMEOUT_MS) {
        loraSetPowerMode(LORA_POWER_DUTY_CYCLE_RX);
        LOG_D("EVT LORA_IDLE_RX reason=no_ble");
      }
      // Приходящий пакет будит задачу через DIO1; таймаут — лишь страховка.
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
      if (!loraRxFlag) continue;
      // Пакет пришёл — дальше его читает общий код. Режим здесь НЕ меняем:
      // смена режима перезапускает приём и стирает ещё не прочитанный пакет.
    }

    // Были в standby (например, после ретранслятора) — вернуться в приём
    if (loraGetPowerMode() == LORA_POWER_SLEEP) {
      lastLoraActivityMs = millis();
      loraSetPowerMode(LORA_POWER_CONTINUOUS_RX);
    }

    // Флаг для beaconTask: радио занято потоковой передачей
    loraAppBusy = pttActive || (fileState != FILE_STATE_IDLE);

    // Idle → duty cycle RX. Требуем явно включённой настройки: режим ловит
    // только пакеты с длинной преамбулой, и любой отправитель, забывший её
    // поставить, становится не слышен. Экономия — около 3,5 мА радиочасти
    // на фоне 45 мА контроллера, так что цена ошибки заметно выше выигрыша.
    if (loraIsDutyCycleEnabled() &&
        loraGetPowerMode() == LORA_POWER_CONTINUOUS_RX && !testActive &&
        !pttActive && fileState == FILE_STATE_IDLE &&
        millis() - lastLoraActivityMs > LORA_IDLE_TIMEOUT_MS) {
      loraSetPowerMode(LORA_POWER_DUTY_CYCLE_RX);
    }

    // Ждём: DIO1 interrupt (RX done), TX queue, или таймаут
    // PTT active → 5мс (быстрый TX), idle → 500мс (экономия CPU)
    uint32_t waitMs = pttActive ? 5 : 500;
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(waitMs));

    // fileSendTask просит перезапустить RX (после loraSend с Core 1)
    if (loraNeedRxRestart) {
      loraNeedRxRestart = false;
      loraStartReceive();
      LOG_D("[LoRa] RX restarted by loraTask");
    }

    // Проверить входящий LoRа пакет
    if (loraRxFlag) {
      loraRxFlag = false;
      lastLoraActivityMs = millis();

      // Сначала забрать пакет из чипа и только потом менять режим:
      // startReceive() внутри смены режима сбрасывает буфер приёма, и пакет,
      // ради которого мы проснулись, терялся, не дойдя до разбора.
      int len = radio.getPacketLength();
      if (len > 0 && len <= (int)sizeof(rxBuf)) {
        int state = radio.readData(rxBuf, len);
        if (state == RADIOLIB_ERR_NONE) {
          lastRssi = loraGetRSSI();
          lastSnr = loraGetSNR();
          processLoRaPacket(rxBuf, len, lastRssi, lastSnr);
        }
      }
      // Пакет разобран — теперь можно вернуться в постоянный приём:
      // разговор начался, и дальше собеседник шлёт короткие преамбулы.
      if (loraGetPowerMode() != LORA_POWER_CONTINUOUS_RX) {
        loraSetPowerMode(LORA_POWER_CONTINUOUS_RX);
      } else {
        loraStartReceive();
      }
    }

    // Отправка аудио из очереди — приоритет над текстом.
    // Раньше очередь читалась только при pttActive, поэтому пакеты, попавшие
    // в неё после снятия PTT, оставались там навсегда.
    if (xQueueReceive(txAudioQueue, &txAudioPkt, 0) == pdTRUE) {
      // Первый кадр серии шлём длинной преамбулой: приёмник в duty cycle
      // просыпается редко и короткую преамбулу просто не слышит. Дальше
      // он уже в постоянном приёме, и будить его каждым кадром незачем.
      bool wake = (millis() - lastLoraActivityMs) > LORA_IDLE_TIMEOUT_MS;
      lastLoraActivityMs = millis();
      if (wake) loraSendWake((uint8_t*)&txAudioPkt, sizeof(txAudioPkt));
      else      loraSend((uint8_t*)&txAudioPkt, sizeof(txAudioPkt));
      loraStartReceive();
    }
    // Текст — когда аудио-очередь пуста и не идёт файловая сессия.
    // Голос (realtime) не откладываем, а текст подождёт: иначе встречный чат
    // рвёт приём файла — половина чанков теряется на коллизиях.
    else if (fileState == FILE_STATE_IDLE &&
             xQueueReceive(txTextQueue, &txTextPkt, 0) == pdTRUE) {
      lastLoraActivityMs = millis();
      size_t textLen = strlen((char*)txTextPkt.text);
      size_t pktLen = 8 + textLen + 1; // header(8) + text + null
      // Текст — всегда будящий пакет: одиночное сообщение приходит в тишине,
      // когда приёмник почти наверняка в duty cycle. Лишние 24 символа
      // преамбулы дешевле потерянного сообщения.
      loraSendWake((uint8_t*)&txTextPkt, pktLen);
      loraStartReceive();
    }

  }
}

// ================================================================
// Обработка принятого LoRa пакета
// ================================================================
static void processLoRaPacket(uint8_t* data, int len, int16_t rssi, int8_t snr) {
  if (len < 1) return;
#ifdef TEST_CONSOLE
  if (testConsoleShouldDrop(data[0])) return;   // LOSS: эмуляция потери в канале
  testConsoleOnLoRaRx(data, len, rssi, snr);
#else
  LOG_F("[LoRa RX] type=0x%02X len=%d rssi=%d\n", data[0], len, rssi);
#endif
  uint8_t pktType = data[0];

  // LED на 500мс при любом принятом пакете
  rxLedUntil = millis() + 500;

  switch (pktType) {
    case PKT_TYPE_AUDIO: {
      if (len < (int)sizeof(LoRaAudioPacket)) break;
      LoRaAudioPacket* pkt = (LoRaAudioPacket*)data;
      if (pkt->channel != currentChannel) break;
      // Свой же голос, вернувшийся от ретранслятора: слышать себя с задержкой
      // в полсекунды — худшее, что можно сделать с рацией.
      if (pkt->sender[0] == senderMac[0] && pkt->sender[1] == senderMac[1]) break;

      // Отправить на телефон через BLE: cmd + flags + sender[2] + payload
      uint8_t bleData[4 + CODEC2_PKT_BYTES];
      bleData[0] = BLE_CMD_AUDIO_RX;
      bleData[1] = pkt->flags; // PKT_FLAG_PTT_END и др.
      bleData[2] = pkt->sender[0];
      bleData[3] = pkt->sender[1];
      memcpy(bleData + 4, pkt->payload, CODEC2_PKT_BYTES);
      bleSendNotify(bleData, 4 + CODEC2_PKT_BYTES);

      // OLED здесь НЕ трогаем: перерисовка кадра по I2C занимает единицы
      // миллисекунд, а голос идёт 12 пакетов в секунду — задача приёма
      // не успевала вернуться в эфир и теряла до половины потока.
      // Экран обновляет bleTask раз в 500-1000 мс, и только когда он включён.
      break;
    }

    case PKT_TYPE_TEXT: {
      if (len < 8) break; // минимум: type+ch+seq+ttl+sender[2]+dest[2]
      LoRaTextPacket* pkt = (LoRaTextPacket*)data;
      if (pkt->channel != currentChannel) break;

      // Проверить адресат: broadcast (0x0000) или наш MAC
      {
        uint16_t d = pkt->dest[0] | (pkt->dest[1] << 8);
        uint16_t me = senderMac[0] | (senderMac[1] << 8);
        if (d != 0x0000 && d != me) break; // не нам
      }

      // Своё же сообщение, вернувшееся от ретранслятора. Дедупликация тут не
      // спасает: в её кеш попадает принятое, а собственное мы отправляли, а не
      // принимали. Для человека это выглядело как дубль в общем чате — ровно
      // это и заметили в группе, когда подняли третью ноду ретранслятором.
      if (pkt->sender[0] == senderMac[0] && pkt->sender[1] == senderMac[1]) break;

      // Дедупликация (для broadcast repeat)
      if (textIsDuplicate(pkt->sender, pkt->seq)) break;

      // Отправить на телефон: 0x08 + RSSI + текст + \0 + sender_id
      size_t textLen = strnlen((char*)pkt->text, 85);
      uint8_t bleData[1 + 1 + 85 + 1 + 2];
      bleData[0] = BLE_CMD_RECV_MESSAGE;
      bleData[1] = (uint8_t)(rssi & 0xFF);
      memcpy(bleData + 2, pkt->text, textLen);
      bleData[2 + textLen] = 0;
      memcpy(bleData + 2 + textLen + 1, pkt->sender, 2);
      bleSendNotify(bleData, 2 + textLen + 1 + 2);

      // OLED: показать первые 16 символов
      char msgPreview[22];
      snprintf(msgPreview, sizeof(msgPreview), "MSG: %.16s", pkt->text);
      oledShowMessage(msgPreview, "", 3000);

      // BLE ACK (локально на телефон)
      uint8_t ackBle[2] = {BLE_CMD_MESSAGE_ACK, pkt->seq};
      bleSendNotify(ackBle, 2);

      // LoRa ACK для адресных сообщений
      {
        uint16_t d = pkt->dest[0] | (pkt->dest[1] << 8);
        if (d != 0x0000) {
          // Turnaround: отправитель ещё переключается с передачи на приём
          vTaskDelay(pdMS_TO_TICKS(30));
          LoRaTextAck ack;
          ack.type = PKT_TYPE_TEXT_ACK;
          ack.channel = currentChannel;
          ack.seq = pkt->seq;
          memcpy(ack.sender, senderMac, 2);
          memcpy(ack.dest, pkt->sender, 2); // обратно отправителю
          bool ok = loraSend((uint8_t*)&ack, sizeof(ack));
          loraStartReceive();
          LOG_F("EVT TEXT_ACK_TX seq=%d ok=%d\n", pkt->seq, ok ? 1 : 0);
        }
      }
      break;
    }

    case PKT_TYPE_TEXT_ACK: {
      if (len < (int)sizeof(LoRaTextAck)) break;
      LoRaTextAck* pkt = (LoRaTextAck*)data;
      if (pkt->channel != currentChannel) break;
      // Проверить что ACK для нас
      uint16_t d = pkt->dest[0] | (pkt->dest[1] << 8);
      uint16_t me = senderMac[0] | (senderMac[1] << 8);
      if (d != me) break;
      // Уведомить телефон: MESSAGE_ACK с seq
      uint8_t ackBle[2] = {BLE_CMD_MESSAGE_ACK, pkt->seq};
      bleSendNotify(ackBle, 2);
      LOG_F("[Text] ACK received for seq %d\n", pkt->seq);
#ifdef TEST_CONSOLE
      Serial.printf("EVT TEXT_ACK_RX seq=%d rssi=%d\n", pkt->seq, rssi);
#endif
      break;
    }

    case PKT_TYPE_FILE_START: {
      if (len < (int)sizeof(LoRaFileHeader)) break;
      LoRaFileHeader* pkt = (LoRaFileHeader*)data;
      if (pkt->channel != currentChannel) break;
      // Проверить адресат: только наш MAC (broadcast не поддерживается для файлов)
      {
        uint16_t d = pkt->dest[0] | (pkt->dest[1] << 8);
        uint16_t me = senderMac[0] | (senderMac[1] << 8);
        if (d != me) break; // не нам
      }

      // Инициировать приём файла
      fileRxChunksTotal = pkt->total_chunks;
      fileRxSize = pkt->total_size;
      fileRxType = pkt->file_type;
      fileRxChunksDone = 0;
      fileSessionId = pkt->session_id;
      strncpy(fileRxName, (char*)pkt->name, 19);
      fileRxSender[0] = pkt->sender[0];
      fileRxSender[1] = pkt->sender[1];
      if (fileState != FILE_STATE_IDLE) {
        LOG_D("[File] RX ignored — busy");
        break;
      }
      fileState = FILE_STATE_RECEIVING;
      loraAppBusy = true;
      fileRxLastChunkMs = millis();
      bitmap_clear(fileRxBitmap, sizeof(fileRxBitmap));
      fileRxUniqueCount = 0;

      // Аллоцировать буфер
      if (fileRxBuffer) free(fileRxBuffer);
      fileRxBuffer = (uint8_t*)calloc(fileRxSize, 1); // обнулить
      if (!fileRxBuffer) {
        LOG_D("[File] malloc failed!");
        fileState = FILE_STATE_IDLE;
      }
      LOG_F("[File] RX start: %s (%d bytes, %d chunks)\n",
        fileRxName, fileRxSize, fileRxChunksTotal);
#ifdef TEST_CONSOLE
      Serial.printf("EVT FILE_RX_START session=%d size=%lu chunks=%d buf=%d\n",
        fileSessionId, (unsigned long)fileRxSize, fileRxChunksTotal,
        fileRxBuffer ? 1 : 0);
#endif
      break;
    }

    case PKT_TYPE_FILE_CHUNK: {
      if (len < 8 || !(fileState == FILE_STATE_RECEIVING)) break;
      LoRaFileChunk* pkt = (LoRaFileChunk*)data;
      if (pkt->session_id != fileSessionId) break;
      // Проверить адресат (только адресные)
      {
        uint16_t d = pkt->dest[0] | (pkt->dest[1] << 8);
        uint16_t me = senderMac[0] | (senderMac[1] << 8);
        if (d != me) break;
      }

      uint16_t idx = pkt->chunk_index;
      size_t dataLen = len - 8;
      if (dataLen > CHUNK_SIZE) dataLen = CHUNK_SIZE;

      uint32_t offset = (uint32_t)idx * CHUNK_SIZE;
      if (fileRxBuffer && offset + dataLen <= fileRxSize) {
        memcpy(fileRxBuffer + offset, pkt->data, dataLen);
      }
      fileRxLastChunkMs = millis();
      loraStartReceive();

      // Считать только уникальные чанки
      if (idx < 1024 && bitmap_set(fileRxBitmap, idx)) {
        fileRxUniqueCount++;
        fileRxChunksDone = fileRxUniqueCount;
      }

      // Прогресс каждые 5 чанков
      if (fileRxUniqueCount % 5 == 0) {
        uint8_t progress[6];
        progress[0] = BLE_CMD_FILE_PROGRESS;
        progress[1] = fileSessionId;
        progress[2] = fileRxUniqueCount & 0xFF;
        progress[3] = fileRxUniqueCount >> 8;
        progress[4] = fileRxChunksTotal & 0xFF;
        progress[5] = fileRxChunksTotal >> 8;
        bleSendNotify(progress, 6);
      }

      // Авто-завершение: пометить как complete, ACK отправится по FILE_END
      if (fileRxUniqueCount >= fileRxChunksTotal && fileRxBuffer) {
        LOG_F("[File] RX all chunks: %s (%d/%d) — waiting for FILE_END\n",
          fileRxName, fileRxUniqueCount, fileRxChunksTotal);
        // НЕ отправляем ACK здесь — ждём FILE_END (отправитель ещё может слать)
      }
      break;
    }

    case PKT_TYPE_FILE_END: {
      if (len < (int)sizeof(LoRaFileEnd)) break;
      LoRaFileEnd* pkt = (LoRaFileEnd*)data;
#ifdef TEST_CONSOLE
      Serial.printf("EVT FILE_END_RX session=%d my_session=%d state=%d "
                    "got=%d/%d cached=%d\n",
        pkt->session_id, fileSessionId, (int)fileState,
        fileRxUniqueCount, fileRxChunksTotal, lastFileRespLen);
#endif

      // Повторный FILE_END — переотправить кешированный ответ (ACK или NACK)
      if (pkt->session_id == lastFileRespSessionId &&
          lastFileRespLen > 0 &&
          millis() - lastFileRespTimeMs < FILE_RESP_CACHE_TTL_MS) {
        if (fileState != FILE_STATE_RECEIVING) {
          LOG_F("[File] Resending cached response for session %d (%d bytes)\n",
            pkt->session_id, lastFileRespLen);
          loraSendWake(lastFileRespData, lastFileRespLen);
          loraStartReceive();
          break;
        }
      }

      if (!(fileState == FILE_STATE_RECEIVING)) break;
      if (pkt->session_id != fileSessionId) break;

      // Turnaround: отправитель переключается с передачи на приём — дать ему время
      vTaskDelay(pdMS_TO_TICKS(30));

      // Проверить что все чанки получены
      if (fileRxUniqueCount >= fileRxChunksTotal) {
        // Всё получено — ACK
        LOG_F("[File] RX complete via FILE_END: %s (%d bytes)\n", fileRxName, fileRxSize);
        fileState = FILE_STATE_IDLE;
        fileRxComplete = true;
        // ACK
        LoRaFileAck ack;
        memset(&ack, 0, sizeof(ack));
        ack.type = PKT_TYPE_FILE_ACK;
        ack.session_id = fileSessionId;
        ack.status = 0x00;
        memcpy(ack.dest, fileRxSender, 2);
        ack.missing_count = 0;
        // Кешировать для повторной отправки
        lastFileRespSessionId = fileSessionId;
        lastFileRespTimeMs = millis();
        lastFileRespLen = 7;
        memcpy(lastFileRespData, &ack, 7);
        bool respSent = loraSendWake((uint8_t*)&ack, 7);
        LOG_F("EVT FILE_RESP kind=ACK len=7 ok=%d t=%lu\n",
          respSent ? 1 : 0, (unsigned long)millis());
        loraStartReceive();
#ifdef TEST_CONSOLE
        testConsoleOnFileRxDone(fileRxName, fileRxSize, fileRxChunksTotal,
                                fileRxUniqueCount, fileRxBuffer);
#endif
      } else {
        // Есть пропуски — NACK с индексами пропущенных
        LoRaFileAck nack;
        memset(&nack, 0, sizeof(nack));
        nack.type = PKT_TYPE_FILE_ACK;
        nack.session_id = fileSessionId;
        nack.status = 0x01; // NACK
        memcpy(nack.dest, fileRxSender, 2);
        uint16_t cnt = bitmap_find_missing(fileRxBitmap, fileRxChunksTotal, nack.missing, 50);
        nack.missing_count = cnt;
        size_t nackLen = 7 + cnt * 2; // header + missing indices
        // Кешировать для повторной отправки
        lastFileRespSessionId = fileSessionId;
        lastFileRespTimeMs = millis();
        lastFileRespLen = nackLen;
        memcpy(lastFileRespData, &nack, nackLen);
        bool respSent = loraSendWake((uint8_t*)&nack, nackLen);
        LOG_F("EVT FILE_RESP kind=NACK len=%d missing=%d ok=%d t=%lu\n",
          (int)nackLen, cnt, respSent ? 1 : 0, (unsigned long)millis());
        loraStartReceive();
        // Не завершаем — ждём досылку
        fileRxLastChunkMs = millis(); // сбросить таймаут
      }
      break;
    }

    case PKT_TYPE_FILE_ACK: {
      // Входящий ACK/NACK для наших отправленных файлов
      if (len < 7) break;
      LoRaFileAck* pkt = (LoRaFileAck*)data;
      uint16_t d = pkt->dest[0] | (pkt->dest[1] << 8);
      uint16_t me = senderMac[0] | (senderMac[1] << 8);
      LOG_F("[File] ACK recv: dest=%04X me=%04X status=%d state=%d\n", d, me, pkt->status, fileState);
      if (d != me) break;

      if (fileState == FILE_STATE_SENDING) {
        // File v2: ACK/NACK обрабатывается в fileSendTask
        fileTxAckStatus = pkt->status;
        if (pkt->status == 0x01) {
          // NACK — скопировать missing
          fileTxMissingCount = pkt->missing_count;
          if (fileTxMissingCount > 50) fileTxMissingCount = 50;
          for (uint16_t i = 0; i < fileTxMissingCount; i++) {
            fileTxMissing[i] = pkt->missing[i];
          }
        }
        fileTxAckReceived = true;
        LOG_F("[File v2] ACK/NACK status=%d missing=%d\n", pkt->status, pkt->missing_count);
      } else {
        // Старый протокол (fallback)
        if (pkt->status == 0x00) {
          LOG_D("[File] TX ACK received");
          uint8_t progress[6];
          progress[0] = BLE_CMD_FILE_PROGRESS;
          progress[1] = pkt->session_id;
          progress[2] = 0xFF; progress[3] = 0xFF;
          progress[4] = 0xFF; progress[5] = 0xFF;
          bleSendNotify(progress, 6);
        }
      }
      break;
    }

    case PKT_TYPE_BEACON: {
      if (len < (int)sizeof(LoRaBeaconPacket)) break;
      LoRaBeaconPacket* pkt = (LoRaBeaconPacket*)data;
      beaconProcessIncoming(pkt, rssi, snr);
      // Сосед вышел на связь и просит отозваться — иначе он увидит нас
      // только через несколько минут, когда подойдёт наш очередной маяк.
      if (pkt->flags & BEACON_FLAG_REQUEST) beaconScheduleReply();
      break;
    }

    // === Вызовы ===
    case PKT_TYPE_CALL_ALL: {
      if (len < (int)sizeof(LoRaCallAll)) break;
      LoRaCallAll* pkt = (LoRaCallAll*)data;
      if (pkt->channel != currentChannel) break;
      callProcessAllCall(pkt, rssi, pkt->ttl);
      break;
    }
    case PKT_TYPE_CALL_PRIVATE: {
      if (len < (int)sizeof(LoRaCallPrivate)) break;
      LoRaCallPrivate* pkt = (LoRaCallPrivate*)data;
      if (pkt->channel != currentChannel) break;
      callProcessPrivateCall(pkt, rssi, pkt->ttl);
      break;
    }
    case PKT_TYPE_CALL_GROUP: {
      if (len >= (int)sizeof(LoRaCallGroup)) {
        LoRaCallGroup* pkt = (LoRaCallGroup*)data;
        if (pkt->channel == currentChannel)
          callProcessGroupCall(pkt, rssi, pkt->ttl);
      }
      break;
    }
    case PKT_TYPE_CALL_EMERGENCY: {
      if (len < (int)sizeof(LoRaCallEmergency)) break;
      LoRaCallEmergency* pkt = (LoRaCallEmergency*)data;
      callProcessEmergency(pkt, rssi, pkt->ttl);
      break;
    }
    case PKT_TYPE_CALL_ACCEPT:
    case PKT_TYPE_CALL_REJECT:
    case PKT_TYPE_CALL_CANCEL: {
      if (len < (int)sizeof(LoRaCallResponse)) break;
      LoRaCallResponse* pkt = (LoRaCallResponse*)data;
      callProcessResponse(pkt);
      break;
    }

    default:
      LOG_F("[LoRa] Unknown packet type: 0x%02X\n", pktType);
      break;
  }
}

// ================================================================
// bleTask — статус каждые 500мс
// ================================================================
static uint32_t ledBlinkTimer = 0;
static bool ledState = false;

// Включение передачи. Обязательно через эту функцию: флаг и отметка времени
// должны выставляться вместе, иначе предохранитель считает от чужого момента
// и глушит передачу в первую же секунду.
static void pttStart() {
  pttActive = true;
  pttStartedMs = millis();
}

// Завершение передачи: общий путь для отпущенной кнопки и для предохранителя
// по времени. Слушателям уходит пакет с признаком конца — по нему телефон
// собеседника даёт отбой; телефону говорящего сообщаем тем же кодом, чтобы он
// снял кнопку, даже если сам об окончании не знает.
static void pttStop(bool byTimeout) {
  if (!pttActive) return;
  pttActive = false;

  LoRaAudioPacket pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = PKT_TYPE_AUDIO;
  pkt.channel = currentChannel;
  pkt.seq = audioSeqNum++;
  pkt.flags = PKT_FLAG_PTT_END;
  pkt.ttl = TTL_DEFAULT;
  memcpy(pkt.sender, senderMac, 2);
  loraSend((uint8_t*)&pkt, sizeof(pkt));
  loraStartReceive();

  if (byTimeout) {
    uint8_t note[2] = {BLE_CMD_PTT_END, 1};   // 1 — остановлено по времени
    bleSendNotify(note, 2);
    LOG_F("[PTT] Остановлено по лимиту %d с\n", PTT_MAX_SECONDS);
#ifdef TEST_CONSOLE
    Serial.printf("EVT PTT_LIMIT seconds=%d\n", PTT_MAX_SECONDS);
#endif
    oledWake();
    oledShowMessage("LIMIT 10s", "", 2000);
  }
}

static void bleTaskFunc(void* param) {
  LOG_D("[BLE Task] Started on Core 1");
  while (true) {
    // === Предохранитель длительности передачи ===
    // В полудуплексе говорящий занимает канал целиком: пока он не отпустит
    // кнопку, остальные не могут ни ответить, ни позвать на помощь. Залипшая
    // кнопка или зависшее приложение без этого затыкали бы сеть насовсем.
    if (pttActive && millis() - pttStartedMs > (uint32_t)PTT_MAX_SECONDS * 1000) {
      pttStop(true);
    }

    // === Сторож объявлений ===
    // Объявления должны идти всегда, пока телефон не подключён: иначе устройство
    // для пользователя просто исчезает. NimBLE обещает возобновлять их сам после
    // разрыва, но обещание — не гарантия, а цена сбоя здесь слишком велика.
    static uint32_t advCheckMs = 0;
#ifndef DISABLE_ADV_WATCHDOG
    if (millis() - advCheckMs > 3000) {
      advCheckMs = millis();
      bleEnsureAdvertising();
    }
#endif

    // === Событие подключения телефона — OLED вне контекста BLE-колбэка ===
    if (bleConnEvent) {
      bleConnEvent = false;
      oledWake();
      oledShowMessage("BLE CONNECTED", "", 3000);
      // Телефон только что подключился, и первое, что видит человек, — пустой
      // список абонентов. Просим соседей отозваться, чтобы он наполнился за
      // секунды, а не за интервал маяка.
      beaconRequestPeers();
    }

    // === Авто-сброс fileTxActive ===
    if (fileTxActive && millis() > fileTxLedUntil) {
      fileTxActive = false;
    }
    // === Таймаут приёма ===
    // Сессия, в которую не пришло ни одного чанка, закрывается быстро: в общем
    // эфире достаточно одного случайного или чужого FILE_START, чтобы занять
    // приёмник на минуту и заставить его отказывать настоящим отправителям.
    uint32_t rxTimeoutMs = (fileRxUniqueCount == 0)
                             ? FILE_RX_FIRST_CHUNK_TIMEOUT_MS
                             : (uint32_t)fileTimeoutSec * 1000;
    if ((fileState == FILE_STATE_RECEIVING) && millis() - fileRxLastChunkMs > rxTimeoutMs) {
      LOG_F("[File] RX timeout: %d/%d unique chunks (lost %d)\n",
        fileRxUniqueCount, fileRxChunksTotal, fileRxChunksTotal - fileRxUniqueCount);
      // Отправить что есть если получено хотя бы 90%
      if (fileRxBuffer && fileRxUniqueCount >= fileRxChunksTotal * 9 / 10) {
        LOG_D("[File] >90% received, sending partial");
        fileRxComplete = true;
      } else {
        if (fileRxBuffer) { free(fileRxBuffer); fileRxBuffer = nullptr; }
      }
      fileState = FILE_STATE_IDLE;
    }

    // === LED индикация ===
    if (pttActive) {
      // TX голос: LED горит постоянно
      digitalWrite(PIN_LED, HIGH);
      ledState = true;
    } else if (fileTxActive || (fileState == FILE_STATE_RECEIVING)) {
      // Передача файла: быстрое мигание 100мс
      if (millis() - ledBlinkTimer > 100) {
        ledState = !ledState;
        digitalWrite(PIN_LED, ledState ? HIGH : LOW);
        ledBlinkTimer = millis();
      }
    } else if (millis() < rxLedUntil) {
      // RX: LED горит пока принимаем данные
      digitalWrite(PIN_LED, HIGH);
      ledState = true;
    } else if (bleIsConnected()) {
      // BLE подключён: короткая вспышка каждые 30 сек (было 5)
      if (!ledState && millis() - ledBlinkTimer > 30000) {
        digitalWrite(PIN_LED, HIGH);
        ledState = true;
        ledBlinkTimer = millis();
      } else if (ledState && millis() - ledBlinkTimer > 70) {
        digitalWrite(PIN_LED, LOW);
        ledState = false;
        ledBlinkTimer = millis();
      }
    } else {
      // Нет BLE: LED выключен (мигнёт только при RX через rxLedUntil)
      digitalWrite(PIN_LED, LOW);
      ledState = false;
    }

    // === Обновить OLED (только когда экран включён) ===
    if (oledIsAwake()) {
      oledShowMain(currentChannel, loraGetFrequency(currentChannel),
                   lastRssi, lastSnr, loraGetTxPower(), bleIsConnected(),
                   loraIsDutyCycleEnabled(), pttActive,
                   false, getCachedBattery());
    }

    // === Принятый файл некому отдать (нет телефона) — освободить RAM ===
    if (fileRxComplete && fileRxBuffer && !bleIsConnected()) {
      fileRxComplete = false;
      free(fileRxBuffer);
      fileRxBuffer = nullptr;
      LOG_D("EVT FILE_RX_DROP reason=no_phone");
    }

    // === Передать принятый файл на телефон ===
    if (fileRxComplete && fileRxBuffer && bleIsConnected()) {
      fileRxComplete = false;
      LOG_F("[File] Sending to phone: %d bytes via BLE\n", fileRxSize);

      // Заголовок: cmd(1)+type(1)+size(4)+chunks(1)+sender(2)+name(20)=29
      uint8_t hdr[9 + 20];
      hdr[0] = BLE_CMD_FILE_RECV;
      hdr[1] = fileRxType;
      hdr[2] = fileRxSize & 0xFF;
      hdr[3] = (fileRxSize >> 8) & 0xFF;
      hdr[4] = (fileRxSize >> 16) & 0xFF;
      hdr[5] = (fileRxSize >> 24) & 0xFF;
      hdr[6] = fileRxChunksTotal & 0xFF;
      hdr[7] = fileRxSender[0];
      hdr[8] = fileRxSender[1];
      memcpy(hdr + 9, fileRxName, 20);
      bleSendNotify(hdr, 29);
      vTaskDelay(pdMS_TO_TICKS(100));

      // Данные чанками
      size_t off = 0;
      while (off < fileRxSize) {
        size_t chunk = fileRxSize - off;
        if (chunk > 100) chunk = 100;
        uint8_t buf[101];
        buf[0] = BLE_CMD_FILE_DATA;
        memcpy(buf + 1, fileRxBuffer + off, chunk);
        bleSendNotify(buf, 1 + chunk);
        off += chunk;
        vTaskDelay(pdMS_TO_TICKS(100));
      }

      char oledBuf[22];
      snprintf(oledBuf, sizeof(oledBuf), "FILE: %s %dKB",
        fileRxName, (int)(fileRxSize / 1024));
      oledShowMessage(oledBuf, "", 3000);

      free(fileRxBuffer);
      fileRxBuffer = nullptr;
      LOG_D("[File] Sent to phone OK");
    }

    // === Pending upload status — дублировать из bleTask ===
    if (pendingUploadStatus != 0xFF && bleIsConnected()) {
      vTaskDelay(pdMS_TO_TICKS(200)); // дать BLE обработать
      uint8_t spkt[4];
      spkt[0] = BLE_CMD_FILE_UPLOAD_STATUS;
      spkt[1] = pendingUploadStatus;
      spkt[2] = pendingUploadSession;
      spkt[3] = 0;
      bleSendNotify(spkt, 4);
      LOG_F("[BLE] Resent UPLOAD_STATUS=%d session=%d\n", pendingUploadStatus, pendingUploadSession);
      pendingUploadStatus = 0xFF;
    }

    // === BLE статус — раз в 10 сек (было 2 сек) ===
    static uint32_t lastStatusMs = 0;
    uint32_t nowMs = millis();
    if (bleIsConnected() && nowMs - lastStatusMs >= 10000) {
      sendStatusUpdate();
      lastStatusMs = nowMs;
    }

    // Idle → 1 сек, active → 500мс (light sleep экономит между пробуждениями)
    bool isActive = pttActive || fileTxActive || (fileState == FILE_STATE_RECEIVING) || oledIsAwake();
    vTaskDelay(pdMS_TO_TICKS(isActive ? 500 : 1000));
  }
}

// ================================================================
// Отправка STATUS_UPDATE (0x06)
// ================================================================
static void sendStatusUpdate() {
  uint8_t data[6];
  data[0] = BLE_CMD_STATUS_UPDATE;
  data[1] = currentChannel;
  data[2] = (uint8_t)(lastRssi & 0xFF);
  data[3] = (uint8_t)((lastRssi >> 8) & 0xFF);
  data[4] = (uint8_t)lastSnr;
  // Батарея: вольты × 10 (напр. 3.85V → 38, 4.20V → 42), 0 = нет данных
  float batV = getCachedBattery();
  data[5] = (batV > 0.5f) ? (uint8_t)(batV * 10.0f) : 0;
  bleSendNotify(data, 6);
}

// ================================================================
// === File Transfer v2: утилиты ===
static void sendUploadStatus(uint8_t status, uint8_t sessionId = 0) {
  // Отправить сразу + сохранить для повторной отправки из bleTask
  uint8_t pkt[4];
  pkt[0] = BLE_CMD_FILE_UPLOAD_STATUS;
  pkt[1] = status;
  pkt[2] = sessionId;
  pkt[3] = 0;
  bleSendNotify(pkt, 4);
  // Для финальных статусов — дублировать через bleTask
  if (status == 3 || status == 4) {
    pendingUploadStatus = status;
    pendingUploadSession = sessionId;
  }
}

// === File Transfer v2: задача отправки из RAM по LoRa ===
static void fileSendTask(void* param) {
  LOG_D("[FileSend] Task started");
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // ждать загрузку файла
    if (!fileTxBuffer || fileTxSize == 0) continue;

    LOG_F("[FileSend] Starting: %s (%d bytes, %d chunks)\n", fileTxName, fileTxSize, fileTxChunksTotal);
    loraAppBusy = true;
    lastLoraActivityMs = millis();  // prevent duty cycle during file TX
    loraSetPowerMode(LORA_POWER_CONTINUOUS_RX);
    sendUploadStatus(2, fileTxSessionId); // SENDING
    fileTxNackRound = 0;
    uint32_t startMs = millis();

    // Отправить FILE_START (длинная преамбула)
    {
      LoRaFileHeader hdr;
      memset(&hdr, 0, sizeof(hdr));
      hdr.type = PKT_TYPE_FILE_START;
      hdr.channel = currentChannel;
      hdr.session_id = fileTxSessionId;
      hdr.ttl = TTL_DEFAULT;
      memcpy(hdr.sender, senderMac, 2);
      memcpy(hdr.dest, fileTxDest, 2);
      hdr.file_type = fileTxType;
      hdr.total_chunks = fileTxChunksTotal;
      hdr.total_size = fileTxSize;
      strncpy((char*)hdr.name, fileTxName, 19);
      loraSendWake((uint8_t*)&hdr, sizeof(hdr));
      loraStartReceive();
      vTaskDelay(pdMS_TO_TICKS(100)); // дать приёмнику подготовиться
    }

    // Отправить все чанки
    for (uint16_t i = 0; i < fileTxChunksTotal; i++) {
      if (millis() - startMs > (uint32_t)fileTimeoutSec * 1000) break; // таймаут
      LoRaFileChunk pkt;
      memset(&pkt, 0, sizeof(pkt));
      pkt.type = PKT_TYPE_FILE_CHUNK;
      pkt.channel = currentChannel;
      pkt.session_id = fileTxSessionId;
      pkt.ttl = TTL_DEFAULT;
      memcpy(pkt.dest, fileTxDest, 2);
      pkt.chunk_index = i;
      uint32_t offset = (uint32_t)i * CHUNK_SIZE;
      size_t dataLen = fileTxSize - offset;
      if (dataLen > CHUNK_SIZE) dataLen = CHUNK_SIZE;
      memcpy(pkt.data, fileTxBuffer + offset, dataLen);
      loraSend((uint8_t*)&pkt, 8 + dataLen);
      // loraTask перезапустит RX через loraNeedRxRestart
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Отправить FILE_END
    {
      LoRaFileEnd endPkt;
      memset(&endPkt, 0, sizeof(endPkt));
      endPkt.type = PKT_TYPE_FILE_END;
      endPkt.session_id = fileTxSessionId;
      endPkt.ttl = TTL_DEFAULT;
      loraSend((uint8_t*)&endPkt, sizeof(endPkt));
      // Немедленно в приём: доступ к радио потокобезопасен (mutex внутри),
      // а ответ приёмника приходит уже через ~20 мс — ждать loraTask нельзя.
      loraStartReceive();
      lastLoraActivityMs = millis();
      LOG_D("[FileSend] FILE_END sent, waiting for ACK...");
    }

    // Ждать ACK/NACK — макс 3 раунда NACK
    bool delivered = false;
    fileTxAckReceived = false;
    while (fileTxNackRound < MAX_NACK_ROUNDS && (millis() - startMs < (uint32_t)fileTimeoutSec * 1000)) {
      // Ждать ACK/NACK, повторяя FILE_END каждые 3 с: сам FILE_END мог
      // потеряться в коллизии, и тогда приёмник просто ждёт продолжения,
      // а отправитель — ответа, которого никто не пошлёт.
      for (int w = 0; w < 300 && !fileTxAckReceived; w++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (w % 30 == 29) {
          LoRaFileEnd rep;
          memset(&rep, 0, sizeof(rep));
          rep.type = PKT_TYPE_FILE_END;
          rep.session_id = fileTxSessionId;
          rep.ttl = TTL_DEFAULT;
          loraSend((uint8_t*)&rep, sizeof(rep));
          loraStartReceive();
          LOG_F("EVT FILE_END_REPEAT n=%d\n", (w + 1) / 30);
        }
      }
      if (!fileTxAckReceived) {
        LOG_D("[FileSend] ACK timeout");
        break;
      }
      if (fileTxAckStatus == 0x00) {
        // ACK — доставлено!
        delivered = true;
        LOG_D("[FileSend] ACK received — DELIVERED");
        break;
      }
      // NACK — досылка пропущенных
      LOG_F("[FileSend] NACK round %d: %d missing\n", fileTxNackRound + 1, fileTxMissingCount);
      for (uint16_t m = 0; m < fileTxMissingCount; m++) {
        uint16_t idx = fileTxMissing[m];
        if (idx >= fileTxChunksTotal) continue;
        LoRaFileChunk pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.type = PKT_TYPE_FILE_CHUNK;
        pkt.channel = currentChannel;
        pkt.session_id = fileTxSessionId;
        pkt.ttl = TTL_DEFAULT;
        memcpy(pkt.dest, fileTxDest, 2);
        pkt.chunk_index = idx;
        uint32_t offset = (uint32_t)idx * CHUNK_SIZE;
        size_t dataLen = fileTxSize - offset;
        if (dataLen > CHUNK_SIZE) dataLen = CHUNK_SIZE;
        memcpy(pkt.data, fileTxBuffer + offset, dataLen);
        loraSend((uint8_t*)&pkt, 8 + dataLen);
        vTaskDelay(pdMS_TO_TICKS(50));
      }
      // Повторный FILE_END
      {
        LoRaFileEnd endPkt;
        memset(&endPkt, 0, sizeof(endPkt));
        endPkt.type = PKT_TYPE_FILE_END;
        endPkt.session_id = fileTxSessionId;
        endPkt.ttl = TTL_DEFAULT;
        loraSend((uint8_t*)&endPkt, sizeof(endPkt));
        loraStartReceive();
      }
      fileTxNackRound++;
      fileTxAckReceived = false; // сбросить перед следующим раундом
    }

    // Результат
    sendUploadStatus(delivered ? 3 : 4, fileTxSessionId); // DELIVERED или FAILED
    LOG_F("[FileSend] %s (%d ms, %d NACK rounds)\n",
      delivered ? "DELIVERED" : "FAILED", millis() - startMs, fileTxNackRound);
#ifdef TEST_CONSOLE
    testConsoleOnFileTxDone(delivered, millis() - startMs, fileTxNackRound);
#endif

    // Освободить буфер
    free(fileTxBuffer);
    fileTxBuffer = nullptr;
    fileTxActive = false;
    fileState = FILE_STATE_IDLE;
    loraAppBusy = false;
  }
}

// ================================================================
// Обработка данных от BLE (телефон→ESP32)
// ================================================================
static void handleBleData(uint8_t* data, size_t len) {
  if (len < 1) return;
  uint8_t cmd = data[0];

  switch (cmd) {
    case BLE_CMD_AUDIO_TX: {
      // Аудио данные от телефона → в очередь TX
      if (len < 1 + CODEC2_PKT_BYTES) break;
      LoRaAudioPacket pkt;
      pkt.type = PKT_TYPE_AUDIO;
      pkt.channel = currentChannel;
      pkt.seq = audioSeqNum++;
      pkt.flags = 0;
      if (pttActive && audioSeqNum == 1) pkt.flags |= PKT_FLAG_PTT_START;
      pkt.ttl = TTL_DEFAULT;
      memcpy(pkt.sender, senderMac, 2);
      memcpy(pkt.payload, data + 1, CODEC2_PKT_BYTES);
      xQueueSend(txAudioQueue, &pkt, 0);
      break;
    }

    case BLE_CMD_PTT_START: {
      pttStart();
      audioSeqNum = 0;
      lastLoraActivityMs = millis();
      LOG_D("[BLE] PTT START");

      // Wake-пакет с длинной преамбулой — будит приёмники из duty cycle
      {
        LoRaAudioPacket wakePkt;
        memset(&wakePkt, 0, sizeof(wakePkt));
        wakePkt.type = PKT_TYPE_AUDIO;
        wakePkt.channel = currentChannel;
        wakePkt.seq = audioSeqNum++;
        wakePkt.flags = PKT_FLAG_PTT_START;
        wakePkt.ttl = TTL_DEFAULT;
        memcpy(wakePkt.sender, senderMac, 2);
        loraSendWake((uint8_t*)&wakePkt, sizeof(wakePkt));
        loraStartReceive();
      }
      break;
    }

    case BLE_CMD_PTT_END: {
      LOG_D("[BLE] PTT END");
      pttStop(false);
      break;
    }

    case BLE_CMD_SET_CHANNEL: {
      if (len < 2) break;
      uint8_t ch = data[1];
      if (ch < NUM_CHANNELS) {
        currentChannel = ch;
        loraSetChannel(ch);
        loraStartReceive();

        Preferences prefs;
        prefs.begin("settings", false);
        prefs.putUChar("channel", ch);
        prefs.end();

        oledShowMain(currentChannel, loraGetFrequency(currentChannel),
                     lastRssi, lastSnr, loraGetTxPower(), bleIsConnected(),
                     loraIsDutyCycleEnabled(), pttActive, false,
                     getCachedBattery());
      }
      break;
    }

    case BLE_CMD_SEND_MESSAGE: {
      // Формат: [0x07, seq, dest_lo, dest_hi, text...]
      if (len < 5) break;
      LoRaTextPacket pkt;
      memset(&pkt, 0, sizeof(pkt));
      pkt.type = PKT_TYPE_TEXT;
      pkt.channel = currentChannel;
      pkt.seq = data[1];
      pkt.ttl = TTL_DEFAULT;
      memcpy(pkt.sender, senderMac, 2);
      pkt.dest[0] = data[2];
      pkt.dest[1] = data[3];
      size_t textLen = len - 4;
      if (textLen > 84) textLen = 84;
      memcpy(pkt.text, data + 4, textLen);
      pkt.text[textLen] = 0;

      uint16_t destId = pkt.dest[0] | (pkt.dest[1] << 8);
      size_t pktLen = 8 + textLen + 1; // header(8) + text + null

      if (destId == 0x0000) {
        // Broadcast: отправить дважды с рандомной задержкой
        loraSend((uint8_t*)&pkt, pktLen);
        loraStartReceive();
        vTaskDelay(pdMS_TO_TICKS(100 + (esp_random() % 200))); // 100-300мс
        loraSend((uint8_t*)&pkt, pktLen);
        loraStartReceive();
      } else {
        // Адресный: отправить один раз (retry на стороне Android)
        loraSend((uint8_t*)&pkt, pktLen);
        loraStartReceive();
      }
      break;
    }

    case BLE_CMD_SET_SETTINGS: {
      // JSON строка с настройками
      if (len < 2) break;
      char jsonBuf[128];
      size_t jsonLen = len - 1;
      if (jsonLen >= sizeof(jsonBuf)) jsonLen = sizeof(jsonBuf) - 1;
      memcpy(jsonBuf, data + 1, jsonLen);
      jsonBuf[jsonLen] = 0;

      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, jsonBuf);
      if (err) {
        LOG_F("[Settings] JSON parse error: %s\n", err.c_str());
        break;
      }

      Preferences prefs;
      prefs.begin("settings", false);

      if (doc["duty_cycle"].is<bool>()) {
        bool dc = doc["duty_cycle"];
        loraSetDutyCycle(dc);
        prefs.putBool("duty_cycle", dc);
      }
      if (doc["tx_power"].is<int>()) {
        int8_t txp = doc["tx_power"];
        loraSetTxPower(txp);
        prefs.putChar("tx_power", txp);
      }
      if (doc["callsign"].is<const char*>()) {
        const char* cs = doc["callsign"];
        beaconSetCallSign(cs);
        LOG_F("[Settings] CallSign → %s\n", cs);
      }
      if (doc["file_timeout"].is<int>()) {
        uint16_t ft = doc["file_timeout"];
        if (ft >= 30 && ft <= 180) {
          fileTimeoutSec = ft;
          prefs.putUShort("file_timeout", ft);
          LOG_F("[Settings] File timeout → %d sec\n", ft);
        }
      }

      prefs.end();
      LOG_D("[Settings] Applied & saved");
      break;
    }

    case BLE_CMD_GET_SETTINGS: {
      // Отправить текущие настройки
      char jsonBuf[160];
      snprintf(jsonBuf, sizeof(jsonBuf),
        "{\"duty_cycle\":%s,\"tx_power\":%d,\"beacon_interval\":%d,\"callsign\":\"%s\",\"file_timeout\":%d}",
        loraIsDutyCycleEnabled() ? "true" : "false",
        loraGetTxPower(),
        (int)beaconGetInterval(),
        beaconGetCallSign(),
        (int)fileTimeoutSec);

      uint8_t resp[1 + 160];
      resp[0] = BLE_CMD_SETTINGS_RESP;
      size_t jl = strlen(jsonBuf);
      memcpy(resp + 1, jsonBuf, jl);
      bleSendNotify(resp, 1 + jl);
      break;
    }

    case BLE_CMD_FILE_START: {
      // Начало отправки файла от телефона
      if (len < (int)sizeof(LoRaFileHeader)) break;
      LoRaFileHeader* hdr = (LoRaFileHeader*)(data + 1);
      hdr->type = PKT_TYPE_FILE_START;
      memcpy(hdr->sender, senderMac, 2); // заполнить sender
      fileSessionId = hdr->session_id;    // сохранить для чанков!
      loraSendWake((uint8_t*)hdr, sizeof(LoRaFileHeader));  // длинная преамбула — будит получателя
      loraStartReceive();
      fileTxActive = true;
      fileTxLedUntil = millis() + 2000;
      LOG_F("[File] TX start: session=%d\n", fileSessionId);
      break;
    }

    case BLE_CMD_FILE_CHUNK: {
      if (len < 6) break;
      // data[1..2] = chunk_index, data[3..4] = dest, data[5..] = данные
      {
        LoRaFileChunk pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.type = PKT_TYPE_FILE_CHUNK;
        pkt.channel = currentChannel;
        pkt.session_id = fileSessionId;
        pkt.ttl = TTL_DEFAULT;
        pkt.dest[0] = data[3];
        pkt.dest[1] = data[4];
        pkt.chunk_index = data[1] | (data[2] << 8);
        size_t dataLen = len - 5;
        if (dataLen > CHUNK_SIZE) dataLen = CHUNK_SIZE;
        memcpy(pkt.data, data + 5, dataLen);
        loraSend((uint8_t*)&pkt, 8 + dataLen);
        loraStartReceive();
        fileTxLedUntil = millis() + 2000;
        vTaskDelay(pdMS_TO_TICKS(50)); // 50мс пауза между чанками
      }
      break;
    }

    case BLE_CMD_FILE_END: {
      // Отправить LoRa FILE_END
      LOG_F("[BLE] FILE_END received, len=%d\n", len);
      if (len >= 3) {
        LoRaFileEnd pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.type = PKT_TYPE_FILE_END;
        pkt.session_id = data[1];
        pkt.ttl = data[2];
        pkt.crc16 = 0; // TODO: реальный CRC
        loraSend((uint8_t*)&pkt, sizeof(pkt));
        loraStartReceive();
        LOG_F("[File] TX end: session=%d\n", pkt.session_id);
      }
      break;
    }

    // === File Transfer v2 ===
    case BLE_CMD_FILE_UPLOAD_START: {
      // Формат: cmd(1) + file_type(1) + dest(2) + size(4) + name(20) = 28 байт
      if (len < 28) break;
      if (fileState != FILE_STATE_IDLE) {
        sendUploadStatus(1); // BUSY
        LOG_D("[File v2] BUSY — отклонено");
        break;
      }
      uint32_t fsize = (data[4]) | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
      if (fsize > 200 * 1024 || ESP.getFreeHeap() < fsize + 30000) {
        sendUploadStatus(5); // NO_MEMORY
        LOG_F("[File v2] NO_MEMORY: size=%d, heap=%d\n", fsize, ESP.getFreeHeap());
        break;
      }
      fileTxBuffer = (uint8_t*)malloc(fsize);
      if (!fileTxBuffer) {
        sendUploadStatus(5);
        break;
      }
      fileTxSize = fsize;
      fileTxOffset = 0;
      fileTxType = data[1];
      fileTxDest[0] = data[2]; fileTxDest[1] = data[3];
      fileTxChunksTotal = (fsize + CHUNK_SIZE - 1) / CHUNK_SIZE;
      fileTxSessionId = (++fileSessionId) & 0xFF;
      memset(fileTxName, 0, 20);
      memcpy(fileTxName, data + 8, 20);
      fileState = FILE_STATE_UPLOADING;
      sendUploadStatus(0, fileTxSessionId); // ACCEPTED
      LOG_F("[File v2] ACCEPTED: %s (%d bytes, session=%d)\n", fileTxName, fsize, fileTxSessionId);
      break;
    }

    case BLE_CMD_FILE_UPLOAD_DATA: {
      if (fileState != FILE_STATE_UPLOADING || !fileTxBuffer) break;
      size_t dataLen = len - 1;
      if (fileTxOffset + dataLen > fileTxSize) dataLen = fileTxSize - fileTxOffset;
      memcpy(fileTxBuffer + fileTxOffset, data + 1, dataLen);
      fileTxOffset += dataLen;
      // Загрузка завершена?
      if (fileTxOffset >= fileTxSize) {
        LOG_F("[File v2] Upload complete: %d bytes → starting LoRa TX\n", fileTxOffset);
        fileState = FILE_STATE_SENDING;
        fileTxActive = true;
        fileTxLedUntil = millis() + (uint32_t)fileTimeoutSec * 1000;
        xTaskNotifyGive(fileSendTaskHandle); // запустить отправку
      }
      break;
    }

    case BLE_CMD_LOCATION_UPD: {
      if (len < 11) break;
      int32_t lat, lon;
      int16_t alt;
      memcpy(&lat, data + 1, 4);
      memcpy(&lon, data + 5, 4);
      memcpy(&alt, data + 9, 2);
      bool gpsValid = true;
      beaconUpdateLocation(lat, lon, alt, gpsValid);
      break;
    }

    // === Вызовы ===
    case BLE_CMD_CALL_ALL: {
      callSendAll(data + 1, len - 1);
      break;
    }
    case BLE_CMD_CALL_PRIVATE: {
      if (len < 5) break;
      callSendPrivate(data + 1, data + 5, len - 5);
      break;
    }
    case BLE_CMD_CALL_GROUP: {
      if (len < 2) break;
      uint8_t groupIdx = data[1];
      if (groupIdx == 0xFF && len >= 3) {
        uint8_t count = data[2];
        callSendGroup(groupIdx, data + 3, count);
      } else {
        callSendGroup(groupIdx, nullptr, 0);
      }
      break;
    }
    case BLE_CMD_CALL_EMERGENCY: {
      // Тревожный вызов убран из проекта: команда больше не выполняется.
      // Приём таких вызовов оставлен — в сети могут остаться устройства со
      // старой прошивкой, и их сигнал должен быть услышан.
      LOG_D("[Call] Тревожный вызов отключён — команда проигнорирована");
      break;
    }
    case BLE_CMD_CALL_ACCEPT: {
      if (len < 2) break;
      callAccept(data[1]);
      break;
    }
    case BLE_CMD_CALL_REJECT: {
      if (len < 2) break;
      callReject(data[1]);
      break;
    }
    case BLE_CMD_CALL_CANCEL: {
      callCancel();
      break;
    }

    case BLE_CMD_PIN_CHECK: {
      if (len < 5) break;
      uint32_t pin = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
      uint8_t result[2] = {BLE_CMD_PIN_RESULT, 0};
      if (pin == bleGetPin()) {
        result[1] = 1; // OK
        bleLinkAuthorized = true;
        LOG_D("[BLE] PIN OK");
        oledWake();
        oledShowMessage("PIN OK", "", 2000);
      } else {
        result[1] = 0; // FAIL
        LOG_F("[BLE] PIN FAIL: got %lu, expected %lu\n",
          (unsigned long)pin, (unsigned long)bleGetPin());
        oledWake();
        oledShowMessage("WRONG PIN", "", 2000);
      }
      bleSendNotify(result, 2);
      break;
    }

    case BLE_CMD_SCAN_PEERS: {
      // Кнопка «обновить» в приложении: рассылаем запрос присутствия, соседи
      // отзовутся своими маяками в пределах пары секунд.
      beaconRequestPeers();
      break;
    }

    case BLE_CMD_SET_REPEATER: {
      if (len < 2) break;
      bool enable = data[1] != 0;
      // Формат: [0x28, enable, ssid\0, pass\0, ip\0]
      if (len > 2) {
        Preferences prefs;
        prefs.begin("repeater", false);
        const char* ssid = (const char*)&data[2];
        size_t ssidLen = strnlen(ssid, len - 2);
        prefs.putString("wifi_ssid", String(ssid));
        size_t passOff = 2 + ssidLen + 1;
        if (passOff < len) {
          const char* pass = (const char*)&data[passOff];
          size_t passLen = strnlen(pass, len - passOff);
          prefs.putString("wifi_pass", String(pass));
          size_t ipOff = passOff + passLen + 1;
          if (ipOff < len) {
            const char* ip = (const char*)&data[ipOff];
            prefs.putString("static_ip", String(ip));
            LOG_F("[BLE] Static IP: %s\n", ip);
          }
        }
        prefs.end();
        LOG_F("[BLE] WiFi config saved: %s\n", ssid);
      }
      LOG_F("[BLE] Repeater: %s\n", enable ? "ON" : "OFF");
      oledWake();
      oledShowMessage(enable ? "REPEATER ON" : "REPEATER OFF", "Restarting...", 1000);
      delay(1000);
      repeaterSetEnabled(enable);
      esp_restart();
      break;
    }

    default:
      LOG_F("[BLE] Unknown cmd: 0x%02X\n", cmd);
      break;
  }
}


// ================================================================
// === Хуки тестовой консоли (только dev-сборки) ===
// ================================================================
#ifdef TEST_CONSOLE

uint8_t testHookCurrentChannel() { return currentChannel; }
uint32_t testHookBootCount() { return bootCount; }

bool testHookSendText(uint16_t dest, const char* text) {
  if (!txTextQueue) return false;
  LoRaTextPacket pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = PKT_TYPE_TEXT;
  pkt.channel = currentChannel;
  pkt.seq = textSeqNum++;
  pkt.ttl = TTL_DEFAULT;
  memcpy(pkt.sender, senderMac, 2);
  pkt.dest[0] = dest & 0xFF;
  pkt.dest[1] = (dest >> 8) & 0xFF;
  strncpy((char*)pkt.text, text, 84);
  pkt.text[84] = 0;
  return xQueueSend(txTextQueue, &pkt, pdMS_TO_TICKS(200)) == pdTRUE;
}

bool testHookPtt(bool on) {
  if (on) {
    pttStart();
    audioSeqNum = 0;
    lastLoraActivityMs = millis();
  } else {
    pttActive = false;
  }
  return true;
}

bool testHookSendAudio(uint16_t count, uint16_t gapMs) {
  if (!txAudioQueue) return false;
  bool wasPtt = pttActive;
  pttStart();                       // loraTask забирает из очереди только при PTT
  lastLoraActivityMs = millis();
  audioSeqNum = 0;
  bool ok = true;
  for (uint16_t i = 0; i < count; i++) {
    LoRaAudioPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = PKT_TYPE_AUDIO;
    pkt.channel = currentChannel;
    pkt.seq = audioSeqNum++;
    if (i == 0)               pkt.flags = PKT_FLAG_PTT_START;
    else if (i + 1 == count)  pkt.flags = PKT_FLAG_PTT_END;
    pkt.ttl = TTL_DEFAULT;
    memcpy(pkt.sender, senderMac, 2);
    for (int k = 0; k < CODEC2_PKT_BYTES; k++) pkt.payload[k] = (uint8_t)(i * 7 + k);
    if (xQueueSend(txAudioQueue, &pkt, pdMS_TO_TICKS(1000)) != pdTRUE) {
      Serial.printf("EVT TX_AUDIO_DROP seq=%u\n", pkt.seq);
      ok = false;
    }
    lastLoraActivityMs = millis();
    vTaskDelay(pdMS_TO_TICKS(gapMs));
  }
  // Дать очереди опустеть перед снятием PTT
  for (int w = 0; w < 50 && uxQueueMessagesWaiting(txAudioQueue) > 0; w++) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  pttActive = wasPtt;
  return ok;
}

bool testHookSendAudioOne(uint8_t flags) {
  if (!txAudioQueue) return false;
  LoRaAudioPacket pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = PKT_TYPE_AUDIO;
  pkt.channel = currentChannel;
  pkt.seq = audioSeqNum++;
  pkt.flags = flags;
  pkt.ttl = TTL_DEFAULT;
  memcpy(pkt.sender, senderMac, 2);
  for (int k = 0; k < CODEC2_PKT_BYTES; k++) pkt.payload[k] = (uint8_t)(pkt.seq + k);
  lastLoraActivityMs = millis();
  return xQueueSend(txAudioQueue, &pkt, pdMS_TO_TICKS(100)) == pdTRUE;
}

bool testHookSendFile(uint8_t fileType, uint32_t size, uint16_t dest) {
  if (fileState != FILE_STATE_IDLE) {
    Serial.printf("EVT ERR cmd=TX_FILE reason=busy state=%d\n", (int)fileState);
    return false;
  }
  if (size == 0 || size > 200 * 1024 || ESP.getFreeHeap() < size + 30000) {
    Serial.printf("EVT ERR cmd=TX_FILE reason=no_memory size=%lu heap=%lu\n",
      (unsigned long)size, (unsigned long)ESP.getFreeHeap());
    return false;
  }
  if (fileTxBuffer) { free(fileTxBuffer); fileTxBuffer = nullptr; }
  fileTxBuffer = (uint8_t*)malloc(size);
  if (!fileTxBuffer) return false;
  for (uint32_t i = 0; i < size; i++) fileTxBuffer[i] = testFilePatternByte(i);

  fileTxSize = size;
  fileTxOffset = size;
  fileTxType = fileType;
  fileTxDest[0] = dest & 0xFF;
  fileTxDest[1] = (dest >> 8) & 0xFF;
  fileTxChunksTotal = (size + CHUNK_SIZE - 1) / CHUNK_SIZE;
  fileTxSessionId = (++fileSessionId) & 0xFF;
  memset(fileTxName, 0, sizeof(fileTxName));
  snprintf(fileTxName, sizeof(fileTxName), "t%02X_%lu", fileType, (unsigned long)size);
  fileState = FILE_STATE_SENDING;
  fileTxActive = true;
  fileTxLedUntil = millis() + (uint32_t)fileTimeoutSec * 1000;
  if (fileSendTaskHandle) xTaskNotifyGive(fileSendTaskHandle);
  return true;
}

bool testHookCall(const char* kind, const uint8_t* target4) {
  if (!kind) return false;
  if (strcasecmp(kind, "all") == 0)   { callSendAll(nullptr, 0); return true; }
  if (strcasecmp(kind, "priv") == 0)  { callSendPrivate(target4, nullptr, 0); return true; }
  if (strcasecmp(kind, "group") == 0) { callSendGroup(0, nullptr, 0); return true; }
  return false;
}

bool testHookCallResponse(const char* kind, uint8_t seq) {
  if (!kind) return false;
  if (strcasecmp(kind, "accept") == 0) { callAccept(seq); return true; }
  if (strcasecmp(kind, "reject") == 0) { callReject(seq); return true; }
  if (strcasecmp(kind, "cancel") == 0) { callCancel(); return true; }
  return false;
}

bool testHookSetChannel(uint8_t ch) {
  // Канал живёт в трёх местах: частота радио, поле channel в пакетах и запись
  // в памяти устройства. Раньше консоль меняла только частоту, и устройства
  // слышали друг друга, но отбрасывали пакеты как «чужой канал». А без записи
  // в память канал терялся при первой же перезагрузке — например, при входе в
  // режим ретранслятора, и узел молча уходил на другую частоту.
  if (ch >= NUM_CHANNELS) return false;
  currentChannel = ch;
  bool ok = loraSetChannel(ch);
  loraStartReceive();
  Preferences prefs;
  prefs.begin("settings", false);
  prefs.putUChar("channel", ch);
  prefs.end();
  return ok;
}

void testHookPowerOff() {
  powerOff();
}

void testHookInfo() {
  // id печатаем ровно в том виде, в каком его принимает parseDest: как
  // 16-битное значение little-endian. Раньше печатались байты по порядку,
  // и адрес из INFO, подставленный в TX, попадал в другое устройство.
  Serial.printf("EVT INFO name=%s cs=%s id=%04X ch=%d freq=%.3f pwr=%d duty=%d "
                "lora_mode=%d ble=%d file_state=%d uptime=%lu boot=%lu heap=%lu "
                "min_heap=%lu bat=%.2f pm=%d test=%d ptt=%d qaudio=%u\n",
    bleGetDeviceName().c_str(), beaconGetCallSign(),
    (unsigned)(senderMac[0] | (senderMac[1] << 8)),
    currentChannel, loraGetFrequency(currentChannel), loraGetTxPower(),
    loraIsDutyCycleEnabled() ? 1 : 0, (int)loraGetPowerMode(),
    bleIsConnected() ? 1 : 0, (int)fileState,
    (unsigned long)(millis() / 1000), (unsigned long)bootCount,
    (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
    getCachedBattery(), pmConfigResult,
    testConsoleIsActive() ? 1 : 0, pttActive ? 1 : 0,
    txAudioQueue ? (unsigned)uxQueueMessagesWaiting(txAudioQueue) : 0);

  Serial.printf("EVT INFO_TASKS lora=%u ble=%u filesend=%u main=%u\n",
    loraTaskHandle ? (unsigned)uxTaskGetStackHighWaterMark(loraTaskHandle) : 0,
    0u,
    fileSendTaskHandle ? (unsigned)uxTaskGetStackHighWaterMark(fileSendTaskHandle) : 0,
    (unsigned)uxTaskGetStackHighWaterMark(nullptr));
}

#endif // TEST_CONSOLE
