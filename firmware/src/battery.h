#pragma once

// Множитель делителя напряжения батареи.
// V3: 5.55 — среднее по двум устройствам (4.03 → 4.15, 4.11 → 4.21).
// V4: те же 5.55 давали 4.35 В при реальных 4.20, отсюда 5.55 x 4.20 / 4.35.
#ifdef BOARD_V4
  #define BAT_DIVIDER 5.36f
#else
  #define BAT_DIVIDER 5.55f
#endif
#include <stdint.h>

void batteryInit();
float batteryReadVoltage();    // напряжение (3.0-4.2V)
uint8_t batteryReadPercent();  // 0-100%
