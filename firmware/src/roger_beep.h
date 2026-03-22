#pragma once
#include <stdint.h>

// === Типы Roger Beep ===
enum RogerBeepType {
  BEEP_NONE      = 0,
  BEEP_SHORT     = 1,  // 440 Гц, 150 мс
  BEEP_MORSE_K   = 2,  // тон-тире-тон (~500 мс)
  BEEP_TWO_TONE  = 3,  // 880→440 Гц
  BEEP_CHIRP     = 4   // свип 600→1200 Гц, 200 мс
};

#define ROGER_BEEP_SAMPLE_RATE  8000
#define ROGER_BEEP_AMPLITUDE    16000

void rogerBeepInit();

// Получить PCM данные Roger Beep для кодирования Codec2
// Возвращает количество сэмплов (кратно 320 для Codec2 фреймов)
int rogerBeepGenerate(RogerBeepType type, int16_t* pcmOut, int maxSamples);

// Получить количество Codec2 фреймов для данного типа
int rogerBeepFrameCount(RogerBeepType type);
