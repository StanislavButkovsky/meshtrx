#pragma once
#include <stdint.h>

// Codec2 режим 3200 bps
// Фрейм: 20мс аудио (160 сэмплов @ 8000 Гц) → 8 байт
// В одном LoRa пакете: 8 фреймов = 64 байт = 160мс
#define CODEC2_FRAME_SAMPLES  160
#define CODEC2_FRAME_BYTES    8  // Codec2 3200bps = 64 bits = 8 bytes
#define CODEC2_FRAMES_PER_PKT 8
#define CODEC2_PKT_BYTES      (CODEC2_FRAME_BYTES * CODEC2_FRAMES_PER_PKT)  // 64

void codecInit();
void codecDestroy();

// Кодировать 320 PCM сэмплов → 8 байт Codec2
void codecEncode(const int16_t* pcm, uint8_t* encoded);

// Декодировать 8 байт → 320 PCM сэмплов
void codecDecode(const uint8_t* encoded, int16_t* pcm);

// Кодировать 6 фреймов (1920 сэмплов → 48 байт)
void codecEncodePacket(const int16_t* pcm, uint8_t* encoded);

// Декодировать 6 фреймов (48 байт → 1920 сэмплов)
void codecDecodePacket(const uint8_t* encoded, int16_t* pcm);
