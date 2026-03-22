#include <Arduino.h>
#include "audio_codec.h"

// Codec2 C library
extern "C" {
#include "codec2.h"
}

static struct CODEC2* codec2State = nullptr;

void codecInit() {
  codec2State = codec2_create(CODEC2_MODE_3200);
  if (codec2State) {
    Serial.println("[Codec2] Initialized, mode 3200");
  } else {
    Serial.println("[Codec2] FAILED to init!");
  }
}

void codecDestroy() {
  if (codec2State) {
    codec2_destroy(codec2State);
    codec2State = nullptr;
  }
}

void codecEncode(const int16_t* pcm, uint8_t* encoded) {
  if (!codec2State) return;
  codec2_encode(codec2State, encoded, (short*)pcm);
}

void codecDecode(const uint8_t* encoded, int16_t* pcm) {
  if (!codec2State) return;
  codec2_decode(codec2State, (short*)pcm, encoded);
}

void codecEncodePacket(const int16_t* pcm, uint8_t* encoded) {
  for (int i = 0; i < CODEC2_FRAMES_PER_PKT; i++) {
    codecEncode(pcm + i * CODEC2_FRAME_SAMPLES,
                encoded + i * CODEC2_FRAME_BYTES);
  }
}

void codecDecodePacket(const uint8_t* encoded, int16_t* pcm) {
  for (int i = 0; i < CODEC2_FRAMES_PER_PKT; i++) {
    codecDecode(encoded + i * CODEC2_FRAME_BYTES,
                pcm + i * CODEC2_FRAME_SAMPLES);
  }
}
