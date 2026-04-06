#pragma once
#include <RadioLib.h>

// === Конфигурация ===
#define ENFORCE_DUTY_CYCLE  false
#define TX_POWER_DBM        14
#define MAX_RADIO_DBM       22   // SX1262 max output
#ifdef BOARD_V4
  #define PA_GAIN_DB        6    // GC1109 PA gain
#else
  #define PA_GAIN_DB        0    // V3 — без PA
#endif
#define MAX_TX_POWER_DBM    (MAX_RADIO_DBM + PA_GAIN_DB)
#define DUTY_CYCLE_PERCENT  1
#define NUM_CHANNELS        23
#define DEFAULT_CHANNEL     0

// === Пины SX1262 (Heltec WiFi LoRa 32 V3/V4) ===
#define LORA_SCK   9
#define LORA_MISO  11
#define LORA_MOSI  10
#define LORA_NSS   8
#define LORA_RST   12
#define LORA_DIO1  14
#define LORA_BUSY  13

// === GC1109 PA (только V4) ===
#ifdef BOARD_V4
  #define PA_FEM_POWER 7   // GC1109 power supply (HIGH = вкл)
  #define PA_FEM_EN    2   // GC1109 enable (CSD)
  #define PA_FEM_CTX   46  // GC1109 TX/RX switch (CTX)
#endif

// === Радио параметры ===
#define LORA_BW          250.0   // кГц
#define LORA_SF          7
#define LORA_CR          7       // 4/7 — больше FEC для надёжности
#define LORA_SYNCWORD    0x34
#define LORA_PREAMBLE    8    // стандартная преамбула для голоса/файлов/чанков

// === Power modes ===
#define LORA_PREAMBLE_SHORT  8    // для голоса/файлов (active mode)
#define LORA_PREAMBLE_LONG   32   // для будящих пакетов (idle wake)

enum LoRaPowerMode {
    LORA_POWER_CONTINUOUS_RX,   // постоянный RX (active voice/file)
    LORA_POWER_DUTY_CYCLE_RX,   // duty cycle RX (idle, BLE connected)
    LORA_POWER_SLEEP,           // radio sleep (BLE disconnected, только beacon TX)
};

extern SX1262 radio;
extern volatile bool loraRxFlag;
extern volatile bool loraTxDone;
extern TaskHandle_t loraTaskHandle;  // для task notification из ISR
extern SemaphoreHandle_t loraRadioMutex;  // mutex для доступа к радио

void loraInit();
bool loraSetChannel(uint8_t ch);
float loraGetFrequency(uint8_t ch);
bool loraSend(uint8_t* data, size_t len);
bool loraStartReceive();
int16_t loraGetRSSI();
int8_t loraGetSNR();
void loraSetTxPower(int8_t power);
void loraSetDutyCycle(bool enabled);
bool loraIsDutyCycleEnabled();
int8_t loraGetTxPower();
uint8_t loraGetChannel();

// Power management
void loraSetPowerMode(LoRaPowerMode mode);
LoRaPowerMode loraGetPowerMode();
bool loraSendWake(uint8_t* data, size_t len);  // с длинной преамбулой
void loraPaEnable();   // включить PA (V4)
void loraPaDisable();  // выключить PA (V4) — экономия ~5-10 мА
