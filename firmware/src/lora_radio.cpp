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

static void IRAM_ATTR onRxDone(void) {
  loraRxFlag = true;
  // Будим loraTask через task notification (вместо busy polling)
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

#ifdef BOARD_V4
  // Включить GC1109 PA, RX mode по умолчанию
  pinMode(PA_FEM_EN, OUTPUT);
  pinMode(PA_FEM_CTX, OUTPUT);
  digitalWrite(PA_FEM_EN, HIGH);
  digitalWrite(PA_FEM_CTX, LOW);
  Serial.println("[LoRa] GC1109 PA enabled (V4)");
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

bool loraSend(uint8_t* data, size_t len) {
  loraTxDone = false;

#ifdef BOARD_V4
  digitalWrite(PA_FEM_CTX, HIGH);  // PA → TX mode
#endif

  // Переключить DIO1 на TX done
  radio.setDio1Action(onTxDone);

  int state = radio.startTransmit(data, len);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LoRa] TX start FAIL: %d\n", state);
    radio.setDio1Action(onRxDone);
    return false;
  }

  // Ждать завершения TX (макс 500мс)
  uint32_t start = millis();
  while (!loraTxDone && (millis() - start) < 500) {
    delay(1);
  }

  // Вернуть DIO1 на RX
  radio.setDio1Action(onRxDone);

#ifdef BOARD_V4
  digitalWrite(PA_FEM_CTX, LOW);   // PA → RX mode
#endif

  if (!loraTxDone) {
    Serial.println("[LoRa] TX timeout");
    return false;
  }

  return true;
}

bool loraStartReceive() {
  loraRxFlag = false;
  radio.setDio1Action(onRxDone);
  int state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LoRa] RX start FAIL: %d\n", state);
    return false;
  }
  return true;
}

int16_t loraGetRSSI() {
  return (int16_t)radio.getRSSI();
}

int8_t loraGetSNR() {
  return (int8_t)radio.getSNR();
}

void loraSetTxPower(int8_t power) {
  // power — эффективная мощность (включая PA gain)
  if (power < 1) power = 1;
  if (power > MAX_TX_POWER_DBM) power = MAX_TX_POWER_DBM;
  currentTxPower = power;
  int8_t radioPower = power - PA_GAIN_DB;
  if (radioPower < 1) radioPower = 1;
  radio.setOutputPower(radioPower);
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

  if (mode == LORA_POWER_DUTY_CYCLE_RX) {
    // RX Duty Cycle — радио само чередует RX окна и sleep
    radio.setDio1Action(onRxDone);
    int state = radio.startReceiveDutyCycleAuto(LORA_PREAMBLE_LONG, 8);
    if (state == RADIOLIB_ERR_NONE) {
      currentPowerMode = LORA_POWER_DUTY_CYCLE_RX;
      Serial.println("[LoRa] Power: DUTY_CYCLE_RX");
    } else {
      // Fallback на постоянный RX
      Serial.printf("[LoRa] Duty cycle failed (%d), fallback continuous\n", state);
      loraStartReceive();
      currentPowerMode = LORA_POWER_CONTINUOUS_RX;
    }
  } else {
    // Постоянный RX
    loraStartReceive();
    currentPowerMode = LORA_POWER_CONTINUOUS_RX;
    Serial.println("[LoRa] Power: CONTINUOUS_RX");
  }
}

LoRaPowerMode loraGetPowerMode() {
  return currentPowerMode;
}

bool loraSendWake(uint8_t* data, size_t len) {
  // Отправка с длинной преамбулой — будит устройства в duty cycle mode
  radio.setPreambleLength(LORA_PREAMBLE_LONG);
  bool ok = loraSend(data, len);
  // Восстановить преамбулу
  radio.setPreambleLength(
    currentPowerMode == LORA_POWER_DUTY_CYCLE_RX ? LORA_PREAMBLE_LONG : LORA_PREAMBLE_SHORT
  );
  return ok;
}
