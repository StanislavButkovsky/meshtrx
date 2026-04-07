#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// === CRC16-CCITT ===
inline uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

// === Bitmap operations for file chunk tracking ===
// Max 1024 chunks, bitmap = 128 bytes

inline void bitmap_clear(uint8_t* bitmap, size_t bitmap_size) {
  memset(bitmap, 0, bitmap_size);
}

inline bool bitmap_set(uint8_t* bitmap, uint16_t idx) {
  // Returns true if bit was NOT set before (new unique chunk)
  uint8_t bit = 1 << (idx & 7);
  if (!(bitmap[idx >> 3] & bit)) {
    bitmap[idx >> 3] |= bit;
    return true;
  }
  return false;
}

inline bool bitmap_get(const uint8_t* bitmap, uint16_t idx) {
  return (bitmap[idx >> 3] & (1 << (idx & 7))) != 0;
}

inline uint16_t bitmap_find_missing(const uint8_t* bitmap, uint16_t total, uint16_t* missing, uint16_t max_missing) {
  uint16_t cnt = 0;
  for (uint16_t i = 0; i < total && cnt < max_missing; i++) {
    if (!bitmap_get(bitmap, i)) {
      missing[cnt++] = i;
    }
  }
  return cnt;
}

inline uint16_t bitmap_count_set(const uint8_t* bitmap, uint16_t total) {
  uint16_t cnt = 0;
  for (uint16_t i = 0; i < total; i++) {
    if (bitmap_get(bitmap, i)) cnt++;
  }
  return cnt;
}
