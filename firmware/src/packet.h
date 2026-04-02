#pragma once
#include <stdint.h>

// === Типы пакетов ===
#define PKT_TYPE_AUDIO      0xA0
#define PKT_TYPE_TEXT       0xB0
#define PKT_TYPE_TEXT_ACK   0xB1
#define PKT_TYPE_FILE_START 0xC0
#define PKT_TYPE_FILE_CHUNK 0xC1
#define PKT_TYPE_FILE_ACK   0xC2
#define PKT_TYPE_FILE_END   0xC3
#define PKT_TYPE_BEACON     0xD0
#define PKT_TYPE_CALL_ALL       0xE0
#define PKT_TYPE_CALL_PRIVATE   0xE1
#define PKT_TYPE_CALL_GROUP     0xE2
#define PKT_TYPE_CALL_EMERGENCY 0xE3
#define PKT_TYPE_CALL_ACCEPT    0xE4
#define PKT_TYPE_CALL_REJECT    0xE5
#define PKT_TYPE_CALL_CANCEL    0xE6

// === Флаги аудио пакета ===
#define PKT_FLAG_PTT_START  0x01
#define PKT_FLAG_PTT_END    0x02
#define PKT_FLAG_ROGER_BEEP 0x04
#define PKT_FLAG_VOX        0x08

// === Типы файлов ===
#define FILE_TYPE_PHOTO     0x01
#define FILE_TYPE_TEXT      0x02
#define FILE_TYPE_BINARY    0x03
#define FILE_TYPE_VOICE     0x04
#define FILE_TYPE_PTT_VOICE 0x05  // адресный PTT — автовоспроизведение, не хранить
#define CHUNK_SIZE          120

// === TTL ===
#define TTL_DEFAULT         2
#define TTL_MAX             4
#define TTL_NO_RELAY        0

// === Beacon флаги ===
#define BEACON_FLAG_GPS_VALID  0x01
#define BEACON_FLAG_VOX_ON     0x02
#define BEACON_FLAG_BUSY       0x04
#define BEACON_FLAG_REPEATER   0x08

// === Beacon интервалы (секунды) ===
#define BEACON_INTERVAL_1MIN   60
#define BEACON_INTERVAL_3MIN   180
#define BEACON_INTERVAL_5MIN   300
#define BEACON_INTERVAL_15MIN  900
#define BEACON_INTERVAL_30MIN  1800
#define BEACON_INTERVAL_1HOUR  3600
#define BEACON_INTERVAL_NEVER  0

// === Вызовы ===
#define CALL_TIMEOUT_SEC        30
#define EMERGENCY_REPEAT_SEC    30
#define MAX_GROUP_MEMBERS       8
#define MAX_SAVED_GROUPS        8

// === Аудио пакет (71 байт) ===
#pragma pack(push, 1)
struct LoRaAudioPacket {
  uint8_t  type;        // 0xA0
  uint8_t  channel;     // 0–22
  uint8_t  seq;
  uint8_t  flags;       // PTT_START/END, ROGER_BEEP, VOX
  uint8_t  ttl;
  uint8_t  sender[2];   // последние 2 байта MAC
  uint8_t  payload[32]; // Codec2 3200: 4 фрейма × 8 байт
};
#pragma pack(pop)

// === Текстовый пакет (до 93 байт) ===
#pragma pack(push, 1)
struct LoRaTextPacket {
  uint8_t  type;        // 0xB0
  uint8_t  channel;
  uint8_t  seq;
  uint8_t  ttl;
  uint8_t  sender[2];
  uint8_t  dest[2];     // 0x0000 = broadcast, иначе последние 2 байта MAC
  uint8_t  text[85];    // UTF-8, null-terminated
};
#pragma pack(pop)

// === Текстовый ACK (7 байт) ===
#pragma pack(push, 1)
struct LoRaTextAck {
  uint8_t  type;        // 0xB1
  uint8_t  channel;
  uint8_t  seq;         // seq подтверждаемого сообщения
  uint8_t  sender[2];   // кто подтверждает
  uint8_t  dest[2];     // кому подтверждение (= sender оригинала)
};
#pragma pack(pop)

// === Файл: заголовок ===
#pragma pack(push, 1)
struct LoRaFileHeader {
  uint8_t  type;         // 0xC0
  uint8_t  channel;
  uint8_t  session_id;
  uint8_t  ttl;
  uint8_t  sender[2];
  uint8_t  dest[2];      // 0x0000 = broadcast, иначе последние 2 байта MAC получателя
  uint8_t  file_type;    // FILE_TYPE_*
  uint16_t total_chunks;
  uint32_t total_size;
  uint8_t  name[20];     // null-terminated
};
#pragma pack(pop)

// === Файл: чанк данных ===
#pragma pack(push, 1)
struct LoRaFileChunk {
  uint8_t  type;         // 0xC1
  uint8_t  channel;
  uint8_t  session_id;
  uint8_t  ttl;
  uint8_t  dest[2];      // 0x0000 = broadcast
  uint16_t chunk_index;
  uint8_t  data[120];
};
#pragma pack(pop)

// === Файл: ACK (всё получено) или NACK bitmap (пропущенные чанки) ===
#pragma pack(push, 1)
struct LoRaFileAck {
  uint8_t  type;         // 0xC2
  uint8_t  session_id;
  uint8_t  status;       // 0x00=OK (всё получено), 0x01=NACK (есть пропуски)
  uint8_t  dest[2];      // кому (= sender оригинала)
  uint16_t missing_count; // кол-во пропущенных чанков
  uint16_t missing[50];  // индексы пропущенных (макс 50)
};
#pragma pack(pop)

// === Файл: конец ===
#pragma pack(push, 1)
struct LoRaFileEnd {
  uint8_t  type;         // 0xC3
  uint8_t  session_id;
  uint8_t  ttl;
  uint16_t crc16;
};
#pragma pack(pop)

// === Beacon (36 байт) ===
#pragma pack(push, 1)
struct LoRaBeaconPacket {
  uint8_t  type;          // 0xD0
  uint8_t  channel;
  uint8_t  device_id[4];  // последние 4 байта MAC
  uint8_t  call_sign[9];  // null-terminated, макс 8 символов
  int32_t  lat_e7;
  int32_t  lon_e7;
  int16_t  altitude_m;
  uint8_t  tx_power;
  uint8_t  battery;       // 0–100 или 0xFF=USB
  uint8_t  flags;
  uint32_t uptime_sec;
  uint16_t beacon_seq;
  uint16_t crc16;
};
#pragma pack(pop)

// === ALL CALL (32 байт) ===
#pragma pack(push, 1)
struct LoRaCallAll {
  uint8_t  type;          // 0xE0
  uint8_t  channel;
  uint8_t  ttl;
  uint8_t  sender[4];
  uint8_t  call_sign[9];
  int32_t  lat_e7;
  int32_t  lon_e7;
};
#pragma pack(pop)

// === PRIVATE CALL (36 байт) ===
#pragma pack(push, 1)
struct LoRaCallPrivate {
  uint8_t  type;          // 0xE1
  uint8_t  channel;
  uint8_t  ttl;
  uint8_t  sender[4];
  uint8_t  target[4];
  uint8_t  call_sign[9];
  uint8_t  call_seq;
  int32_t  lat_e7;
  int32_t  lon_e7;
};
#pragma pack(pop)

// === GROUP CALL ===
#pragma pack(push, 1)
struct LoRaCallGroup {
  uint8_t  type;          // 0xE2
  uint8_t  channel;
  uint8_t  ttl;
  uint8_t  sender[4];
  uint16_t group_id;
  uint8_t  member_count;
  uint8_t  members[8][4]; // макс 8 участников
  uint8_t  call_sign[9];
  uint8_t  group_name[9];
  int32_t  lat_e7;
  int32_t  lon_e7;
};
#pragma pack(pop)

// === EMERGENCY / SOS (30 байт) ===
#pragma pack(push, 1)
struct LoRaCallEmergency {
  uint8_t  type;          // 0xE3
  uint8_t  channel;
  uint8_t  ttl;
  uint8_t  sender[4];
  uint8_t  call_sign[9];
  int32_t  lat_e7;
  int32_t  lon_e7;
  uint8_t  sos_seq;
  uint8_t  flags;
  uint8_t  message[4];
};
#pragma pack(pop)

// === Ответ на вызов (8 байт) ===
#pragma pack(push, 1)
struct LoRaCallResponse {
  uint8_t  type;          // 0xE4/0xE5/0xE6
  uint8_t  channel;
  uint8_t  ttl;
  uint8_t  sender[4];
  uint8_t  call_seq;
};
#pragma pack(pop)
