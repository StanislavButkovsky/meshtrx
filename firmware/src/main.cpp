#include <Arduino.h>
#include <nvs_flash.h>
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

#include "debug.h"

// === Пины ===
#define PIN_LED       35
#define PIN_USER_BTN  0

// === Очереди FreeRTOS ===
static QueueHandle_t txAudioQueue = nullptr;   // аудио пакеты для TX
static QueueHandle_t txTextQueue  = nullptr;   // текстовые пакеты для TX

// === Состояние ===
static volatile bool pttActive = false;
uint8_t currentChannel = DEFAULT_CHANNEL;
static uint8_t audioSeqNum = 0;
static uint8_t textSeqNum = 0;
static uint8_t senderMac[2] = {0};  // последние 2 байта MAC
static int16_t lastRssi = 0;
static int8_t lastSnr = 0;

// Файловая передача
static uint8_t fileSessionId = 0;
static uint8_t* fileRxBuffer = nullptr;
static uint32_t fileRxSize = 0;
static uint16_t fileRxChunksTotal = 0;
static uint16_t fileRxChunksDone = 0;
static uint8_t fileRxType = 0;
static char fileRxName[20] = {0};
static uint8_t fileRxSender[2] = {0};
static bool fileRxActive = false;
static uint32_t fileRxLastChunkMs = 0; // таймаут приёма
static volatile bool fileRxComplete = false; // флаг: передать на телефон из bleTask
static uint8_t fileRxBitmap[128]; // битовая карта: максимум 1024 чанков (128*8)
static uint16_t fileRxUniqueCount = 0;
static volatile bool fileTxActive = false;  // LED при отправке файла
static volatile uint32_t fileTxLedUntil = 0; // авто-сброс

// LED RX индикация
static volatile uint32_t rxLedUntil = 0;

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
static void fileTaskFunc(void* param);
static void handleBleData(uint8_t* data, size_t len);
static void processLoRaPacket(uint8_t* data, int len, int16_t rssi, int8_t snr);
static void sendStatusUpdate();
static void loadSettings();
static void handleUserButton();

// ================================================================
// SETUP
// ================================================================
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

    // Инициализация модулей
    loraInit();
    codecInit();
    beaconInit();
    callManagerInit();
    loadSettings();

    // BLE
    bleInit();
    bleSetDataCallback(handleBleData);

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
  }

  LOG_D("=== MeshTRX Ready ===");
}

// ================================================================
// LOOP — обработка кнопки USER + вызовы callTick
// ================================================================
void loop() {
  handleUserButton();
  oledSleepTick();

  if (!repeaterIsEnabled()) {
    callTick();
  }

  delay(200);  // 200мс (было 50мс) — экономия CPU, кнопка реагирует приемлемо
}

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

  prefs.end();
  LOG_D("[Settings] Loaded from NVS");
}

// ================================================================
// Кнопка USER (GPIO0)
// ================================================================
static void handleUserButton() {
  bool pressed = (digitalRead(PIN_USER_BTN) == LOW);

  if (pressed && !userBtnPressed) {
    userBtnPressTime = millis();
    userBtnPressed = true;
  }

  if (!pressed && userBtnPressed) {
    uint32_t held = millis() - userBtnPressTime;
    userBtnPressed = false;

    if (repeaterIsEnabled()) {
      if (held > 3000) {
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
      if (held > 5000 && callGetState() == CALL_EMERGENCY_TX) {
        callCancel();
        oledShowMessage("SOS CANCELLED", "", 2000);
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
    // Ждём: DIO1 interrupt (RX done), TX queue, или таймаут 50мс
    // При PTT active — чаще проверяем TX очередь
    uint32_t waitMs = pttActive ? 5 : 50;
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(waitMs));

    // Проверить входящий LoRa пакет
    if (loraRxFlag) {
      loraRxFlag = false;

      int len = radio.getPacketLength();
      if (len > 0 && len <= (int)sizeof(rxBuf)) {
        int state = radio.readData(rxBuf, len);
        if (state == RADIOLIB_ERR_NONE) {
          lastRssi = loraGetRSSI();
          lastSnr = loraGetSNR();
          processLoRaPacket(rxBuf, len, lastRssi, lastSnr);
        }
      }
      loraStartReceive();
    }

    // Отправка аудио из очереди (приоритет)
    if (pttActive) {
      if (xQueueReceive(txAudioQueue, &txAudioPkt, 0) == pdTRUE) {
        loraSend((uint8_t*)&txAudioPkt, sizeof(txAudioPkt));
        loraStartReceive();
      }
    }

    // Отправка текста из очереди
    if (!pttActive) {
      if (xQueueReceive(txTextQueue, &txTextPkt, 0) == pdTRUE) {
        size_t textLen = strlen((char*)txTextPkt.text);
        size_t pktLen = 6 + textLen + 1; // header + text + null
        loraSend((uint8_t*)&txTextPkt, pktLen);
        loraStartReceive();
      }
    }
  }
}

// ================================================================
// Обработка принятого LoRa пакета
// ================================================================
static void processLoRaPacket(uint8_t* data, int len, int16_t rssi, int8_t snr) {
  if (len < 1) return;
  uint8_t pktType = data[0];

  // LED на 500мс при любом принятом пакете
  rxLedUntil = millis() + 500;

  switch (pktType) {
    case PKT_TYPE_AUDIO: {
      if (len < (int)sizeof(LoRaAudioPacket)) break;
      LoRaAudioPacket* pkt = (LoRaAudioPacket*)data;
      if (pkt->channel != currentChannel) break;

      // Отправить на телефон через BLE: cmd + flags + sender[2] + payload
      uint8_t bleData[4 + CODEC2_PKT_BYTES];
      bleData[0] = BLE_CMD_AUDIO_RX;
      bleData[1] = pkt->flags; // PKT_FLAG_PTT_END и др.
      bleData[2] = pkt->sender[0];
      bleData[3] = pkt->sender[1];
      memcpy(bleData + 4, pkt->payload, CODEC2_PKT_BYTES);
      bleSendNotify(bleData, 4 + CODEC2_PKT_BYTES);

      // Обновить OLED
      oledShowMain(currentChannel, loraGetFrequency(currentChannel),
                   rssi, snr, loraGetTxPower(), bleIsConnected(),
                   loraIsDutyCycleEnabled(), false, false, getCachedBattery());
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
          LoRaTextAck ack;
          ack.type = PKT_TYPE_TEXT_ACK;
          ack.channel = currentChannel;
          ack.seq = pkt->seq;
          memcpy(ack.sender, senderMac, 2);
          memcpy(ack.dest, pkt->sender, 2); // обратно отправителю
          loraSend((uint8_t*)&ack, sizeof(ack));
          loraStartReceive();
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
      fileRxActive = true;
      fileRxLastChunkMs = millis();
      memset(fileRxBitmap, 0, sizeof(fileRxBitmap));
      fileRxUniqueCount = 0;

      // Аллоцировать буфер
      if (fileRxBuffer) free(fileRxBuffer);
      fileRxBuffer = (uint8_t*)calloc(fileRxSize, 1); // обнулить
      if (!fileRxBuffer) {
        LOG_D("[File] malloc failed!");
        fileRxActive = false;
      }
      LOG_F("[File] RX start: %s (%d bytes, %d chunks)\n",
        fileRxName, fileRxSize, fileRxChunksTotal);
      break;
    }

    case PKT_TYPE_FILE_CHUNK: {
      if (len < 8 || !fileRxActive) break;
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
      if (idx < 1024) {
        uint8_t bit = 1 << (idx & 7);
        if (!(fileRxBitmap[idx >> 3] & bit)) {
          fileRxBitmap[idx >> 3] |= bit;
          fileRxUniqueCount++;
          fileRxChunksDone = fileRxUniqueCount;
        }
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

      // Авто-завершение когда ВСЕ уникальные чанки получены
      if (fileRxUniqueCount >= fileRxChunksTotal && fileRxBuffer) {
        LOG_F("[File] RX complete: %s (%d bytes, %d/%d unique chunks)\n",
          fileRxName, fileRxSize, fileRxUniqueCount, fileRxChunksTotal);
        fileRxActive = false;
        fileRxComplete = true;
        // Отправить ACK (всё получено)
        LoRaFileAck ack;
        memset(&ack, 0, sizeof(ack));
        ack.type = PKT_TYPE_FILE_ACK;
        ack.session_id = fileSessionId;
        ack.status = 0x00; // OK
        memcpy(ack.dest, fileRxSender, 2);
        ack.missing_count = 0;
        loraSend((uint8_t*)&ack, 7); // type+session+status+dest+missing_count
        loraStartReceive();
      }
      break;
    }

    case PKT_TYPE_FILE_END: {
      if (len < (int)sizeof(LoRaFileEnd) || !fileRxActive) break;
      LoRaFileEnd* pkt = (LoRaFileEnd*)data;
      if (pkt->session_id != fileSessionId) break;

      // Проверить что все чанки получены
      if (fileRxUniqueCount >= fileRxChunksTotal) {
        // Всё получено — ACK
        LOG_F("[File] RX complete via FILE_END: %s (%d bytes)\n", fileRxName, fileRxSize);
        fileRxActive = false;
        fileRxComplete = true;
        LoRaFileAck ack;
        memset(&ack, 0, sizeof(ack));
        ack.type = PKT_TYPE_FILE_ACK;
        ack.session_id = fileSessionId;
        ack.status = 0x00;
        memcpy(ack.dest, fileRxSender, 2);
        ack.missing_count = 0;
        loraSend((uint8_t*)&ack, 7);
        loraStartReceive();
      } else {
        // Есть пропуски — NACK с индексами пропущенных
        LoRaFileAck nack;
        memset(&nack, 0, sizeof(nack));
        nack.type = PKT_TYPE_FILE_ACK;
        nack.session_id = fileSessionId;
        nack.status = 0x01; // NACK
        memcpy(nack.dest, fileRxSender, 2);
        uint16_t cnt = 0;
        for (uint16_t i = 0; i < fileRxChunksTotal && cnt < 50; i++) {
          if (!(fileRxBitmap[i >> 3] & (1 << (i & 7)))) {
            nack.missing[cnt++] = i;
          }
        }
        nack.missing_count = cnt;
        size_t nackLen = 7 + cnt * 2; // header + missing indices
        loraSend((uint8_t*)&nack, nackLen);
        loraStartReceive();
        LOG_F("[File] NACK: %d missing chunks\n", cnt);
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
      if (d != me) break;
      if (pkt->status == 0x00) {
        LOG_F("[File] TX ACK received — file delivered\n");
        // Уведомить телефон
        uint8_t progress[6];
        progress[0] = BLE_CMD_FILE_PROGRESS;
        progress[1] = pkt->session_id;
        progress[2] = 0xFF; progress[3] = 0xFF; // done marker
        progress[4] = 0xFF; progress[5] = 0xFF;
        bleSendNotify(progress, 6);
      } else {
        LOG_F("[File] TX NACK: %d missing chunks — forwarding to phone\n", pkt->missing_count);
        // Переслать NACK на телефон для досылки
        // BLE: FILE_PROGRESS с missing_count в special формате
        uint8_t nackBle[3];
        nackBle[0] = BLE_CMD_FILE_PROGRESS;
        nackBle[1] = pkt->session_id;
        nackBle[2] = 0xFE; // NACK marker
        bleSendNotify(nackBle, 3);
      }
      break;
    }

    case PKT_TYPE_BEACON: {
      if (len < (int)sizeof(LoRaBeaconPacket)) break;
      LoRaBeaconPacket* pkt = (LoRaBeaconPacket*)data;
      beaconProcessIncoming(pkt, rssi, snr);
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

static void bleTaskFunc(void* param) {
  LOG_D("[BLE Task] Started on Core 1");
  while (true) {
    // === Авто-сброс fileTxActive ===
    if (fileTxActive && millis() > fileTxLedUntil) {
      fileTxActive = false;
    }
    // === Таймаут fileRxActive (10 сек без чанков) ===
    if (fileRxActive && millis() - fileRxLastChunkMs > 10000) {
      LOG_F("[File] RX timeout: %d/%d unique chunks (lost %d)\n",
        fileRxUniqueCount, fileRxChunksTotal, fileRxChunksTotal - fileRxUniqueCount);
      // Отправить что есть если получено хотя бы 90%
      if (fileRxBuffer && fileRxUniqueCount >= fileRxChunksTotal * 9 / 10) {
        LOG_D("[File] >90% received, sending partial");
        fileRxComplete = true;
      } else {
        if (fileRxBuffer) { free(fileRxBuffer); fileRxBuffer = nullptr; }
      }
      fileRxActive = false;
    }

    // === LED индикация ===
    if (pttActive) {
      // TX голос: LED горит постоянно
      digitalWrite(PIN_LED, HIGH);
      ledState = true;
    } else if (fileTxActive || fileRxActive) {
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
      // BLE подключён: короткая вспышка каждые 5 сек
      if (!ledState && millis() - ledBlinkTimer > 5000) {
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

    // === Обновить OLED ===
    oledShowMain(currentChannel, loraGetFrequency(currentChannel),
                 lastRssi, lastSnr, loraGetTxPower(), bleIsConnected(),
                 loraIsDutyCycleEnabled(), pttActive,
                 false, getCachedBattery());

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

    // === BLE статус ===
    if (bleIsConnected()) {
      sendStatusUpdate();
    }

    vTaskDelay(pdMS_TO_TICKS(500));
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
      pttActive = true;
      audioSeqNum = 0;
      LOG_D("[BLE] PTT START");
      break;
    }

    case BLE_CMD_PTT_END: {
      pttActive = false;
      LOG_D("[BLE] PTT END");
      // Отправить пакет с флагом PTT_END (телефон получателя воспроизведёт тон)
      {
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
      }
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

      prefs.end();
      LOG_D("[Settings] Applied & saved");
      break;
    }

    case BLE_CMD_GET_SETTINGS: {
      // Отправить текущие настройки
      char jsonBuf[160];
      snprintf(jsonBuf, sizeof(jsonBuf),
        "{\"duty_cycle\":%s,\"tx_power\":%d,\"beacon_interval\":%d,\"callsign\":\"%s\"}",
        loraIsDutyCycleEnabled() ? "true" : "false",
        loraGetTxPower(),
        (int)beaconGetInterval(),
        beaconGetCallSign());

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
      loraSend((uint8_t*)hdr, sizeof(LoRaFileHeader));
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
        vTaskDelay(pdMS_TO_TICKS(10)); // дать другим задачам время
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
      int32_t lat = 0, lon = 0;
      if (len >= 9) {
        memcpy(&lat, data + 1, 4);
        memcpy(&lon, data + 5, 4);
      }
      callSendEmergency(lat, lon);
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
