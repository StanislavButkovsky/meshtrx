#include "battery.h"
#include <Arduino.h>

#define PIN_VBAT_ADC  1
#define PIN_ADC_CTRL  37

static bool batInit = false;

void batteryInit() {
  pinMode(PIN_ADC_CTRL, OUTPUT);
  digitalWrite(PIN_ADC_CTRL, LOW);
  batInit = true;
}

float batteryReadVoltage() {
  if (!batInit) batteryInit();
  digitalWrite(PIN_ADC_CTRL, HIGH);
  delay(10);
  // Усреднение 8 замеров для стабильности
  int sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(PIN_VBAT_ADC);
    delayMicroseconds(500);
  }
  int raw = sum / 8;
  digitalWrite(PIN_ADC_CTRL, LOW);
  // Множитель делителя подобран замером: у V3 и V4 разные номиналы, и общая
  // константа давала на V4 завышение — 4,35 В там, где на клеммах 4,20.
  return (raw / 4095.0f) * 3.3f * BAT_DIVIDER;
}

uint8_t batteryReadPercent() {
  float v = batteryReadVoltage();
  if (v > 4.1f) return 100;
  if (v < 3.0f) return 0;
  return (uint8_t)((v - 3.0f) / 1.2f * 100.0f);
}
