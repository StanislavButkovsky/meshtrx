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

// === Станции, которые слышит ретранслятор ===
//
// Маяки с координатами он и так разбирает — брал из них только адрес для
// защиты от повторов, а остальное выбрасывал. Теперь запоминает: на странице
// ретранслятора из этого получается карта, кто где находится. Список
// ограничен, вытесняется самая давняя запись: памяти на V3 немного, а видеть
// три десятка станций разом — уже редкость.
#define REPEATER_NODES_MAX 32

struct RepeaterNode {
  uint8_t  id[4];
  char     call_sign[9];
  int32_t  lat_e7;
  int32_t  lon_e7;
  int16_t  altitude_m;
  uint8_t  battery;        // 0–100, 0xFF = питание от USB
  bool     has_gps;
  int16_t  rssi;
  int8_t   snr;
  uint32_t last_seen_ms;
  uint32_t beacons;
};

void repeaterInit();
// Сколько станций в списке и сам список (указатель на внутренний массив)
uint8_t repeaterGetNodes(const RepeaterNode** out);
void repeaterTask(void* param);
void repeaterResetStats();
RepeaterStats repeaterGetStats();
bool repeaterIsEnabled();
void repeaterSetEnabled(bool enabled);
