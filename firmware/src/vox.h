#pragma once
#include <stdint.h>

// === VOX состояния ===
enum VoxState {
  VOX_IDLE,
  VOX_ATTACK,
  VOX_ACTIVE,
  VOX_HANGTIME
};

// === VOX параметры по умолчанию ===
#define VOX_DEFAULT_THRESHOLD  800   // 0–32767
#define VOX_DEFAULT_HANGTIME   800   // мс
#define VOX_DEFAULT_ATTACK     50    // мс

void voxInit();
void voxProcess(uint16_t rms);
VoxState voxGetState();
bool voxIsActive();

void voxSetThreshold(uint16_t threshold);
void voxSetHangtime(uint32_t ms);
void voxSetAttack(uint32_t ms);
void voxReset();

uint16_t voxGetThreshold();
uint32_t voxGetHangtime();
