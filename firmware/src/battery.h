#pragma once
#include <stdint.h>

void batteryInit();
float batteryReadVoltage();    // напряжение (3.0-4.2V)
uint8_t batteryReadPercent();  // 0-100%
