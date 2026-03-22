#pragma once
#include <U8g2lib.h>

// === Пины OLED (Heltec V3) ===
#define OLED_SDA  17
#define OLED_SCL  18
#define OLED_RST  21

void oledInit();
void oledClear();

// Нормальный режим
void oledShowMain(uint8_t channel, float freqMHz, int16_t rssi, int8_t snr,
                  int8_t txPower, bool bleConnected, bool dutyCycle,
                  bool pttActive, bool voxActive, float batteryV);

// Временное сообщение (показать на N секунд)
void oledShowMessage(const char* line1, const char* line2, uint16_t durationMs = 3000);

// Режим ретранслятора
void oledShowRepeater(uint8_t channel, float freqMHz, int8_t txPower, bool dutyCycle,
                      int16_t rssi, int8_t snr, uint8_t lastTtlFrom, uint8_t lastTtlTo,
                      const char* lastPktType, uint32_t fwdCount, uint32_t dropCount,
                      uint32_t uptimeSec);

// Индикация вызовов
void oledShowCallIncoming(const char* callType, const char* callSign,
                          const char* message, int16_t rssi, uint8_t ttl);
void oledShowSosActive(uint8_t sentCount, uint8_t countdownSec);
void oledShowSosIncoming(const char* callSign, int32_t lat_e7, int32_t lon_e7,
                         int16_t rssi, uint8_t ttl);

// Мигание TX при ретрансляции
void oledRepeaterTxBlink(bool on);

// Управление питанием экрана
void oledWake();       // включить на 60 сек
bool oledIsAwake();    // проверить активен ли экран
void oledSleepTick();  // вызывать из loop — гасит экран по таймеру
void oledOff();        // выключить немедленно

U8G2_SSD1306_128X64_NONAME_F_HW_I2C& oledGetDisplay();
