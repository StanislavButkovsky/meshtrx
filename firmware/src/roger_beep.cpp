#include "roger_beep.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Генерировать синус заданной частоты в буфер PCM
static void generateTone(int16_t* buf, int startSample, int numSamples,
                          float freqHz, int16_t amplitude) {
  for (int i = 0; i < numSamples; i++) {
    float t = (float)(startSample + i) / ROGER_BEEP_SAMPLE_RATE;
    buf[startSample + i] = (int16_t)(amplitude * sinf(2.0f * M_PI * freqHz * t));
  }
}

// Генерировать свип (линейная интерполяция частоты)
static void generateChirp(int16_t* buf, int startSample, int numSamples,
                           float freqStart, float freqEnd, int16_t amplitude) {
  for (int i = 0; i < numSamples; i++) {
    float progress = (float)i / numSamples;
    float freq = freqStart + (freqEnd - freqStart) * progress;
    float t = (float)(startSample + i) / ROGER_BEEP_SAMPLE_RATE;
    buf[startSample + i] = (int16_t)(amplitude * sinf(2.0f * M_PI * freq * t));
  }
}

// Тишина
static void generateSilence(int16_t* buf, int startSample, int numSamples) {
  memset(buf + startSample, 0, numSamples * sizeof(int16_t));
}

void rogerBeepInit() {
  // Ничего предпросчитывать не нужно на ESP32 — генерим на лету
}

int rogerBeepGenerate(RogerBeepType type, int16_t* pcmOut, int maxSamples) {
  memset(pcmOut, 0, maxSamples * sizeof(int16_t));

  switch (type) {
    case BEEP_NONE:
      return 0;

    case BEEP_SHORT: {
      // 440 Гц, 150 мс = 1200 сэмплов → округлить до 1 фрейма Codec2 = 320 * 1 = 320
      // Но 1200 > 320, нужно 4 фрейма = 1280 сэмплов
      int samples = 1200;  // 150мс
      if (samples > maxSamples) samples = maxSamples;
      generateTone(pcmOut, 0, samples, 440.0f, ROGER_BEEP_AMPLITUDE);
      // Округлить до кратного 320
      int frames = (samples + 319) / 320;
      return frames * 320;
    }

    case BEEP_MORSE_K: {
      // K = -.- : тире(300мс) + пауза(100мс) + точка(100мс) + пауза(100мс) + тире(300мс)
      // Итого ~900мс ≈ 7200 сэмплов → 23 фрейма... упростим до 3 элемента = ~500мс
      int pos = 0;
      // Тире 200мс
      generateTone(pcmOut, pos, 1600, 700.0f, ROGER_BEEP_AMPLITUDE); pos += 1600;
      // Пауза 80мс
      generateSilence(pcmOut, pos, 640); pos += 640;
      // Точка 80мс
      generateTone(pcmOut, pos, 640, 700.0f, ROGER_BEEP_AMPLITUDE); pos += 640;
      // Пауза 80мс
      generateSilence(pcmOut, pos, 640); pos += 640;
      // Тире 200мс
      if (pos + 1600 <= maxSamples) {
        generateTone(pcmOut, pos, 1600, 700.0f, ROGER_BEEP_AMPLITUDE);
        pos += 1600;
      }
      int frames = (pos + 319) / 320;
      return frames * 320;
    }

    case BEEP_TWO_TONE: {
      // 880 Гц 80мс → 440 Гц 120мс = 200мс = 1600 сэмплов
      int pos = 0;
      generateTone(pcmOut, pos, 640, 880.0f, ROGER_BEEP_AMPLITUDE); pos += 640;
      generateTone(pcmOut, pos, 960, 440.0f, ROGER_BEEP_AMPLITUDE); pos += 960;
      int frames = (pos + 319) / 320;
      return frames * 320;
    }

    case BEEP_CHIRP: {
      // Свип 600→1200 Гц, 200мс = 1600 сэмплов
      int samples = 1600;
      if (samples > maxSamples) samples = maxSamples;
      generateChirp(pcmOut, 0, samples, 600.0f, 1200.0f, ROGER_BEEP_AMPLITUDE);
      int frames = (samples + 319) / 320;
      return frames * 320;
    }
  }
  return 0;
}

int rogerBeepFrameCount(RogerBeepType type) {
  switch (type) {
    case BEEP_NONE:      return 0;
    case BEEP_SHORT:     return 4;   // 1280 сэмплов
    case BEEP_MORSE_K:   return 16;  // ~5120 сэмплов
    case BEEP_TWO_TONE:  return 5;   // 1600 сэмплов
    case BEEP_CHIRP:     return 5;   // 1600 сэмплов
    default:             return 0;
  }
}
