#include "vox.h"
#include <Arduino.h>

static VoxState state = VOX_IDLE;
static uint16_t threshold = VOX_DEFAULT_THRESHOLD;
static uint32_t hangtimeMs = VOX_DEFAULT_HANGTIME;
static uint32_t attackMs = VOX_DEFAULT_ATTACK;
static uint32_t voxTimerStart = 0;

void voxInit() {
  state = VOX_IDLE;
  voxTimerStart = 0;
}

void voxProcess(uint16_t rms) {
  uint32_t now = millis();

  switch (state) {
    case VOX_IDLE:
      if (rms > threshold) {
        voxTimerStart = now;
        state = VOX_ATTACK;
      }
      break;

    case VOX_ATTACK:
      if (rms < threshold) {
        // Щелчок / помеха — вернуться в IDLE
        state = VOX_IDLE;
      } else if ((now - voxTimerStart) >= attackMs) {
        // Подтверждённый голос — активировать TX
        state = VOX_ACTIVE;
      }
      break;

    case VOX_ACTIVE:
      if (rms < threshold) {
        voxTimerStart = now;
        state = VOX_HANGTIME;
      }
      break;

    case VOX_HANGTIME:
      if (rms > threshold) {
        // Продолжение речи
        state = VOX_ACTIVE;
      } else if ((now - voxTimerStart) >= hangtimeMs) {
        // Тишина — деактивировать TX
        state = VOX_IDLE;
      }
      break;
  }
}

VoxState voxGetState() {
  return state;
}

bool voxIsActive() {
  return (state == VOX_ACTIVE || state == VOX_HANGTIME);
}

void voxSetThreshold(uint16_t t) {
  threshold = t;
}

void voxSetHangtime(uint32_t ms) {
  hangtimeMs = ms;
}

void voxSetAttack(uint32_t ms) {
  attackMs = ms;
}

void voxReset() {
  state = VOX_IDLE;
  voxTimerStart = 0;
}

uint16_t voxGetThreshold() {
  return threshold;
}

uint32_t voxGetHangtime() {
  return hangtimeMs;
}
