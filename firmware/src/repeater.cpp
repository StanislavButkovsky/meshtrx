#include "repeater.h"
#include "lora_radio.h"
#include "oled_display.h"
#include "beacon.h"
#include "packet.h"
#include <Arduino.h>
#include <Preferences.h>

static DedupEntry dedupCache[DEDUP_CACHE_SIZE];
static uint8_t dedupHead = 0;
static RepeaterStats stats;
static bool repeaterMode = false;

// Последний пакет для OLED
static int16_t lastRssi = 0;
static int8_t lastSnr = 0;
static uint8_t lastTtlFrom = 0;
static uint8_t lastTtlTo = 0;
static char lastPktType[4] = "---";

static bool dedupCheck(const uint8_t* sender, uint8_t seq, uint8_t type) {
  uint32_t now = millis();
  for (int i = 0; i < DEDUP_CACHE_SIZE; i++) {
    DedupEntry& e = dedupCache[i];
    if (e.timestamp == 0) continue;
    if ((now - e.timestamp) > DEDUP_LIFETIME_MS) continue;
    if (e.sender[0] == sender[0] && e.sender[1] == sender[1] &&
        e.seq == seq && e.type == type) {
      return true;  // дубль
    }
  }
  return false;
}

static void dedupAdd(const uint8_t* sender, uint8_t seq, uint8_t type) {
  DedupEntry& e = dedupCache[dedupHead];
  e.sender[0] = sender[0];
  e.sender[1] = sender[1];
  e.seq = seq;
  e.type = type;
  e.timestamp = millis();
  dedupHead = (dedupHead + 1) % DEDUP_CACHE_SIZE;
}

static const char* pktTypeName(uint8_t type) {
  switch (type) {
    case PKT_TYPE_AUDIO:      return "AUD";
    case PKT_TYPE_TEXT:       return "TXT";
    case PKT_TYPE_FILE_START: return "FIL";
    case PKT_TYPE_FILE_CHUNK: return "FIL";
    case PKT_TYPE_FILE_END:   return "FIL";
    case PKT_TYPE_BEACON:     return "BCN";
    case PKT_TYPE_CALL_ALL:
    case PKT_TYPE_CALL_PRIVATE:
    case PKT_TYPE_CALL_GROUP:
    case PKT_TYPE_CALL_EMERGENCY:
    case PKT_TYPE_CALL_ACCEPT:
    case PKT_TYPE_CALL_REJECT:
    case PKT_TYPE_CALL_CANCEL:return "CLL";
    default:                  return "???";
  }
}

static void updateStats(uint8_t type) {
  stats.fwd_count++;
  switch (type) {
    case PKT_TYPE_AUDIO:      stats.audio_fwd++; break;
    case PKT_TYPE_TEXT:       stats.text_fwd++; break;
    case PKT_TYPE_FILE_START:
    case PKT_TYPE_FILE_CHUNK:
    case PKT_TYPE_FILE_END:   stats.file_fwd++; break;
    case PKT_TYPE_BEACON:     stats.beacon_fwd++; break;
  }
}

void repeaterInit() {
  memset(dedupCache, 0, sizeof(dedupCache));
  memset(&stats, 0, sizeof(stats));
  stats.min_rssi = 0;
  stats.max_rssi = -150;

  Preferences prefs;
  prefs.begin("repeater", true);
  repeaterMode = prefs.getBool("mode", false);
  prefs.end();

  Serial.printf("[Repeater] Mode: %s\n", repeaterMode ? "ON" : "OFF");
}

void repeaterSetEnabled(bool enabled) {
  repeaterMode = enabled;
  Preferences prefs;
  prefs.begin("repeater", false);
  prefs.putBool("mode", enabled);
  prefs.end();
}

bool repeaterIsEnabled() {
  return repeaterMode;
}

void repeaterResetStats() {
  memset(&stats, 0, sizeof(stats));
  stats.min_rssi = 0;
  stats.max_rssi = -150;
  Serial.println("[Repeater] Stats cleared");
}

RepeaterStats repeaterGetStats() {
  return stats;
}

void repeaterTask(void* param) {
  Serial.println("[Repeater] Task started");
  loraStartReceive();

  uint8_t rxBuf[222];

  while (true) {
    if (!loraRxFlag) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    loraRxFlag = false;

    int len = radio.getPacketLength();
    if (len <= 0 || len > (int)sizeof(rxBuf)) {
      stats.drop_count++;
      loraStartReceive();
      continue;
    }

    int state = radio.readData(rxBuf, len);
    if (state != RADIOLIB_ERR_NONE) {
      stats.drop_count++;
      loraStartReceive();
      continue;
    }

    int16_t rssi = loraGetRSSI();
    int8_t snr = loraGetSNR();

    // Обновить RSSI min/max
    if (rssi < stats.min_rssi) stats.min_rssi = rssi;
    if (rssi > stats.max_rssi) stats.max_rssi = rssi;

    lastRssi = rssi;
    lastSnr = snr;

    uint8_t pktType = rxBuf[0];

    // Не ретранслировать FILE_ACK и неизвестные типы
    if (pktType == PKT_TYPE_FILE_ACK) {
      loraStartReceive();
      continue;
    }

    // Проверить канал (байт 1 для большинства пакетов)
    if (len >= 2 && rxBuf[1] != loraGetChannel()) {
      stats.drop_count++;
      loraStartReceive();
      continue;
    }

    // Извлечь sender, seq, ttl в зависимости от типа
    uint8_t sender[2] = {0, 0};
    uint8_t seq = 0;
    int ttlOffset = -1;

    if (pktType == PKT_TYPE_AUDIO && len >= (int)sizeof(LoRaAudioPacket)) {
      LoRaAudioPacket* p = (LoRaAudioPacket*)rxBuf;
      sender[0] = p->sender[0]; sender[1] = p->sender[1];
      seq = p->seq;
      ttlOffset = offsetof(LoRaAudioPacket, ttl);
    } else if (pktType == PKT_TYPE_TEXT && len >= 6) {
      LoRaTextPacket* p = (LoRaTextPacket*)rxBuf;
      sender[0] = p->sender[0]; sender[1] = p->sender[1];
      seq = p->seq;
      ttlOffset = offsetof(LoRaTextPacket, ttl);
    } else if (pktType == PKT_TYPE_FILE_START && len >= (int)sizeof(LoRaFileHeader)) {
      LoRaFileHeader* p = (LoRaFileHeader*)rxBuf;
      sender[0] = p->sender[0]; sender[1] = p->sender[1];
      seq = p->session_id;
      ttlOffset = offsetof(LoRaFileHeader, ttl);
    } else if (pktType == PKT_TYPE_FILE_CHUNK && len >= 6) {
      LoRaFileChunk* p = (LoRaFileChunk*)rxBuf;
      seq = p->session_id;
      ttlOffset = offsetof(LoRaFileChunk, ttl);
    } else if (pktType == PKT_TYPE_FILE_END && len >= (int)sizeof(LoRaFileEnd)) {
      seq = rxBuf[1];
      ttlOffset = offsetof(LoRaFileEnd, ttl);
    } else if (pktType == PKT_TYPE_BEACON && len >= (int)sizeof(LoRaBeaconPacket)) {
      LoRaBeaconPacket* p = (LoRaBeaconPacket*)rxBuf;
      sender[0] = p->device_id[2]; sender[1] = p->device_id[3];
      seq = (uint8_t)(p->beacon_seq & 0xFF);
      // Beacon не имеет TTL поля — используем TTL_DEFAULT
      ttlOffset = -1;
    } else if (pktType >= PKT_TYPE_CALL_ALL && pktType <= PKT_TYPE_CALL_CANCEL) {
      // Вызовы: ttl в байте 2
      ttlOffset = 2;
      if (len >= 7) {
        sender[0] = rxBuf[5]; sender[1] = rxBuf[6];
      }
      seq = rxBuf[2]; // используем ttl как часть ключа — не идеально, но для вызовов допустимо
    } else {
      // Неизвестный тип
      stats.drop_count++;
      loraStartReceive();
      continue;
    }

    // Дедупликация
    if (dedupCheck(sender, seq, pktType)) {
      stats.drop_count++;
      loraStartReceive();
      continue;
    }
    dedupAdd(sender, seq, pktType);

    // Проверка и декремент TTL
    uint8_t origTtl = 0;
    if (ttlOffset >= 0) {
      origTtl = rxBuf[ttlOffset];
      if (origTtl == 0) {
        stats.drop_count++;
        loraStartReceive();
        continue;
      }
      rxBuf[ttlOffset] = origTtl - 1;
    }

    lastTtlFrom = origTtl;
    lastTtlTo = origTtl > 0 ? origTtl - 1 : 0;
    strncpy(lastPktType, pktTypeName(pktType), 3);
    lastPktType[3] = 0;

    // Случайная задержка 10–50 мс (антиколлизия)
    vTaskDelay(pdMS_TO_TICKS(10 + (esp_random() % 41)));

    // TX
    oledRepeaterTxBlink(true);
    bool ok = loraSend(rxBuf, len);
    oledRepeaterTxBlink(false);

    if (ok) {
      updateStats(pktType);
    }

    loraStartReceive();

    // Обновить OLED
    oledShowRepeater(
      loraGetChannel(), loraGetFrequency(loraGetChannel()),
      loraGetTxPower(), loraIsDutyCycleEnabled(),
      lastRssi, lastSnr, lastTtlFrom, lastTtlTo, lastPktType,
      stats.fwd_count, stats.drop_count,
      (uint32_t)(esp_timer_get_time() / 1000000ULL)
    );
  }
}
