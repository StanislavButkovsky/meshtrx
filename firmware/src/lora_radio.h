#pragma once
#include <RadioLib.h>

// === Конфигурация ===
#define ENFORCE_DUTY_CYCLE  false
#define TX_POWER_DBM        14
#ifdef BOARD_V4
  #define MAX_TX_POWER_DBM  22   // SX1262 max; GC1109 PA добавляет ~6 dB
#else
  #define MAX_TX_POWER_DBM  22   // SX1262 max (V3, без PA)
#endif
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
  #define PA_FEM_EN  2    // GC1109 enable (CSD)
  #define PA_FEM_CTX 46   // GC1109 TX/RX switch (CTX)
#endif

// === Радио параметры ===
#define LORA_BW          250.0   // кГц
#define LORA_SF          7
#define LORA_CR          5       // 4/5
#define LORA_SYNCWORD    0x34
#define LORA_PREAMBLE    8

extern SX1262 radio;
extern volatile bool loraRxFlag;
extern volatile bool loraTxDone;

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
