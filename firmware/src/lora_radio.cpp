#include "lora_radio.h"
#include <SPI.h>

// SPI для LoRa
SPIClass loraSpi(HSPI);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSpi);

volatile bool loraRxFlag = false;
volatile bool loraTxDone = false;

static uint8_t currentChannel = DEFAULT_CHANNEL;
static int8_t  currentTxPower = TX_POWER_DBM;
static bool    dutyCycleEnabled = ENFORCE_DUTY_CYCLE;
static int16_t lastRSSI = 0;
static int8_t  lastSNR = 0;

// Таблица каналов: 863.150 + i * 0.300 МГц
static float channelFreq(uint8_t ch) {
  return 863.150f + ch * 0.300f;
}

// #ifdef REGION_US915
// static float channelFreqUS(uint8_t ch) {
//   return 903.900f + ch * 0.200f;
// }
// #endif

static LoRaPowerMode currentPowerMode = LORA_POWER_CONTINUOUS_RX;
TaskHandle_t loraTaskHandle = nullptr;
SemaphoreHandle_t loraRadioMutex = nullptr;
volatile bool loraAppBusy = false;

// Короткий RAII-захват mutex радио (для операций без вложенных вызовов)
struct MutexGuard {
  bool taken;
  MutexGuard() { taken = loraRadioMutex && xSemaphoreTake(loraRadioMutex, pdMS_TO_TICKS(500)) == pdTRUE; }
  ~MutexGuard() { if (taken) xSemaphoreGive(loraRadioMutex); }
};

// Обработчики DIO1. Уведомление задачи идёт только когда радио реально
// переведено в приём (rxArmed): иначе прерывание после передачи уходило
// в приёмную ветку и устраивало шторм — Interrupt WDT ронял CPU1.
static volatile bool txInProgress = false;
static volatile bool rxArmed = false;

// === Учёт времени в режимах радио ===
// Потребление SX1262 отличается на порядок между передачей, приёмом и
// standby, а амперметра под рукой нет. Копим фактические миллисекунды в
// каждом режиме — этого достаточно, чтобы посчитать средний ток и оценить
// время жизни батареи, а заодно увидеть, что радио «залипло» в приёме.
static volatile uint8_t  radioState = RADIO_ACC_STANDBY;
static volatile uint32_t radioStateSince = 0;
static volatile uint32_t radioAccMs[RADIO_ACC_COUNT] = {0, 0, 0, 0};
static volatile uint32_t radioAccStart = 0;
static volatile uint32_t radioTxCount = 0;

void loraResetRadioTime();

static void radioAccount(uint8_t next) {
  uint32_t now = millis();
  radioAccMs[radioState] += now - radioStateSince;
  radioStateSince = now;
  radioState = next;
}

static void IRAM_ATTR onRxDone(void) {
  if (txInProgress || !rxArmed) return;
  loraRxFlag = true;
  if (loraTaskHandle) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(loraTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

static void IRAM_ATTR onTxDone(void) {
  loraTxDone = true;
}

void loraInit() {
  loraResetRadioTime();
  loraRadioMutex = xSemaphoreCreateMutex();
  loraSpi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

  Serial.print("[LoRa] Initializing... ");
  int state = radio.begin(
    channelFreq(currentChannel),
    LORA_BW,
    LORA_SF,
    LORA_CR,
    LORA_SYNCWORD,
    currentTxPower,
    LORA_PREAMBLE
  );

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("FAILED, code ");
    Serial.println(state);
    while (true) { delay(1000); }
  }
  Serial.println("OK");

  // Включить аппаратный CRC
  radio.setCRC(true);

  // DIO1 прерывания
  radio.setDio1Action(onRxDone);

#if defined(BOARD_V4) && !defined(DISABLE_FEM)
  pinMode(PA_FEM_POWER, OUTPUT);
  pinMode(PA_FEM_EN, OUTPUT);
#ifdef PA_FEM_TX_MODE
  pinMode(PA_FEM_TX_MODE, OUTPUT);
#endif
#ifdef PA_FEM_RX_MODE
  pinMode(PA_FEM_RX_MODE, OUTPUT);
#endif
  // Приём/передачу усилителя переключает сам SX1262 линией DIO2. Делать это
  // ещё и из программы значит управлять одним входом с двух сторон.
  radio.setDio2AsRfSwitch(true);
  loraPaEnable();
  Serial.printf("[LoRa] FEM V4 rev %d enabled\n", BOARD_V4_REV);
#endif

  // Duty cycle — управляется программно в loraSend()
  // RadioLib SX1262 не имеет метода setDutyCycle

  Serial.printf("[LoRa] CH:%d  %.3f MHz  PWR:%d dBm  DC:%s\n",
    currentChannel, channelFreq(currentChannel), currentTxPower,
    dutyCycleEnabled ? "ON" : "OFF");
}

bool loraSetChannel(uint8_t ch) {
  if (ch >= NUM_CHANNELS) return false;
  currentChannel = ch;
  float freq = channelFreq(ch);
  MutexGuard g;
  int state = radio.setFrequency(freq);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LoRa] setFrequency FAIL: %d\n", state);
    return false;
  }
  Serial.printf("[LoRa] Channel %d → %.3f MHz\n", ch, freq);
  return true;
}

float loraGetFrequency(uint8_t ch) {
  if (ch >= NUM_CHANNELS) return 0;
  return channelFreq(ch);
}

// Внутренняя отправка — вызывающий ДОЛЖЕН держать loraRadioMutex
static bool loraSendLocked(uint8_t* data, size_t len) {
  loraTxDone = false;
  radioAccount(RADIO_ACC_TX);
  radioTxCount++;
  txInProgress = true;
  rxArmed = false;
  radio.setDio1Action(onTxDone);

#ifdef PA_FEM_TX_MODE
  digitalWrite(PA_FEM_TX_MODE, HIGH);  // полная мощность усилителя на время передачи
#endif

  int state = radio.startTransmit(data, len);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LoRa] TX start FAIL: %d\n", state);
    radio.setDio1Action(onRxDone);
    txInProgress = false;
    radioAccount(RADIO_ACC_STANDBY);
    return false;
  }

  // Ждать завершения TX (макс 500мс)
  uint32_t start = millis();
  while (!loraTxDone && (millis() - start) < 500) {
    delay(1);
  }

  // Завершить передачу: очистить IRQ-флаги и вернуть чип в standby.
  // Без finishTransmit() флаги TX оставались взведёнными, и последующий
  // приём молчал — отправитель не слышал ни ACK, ни NACK.
  radio.finishTransmit();
  radio.setDio1Action(onRxDone);
  txInProgress = false;
  radioAccount(RADIO_ACC_STANDBY);

#ifdef PA_FEM_TX_MODE
  // GPIO46 — strapping-пин: держим его низким везде, кроме самой передачи
  digitalWrite(PA_FEM_TX_MODE, LOW);
#endif

  if (!loraTxDone) {
    Serial.println("[LoRa] TX timeout");
    return false;
  }
  return true;
}

bool loraSend(uint8_t* data, size_t len) {
  if (!xSemaphoreTake(loraRadioMutex, pdMS_TO_TICKS(1000))) {
    Serial.println("[LoRa] TX mutex timeout");
    return false;
  }
  bool ok = loraSendLocked(data, len);
  xSemaphoreGive(loraRadioMutex);
  return ok;
}

// Внутренний переход в приём — вызывающий ДОЛЖЕН держать loraRadioMutex
static bool loraStartReceiveLocked() {
  loraRxFlag = false;
  txInProgress = false;
  rxArmed = false;
  radio.setDio1Action(onRxDone);
  int state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LoRa] RX start FAIL: %d\n", state);
    radioAccount(RADIO_ACC_STANDBY);
    return false;
  }
  rxArmed = true;
  radioAccount(RADIO_ACC_RX);
  return true;
}

bool loraStartReceive() {
  if (!xSemaphoreTake(loraRadioMutex, pdMS_TO_TICKS(500))) return false;
  bool ok = loraStartReceiveLocked();
  xSemaphoreGive(loraRadioMutex);
  return ok;
}

int16_t loraGetRSSI() {
  MutexGuard g;
  return (int16_t)radio.getRSSI();
}

int8_t loraGetSNR() {
  MutexGuard g;
  return (int8_t)radio.getSNR();
}

void loraSetTxPower(int8_t power) {
  // power — эффективная мощность (включая PA gain)
  if (power < MIN_TX_POWER_DBM) power = MIN_TX_POWER_DBM;
  if (power > MAX_TX_POWER_DBM) power = MAX_TX_POWER_DBM;
  currentTxPower = power;
  int8_t radioPower = power - PA_GAIN_DB;
  if (radioPower < MIN_RADIO_DBM) radioPower = MIN_RADIO_DBM;
  { MutexGuard g; radio.setOutputPower(radioPower); }
  Serial.printf("[LoRa] TX power → %d dBm effective (radio %d, PA +%d)\n", power, radioPower, PA_GAIN_DB);
}

void loraSetDutyCycle(bool enabled) {
  dutyCycleEnabled = enabled;
  Serial.printf("[LoRa] Duty cycle → %s\n", enabled ? "ON" : "OFF");
}

bool loraIsDutyCycleEnabled() {
  return dutyCycleEnabled;
}

int8_t loraGetTxPower() {
  return currentTxPower;
}

uint8_t loraGetChannel() {
  return currentChannel;
}

// === Power management ===

void loraSetPowerMode(LoRaPowerMode mode) {
  if (mode == currentPowerMode) return;
  MutexGuard g;

  if (mode == LORA_POWER_SLEEP) {
    // Radio standby — низкое потребление (~1.5 мА), можно быстро выйти в RX/TX
    // НЕ используем radio.sleep() — после него startReceive() возвращает -705
    radio.standby();
    rxArmed = false;
    radioAccount(RADIO_ACC_STANDBY);
    loraPaDisable();  // отключить PA полностью (V4)
    currentPowerMode = LORA_POWER_SLEEP;
    Serial.println("[LoRa] Power: STANDBY (beacon-only)");
  } else if (mode == LORA_POWER_DUTY_CYCLE_RX) {
    // RX Duty Cycle — радио само чередует RX окна и sleep
    loraPaEnable();   // приём без включённого PA на V4 глухой
    txInProgress = false;
    radio.setDio1Action(onRxDone);
    rxArmed = true;
    int state = radio.startReceiveDutyCycleAuto(LORA_PREAMBLE_LONG, 8);
    if (state == RADIOLIB_ERR_NONE) {
      currentPowerMode = LORA_POWER_DUTY_CYCLE_RX;
      radioAccount(RADIO_ACC_DUTY);
      Serial.println("[LoRa] Power: DUTY_CYCLE_RX");
    } else {
      // Fallback на постоянный RX
      Serial.printf("[LoRa] Duty cycle failed (%d), fallback continuous\n", state);
      loraStartReceiveLocked();
      currentPowerMode = LORA_POWER_CONTINUOUS_RX;
    }
  } else {
    // Постоянный RX
    loraPaEnable();  // включить PA (V4) — был выключен в sleep
    loraStartReceiveLocked();
    currentPowerMode = LORA_POWER_CONTINUOUS_RX;
    Serial.println("[LoRa] Power: CONTINUOUS_RX");
  }
}

LoRaPowerMode loraGetPowerMode() {
  return currentPowerMode;
}

uint16_t loraGetIrqStatus() {
  MutexGuard g;
  return radio.getIrqStatus();
}

void loraGetRadioTime(uint32_t out[RADIO_ACC_COUNT], uint32_t* windowMs,
                      uint32_t* txCount) {
  uint32_t now = millis();
  radioAccMs[radioState] += now - radioStateSince;
  radioStateSince = now;
  for (int i = 0; i < RADIO_ACC_COUNT; i++) out[i] = radioAccMs[i];
  if (windowMs) *windowMs = now - radioAccStart;
  if (txCount)  *txCount  = radioTxCount;
}

void loraResetRadioTime() {
  uint32_t now = millis();
  for (int i = 0; i < RADIO_ACC_COUNT; i++) radioAccMs[i] = 0;
  radioStateSince = now;
  radioAccStart = now;
  radioTxCount = 0;
}

bool loraIsRxArmed() { return rxArmed; }
bool loraIsTxInProgress() { return txInProgress; }

void loraPaEnable() {
#if defined(BOARD_V4) && !defined(DISABLE_FEM)
  digitalWrite(PA_FEM_POWER, HIGH);  // питание усилителя
  digitalWrite(PA_FEM_EN, HIGH);     // CSD: микросхема включена
#ifdef PA_FEM_TX_MODE
  digitalWrite(PA_FEM_TX_MODE, LOW); // 4.2: обход PA, пока не передаём
#endif
#ifdef PA_FEM_RX_MODE
  digitalWrite(PA_FEM_RX_MODE, LOW); // 4.3: приём через малошумящий усилитель
#endif
  Serial.println("[LoRa] PA ON");
#endif
}

void loraPaDisable() {
#if defined(BOARD_V4) && !defined(DISABLE_FEM)
#ifdef PA_FEM_TX_MODE
  digitalWrite(PA_FEM_TX_MODE, LOW);
#endif
#ifdef PA_FEM_RX_MODE
  digitalWrite(PA_FEM_RX_MODE, LOW);
#endif
  digitalWrite(PA_FEM_EN, LOW);
  digitalWrite(PA_FEM_POWER, LOW);   // полностью снять питание усилителя
  Serial.println("[LoRa] PA OFF");
#endif
}

void loraSleepForPowerOff() {
  // Здесь, в отличие от LORA_POWER_SLEEP, зовём именно radio.sleep(): после
  // него startReceive() возвращает -705 и радио надо инициализировать заново,
  // но возвращаться нам и не придётся — выход из глубокого сна на ESP32 это
  // полный сброс. Зато потребление чипа падает с полутора миллиампер до
  // единиц микроампер, а ради этого всё и затевалось.
  MutexGuard g;
  loraPaDisable();
  radio.sleep();
  rxArmed = false;
  txInProgress = false;
}

bool loraSendWake(uint8_t* data, size_t len) {
  // Отправка с длинной преамбулой — будит устройства в duty cycle mode.
  // Вся последовательность (преамбула → TX → возврат режима) под одним mutex:
  // иначе beacon из beaconTask рвал приём файла в loraTask (гонка за SPI).
  if (!xSemaphoreTake(loraRadioMutex, pdMS_TO_TICKS(1000))) {
    Serial.println("[LoRa] WAKE mutex timeout");
    return false;
  }
  bool wasSleeping = (currentPowerMode == LORA_POWER_SLEEP);
  if (wasSleeping) {
    loraPaEnable();  // временно включить PA для TX (V4)
  }
  radio.setPreambleLength(LORA_PREAMBLE_LONG);
  bool ok = loraSendLocked(data, len);
  radio.setPreambleLength(LORA_PREAMBLE);  // восстановить стандартную преамбулу

  if (wasSleeping) {
    loraPaDisable();    // выключить PA обратно
    radio.standby();    // обратно в standby после beacon TX
  } else {
    // Радио было в приёме — вернуть его туда БЕЗУСЛОВНО.
    // Раньше это делал вызывающий и только при подключённом телефоне,
    // из-за чего после beacon приёмник глох посреди файловой передачи.
    loraStartReceiveLocked();
    // Чип теперь в постоянном приёме — режим должен это отражать, иначе
    // учёт времени и логика возврата в duty cycle расходятся с железом.
    currentPowerMode = LORA_POWER_CONTINUOUS_RX;
  }
  xSemaphoreGive(loraRadioMutex);
  return ok;
}
