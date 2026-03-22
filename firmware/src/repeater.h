#pragma once
#include <stdint.h>

// === Дедупликационный кеш ===
#define DEDUP_CACHE_SIZE  64
#define DEDUP_LIFETIME_MS 30000  // 30 секунд

struct DedupEntry {
  uint8_t  sender[2];
  uint8_t  seq;
  uint8_t  type;
  uint32_t timestamp;
};

// === Статистика ===
struct RepeaterStats {
  uint32_t fwd_count;
  uint32_t drop_count;
  uint32_t audio_fwd;
  uint32_t text_fwd;
  uint32_t file_fwd;
  uint32_t beacon_fwd;
  int16_t  min_rssi;
  int16_t  max_rssi;
};

void repeaterInit();
void repeaterTask(void* param);
void repeaterResetStats();
RepeaterStats repeaterGetStats();
bool repeaterIsEnabled();
void repeaterSetEnabled(bool enabled);
