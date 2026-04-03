#include "oled_display.h"
#include <Wire.h>

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RST, OLED_SCL, OLED_SDA);

static uint32_t tempMsgExpire = 0;
static char tempLine1[32] = {0};
static char tempLine2[32] = {0};

// === Управление сном экрана ===
static uint32_t oledSleepAt = 0;      // millis() когда погасить (0 = не гасить)
static bool oledAwake = true;
#define OLED_TIMEOUT_MS  30000         // 30 секунд

void oledInit() {
  // Heltec V3: Vext (GPIO36) управляет питанием OLED
  pinMode(36, OUTPUT);
  digitalWrite(36, LOW);  // LOW = включить питание OLED
  delay(50);

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.clearBuffer();
  u8g2.drawStr(20, 30, "MeshTRX");
  u8g2.drawStr(20, 45, "Starting...");
  u8g2.sendBuffer();
  oledSleepAt = millis() + OLED_TIMEOUT_MS;
  oledAwake = true;
  Serial.println("[OLED] Initialized");
}

void oledClear() {
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

void oledShowMain(uint8_t channel, float freqMHz, int16_t rssi, int8_t snr,
                  int8_t txPower, bool bleConn, bool dutyCycle,
                  bool pttActive, bool voxActive, float batteryV) {
  // Экран спит — не обновлять
  if (!oledAwake) return;

  // Если есть временное сообщение — показать его
  if (tempMsgExpire > 0 && millis() < tempMsgExpire) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 25, tempLine1);
    u8g2.drawStr(0, 40, tempLine2);
    u8g2.sendBuffer();
    return;
  }
  tempMsgExpire = 0;

  char buf[22];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  // Строка 0: MeshTRX + статус PTT/VOX
  if (pttActive) {
    u8g2.drawStr(0, 8, "MeshTRX  >>> TX <<<");
  } else if (voxActive) {
    u8g2.drawStr(0, 8, "MeshTRX  [VOX]");
  } else {
    u8g2.drawStr(0, 8, "MeshTRX");
  }

  // Строка 1: канал и частота
  snprintf(buf, sizeof(buf), "CH:%02d %.2fMHz", channel, freqMHz);
  u8g2.drawStr(0, 20, buf);

  // Строка 2: RSSI и мощность
  snprintf(buf, sizeof(buf), "RSSI:%d PWR:%d", rssi, txPower);
  u8g2.drawStr(0, 32, buf);

  // Строка 3: SNR
  snprintf(buf, sizeof(buf), "SNR:%d dB", snr);
  u8g2.drawStr(0, 44, buf);

  // Батарея: напряжение + иконка (правый верхний угол)
  // Напряжение текстом перед иконкой
  snprintf(buf, sizeof(buf), "%.2fv", batteryV);
  u8g2.drawStr(80, 8, buf);

  // Иконка
  int bx = 110, by = 0, bw = 16, bh = 8;
  u8g2.drawFrame(bx, by, bw, bh);
  u8g2.drawBox(bx + bw, by + 2, 2, 4);
  // Заполнение: 3.0V=0%, 4.2V=100%
  int pct = (int)((batteryV - 3.0f) / 1.2f * 100.0f);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  int fill = (bw - 2) * pct / 100;
  if (fill > 0) u8g2.drawBox(bx + 1, by + 1, fill, bh - 2);

  // Строка 4: BLE статус + duty cycle
  snprintf(buf, sizeof(buf), "BLE:%s DC:%s",
    bleConn ? "OK" : "--",
    dutyCycle ? "ON" : "OFF");
  u8g2.drawStr(0, 56, buf);

  u8g2.sendBuffer();
}

void oledShowMessage(const char* line1, const char* line2, uint16_t durationMs) {
  if (!oledAwake) return;  // не будить экран — показать только если уже активен
  strncpy(tempLine1, line1, sizeof(tempLine1) - 1);
  strncpy(tempLine2, line2, sizeof(tempLine2) - 1);
  tempMsgExpire = millis() + durationMs;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 25, tempLine1);
  u8g2.drawStr(0, 40, tempLine2);
  u8g2.sendBuffer();
}

void oledShowRepeater(uint8_t channel, float freqMHz, int8_t txPower, bool dutyCycle,
                      int16_t rssi, int8_t snr, uint8_t lastTtlFrom, uint8_t lastTtlTo,
                      const char* lastPktType, uint32_t fwdCount, uint32_t dropCount,
                      uint32_t uptimeSec) {
  char buf[22];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  // Строка 0: заголовок (инвертированный)
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 10);
  u8g2.setDrawColor(0);
  u8g2.drawStr(16, 8, "** REPEATER **");
  u8g2.setDrawColor(1);

  // Строка 1: канал
  snprintf(buf, sizeof(buf), "CH:%02d  %.2fMHz", channel, freqMHz);
  u8g2.drawStr(0, 18, buf);

  // Строка 2: мощность и DC
  snprintf(buf, sizeof(buf), "PWR:%ddBm  DC:%s", txPower, dutyCycle ? "ON" : "OFF");
  u8g2.drawStr(0, 28, buf);

  // Разделитель
  u8g2.drawHLine(0, 30, 128);

  // Строка 4: RSSI последнего пакета
  snprintf(buf, sizeof(buf), "RX:%04ddBm SNR:%+d", rssi, snr);
  u8g2.drawStr(0, 40, buf);

  // Строка 5: TTL и тип
  snprintf(buf, sizeof(buf), "TTL:%d>%d [%s]", lastTtlFrom, lastTtlTo, lastPktType ? lastPktType : "---");
  u8g2.drawStr(0, 50, buf);

  // Строка 6: счётчики
  snprintf(buf, sizeof(buf), "FWD:%06lu DRP:%03lu", (unsigned long)fwdCount, (unsigned long)dropCount);
  u8g2.drawStr(0, 58, buf);

  // Строка 7: uptime — если хватает места
  // 128x64 = 8 строк по 8px, но шрифт 10px — 6 строк реально

  u8g2.sendBuffer();
}

void oledShowCallIncoming(const char* callType, const char* callSign,
                          const char* message, int16_t rssi, uint8_t ttl) {
  char buf[22];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  // Мигающий заголовок (инвертированный)
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  snprintf(buf, sizeof(buf), ">>> %s <<<", callType);
  u8g2.drawStr(4, 10, buf);
  u8g2.setDrawColor(1);

  snprintf(buf, sizeof(buf), "From: %s", callSign);
  u8g2.drawStr(0, 26, buf);

  if (message && message[0]) {
    u8g2.drawStr(0, 38, message);
  }

  snprintf(buf, sizeof(buf), "%ddBm  TTL:%d", rssi, ttl);
  u8g2.drawStr(0, 52, buf);

  u8g2.sendBuffer();
}

void oledShowSosActive(uint8_t sentCount, uint8_t countdownSec) {
  char buf[22];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  // Весь экран инвертирован
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 64);
  u8g2.setDrawColor(0);

  u8g2.drawStr(8, 14, "!! SOS ACTIVE !!");

  snprintf(buf, sizeof(buf), "Repeat in: %ds", countdownSec);
  u8g2.drawStr(8, 30, buf);

  snprintf(buf, sizeof(buf), "Sent: %d", sentCount);
  u8g2.drawStr(8, 44, buf);

  u8g2.drawStr(8, 58, "[USER btn=cancel]");

  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void oledShowSosIncoming(const char* callSign, int32_t lat_e7, int32_t lon_e7,
                         int16_t rssi, uint8_t ttl) {
  char buf[22];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  // Инвертированный экран
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 64);
  u8g2.setDrawColor(0);

  u8g2.drawStr(16, 12, "!!! S O S !!!");

  u8g2.drawStr(4, 26, callSign);

  if (lat_e7 != 0 || lon_e7 != 0) {
    float lat = lat_e7 / 1e7f;
    float lon = lon_e7 / 1e7f;
    snprintf(buf, sizeof(buf), "%.3f%c %.3f%c",
      fabsf(lat), lat >= 0 ? 'N' : 'S',
      fabsf(lon), lon >= 0 ? 'E' : 'W');
    u8g2.drawStr(4, 40, buf);
  }

  snprintf(buf, sizeof(buf), "RSSI:%d TTL:%d", rssi, ttl);
  u8g2.drawStr(4, 54, buf);

  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void oledRepeaterTxBlink(bool on) {
  u8g2.setDrawColor(on ? 0 : 1);
  u8g2.drawBox(0, 0, 128, 10);
  u8g2.setDrawColor(on ? 1 : 0);
  u8g2.drawStr(16, 8, "** REPEATER **");
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void oledWake() {
  if (!oledAwake) {
    digitalWrite(36, LOW);   // включить Vext
    delay(50);
    u8g2.setPowerSave(0);    // включить дисплей
    oledAwake = true;
  }
  oledSleepAt = millis() + OLED_TIMEOUT_MS;
}

bool oledIsAwake() {
  return oledAwake;
}

void oledSleepTick() {
  if (oledAwake && oledSleepAt > 0 && millis() >= oledSleepAt) {
    u8g2.setPowerSave(1);   // выключить дисплей
    delay(50);
    digitalWrite(36, HIGH); // отключить Vext полностью (экономия ~1-5 мА)
    oledAwake = false;
    oledSleepAt = 0;
  }
}

void oledOff() {
  u8g2.setPowerSave(1);
  delay(50);
  digitalWrite(36, HIGH);   // отключить Vext полностью
  oledAwake = false;
  oledSleepAt = 0;
}

U8G2_SSD1306_128X64_NONAME_F_HW_I2C& oledGetDisplay() {
  return u8g2;
}
