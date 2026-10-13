#include "inbox.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>
#include "debug.h"

// Размер буфера — от того, какая память есть на плате. У V4 есть PSRAM, и
// там 128 КБ ничего не стоят. У V3 её нет, а в общей куче (~115 КБ свободно)
// живут BLE, приём файлов и аудио: берём 8 КБ — это 150–250 обычных
// сообщений, файлам остаётся почти всё.
#define INBOX_BYTES_PSRAM  (128 * 1024)
#define INBOX_BYTES_HEAP   (8 * 1024)

// Запись в кольце: [длина пакета: 1][время приёма millis: 4][пакет]
#define REC_HDR 5

static uint8_t* ring = nullptr;
static size_t cap = 0;
static size_t head = 0;      // начало самой старой записи
static size_t used = 0;      // занято байт
static uint16_t count = 0;
static SemaphoreHandle_t lock = nullptr;

static void ringWrite(size_t pos, const uint8_t* src, size_t n) {
  for (size_t i = 0; i < n; i++) ring[(pos + i) % cap] = src[i];
}

static void ringRead(size_t pos, uint8_t* dst, size_t n) {
  for (size_t i = 0; i < n; i++) dst[i] = ring[(pos + i) % cap];
}

static void dropOldestLocked() {
  if (!count) return;
  size_t len = ring[head];
  head = (head + REC_HDR + len) % cap;
  used -= REC_HDR + len;
  count--;
}

void inboxInit() {
  if (ring) return;
  lock = xSemaphoreCreateMutex();
  if (psramFound()) {
    ring = (uint8_t*)heap_caps_malloc(INBOX_BYTES_PSRAM, MALLOC_CAP_SPIRAM);
    if (ring) cap = INBOX_BYTES_PSRAM;
  }
  if (!ring) {
    ring = (uint8_t*)malloc(INBOX_BYTES_HEAP);
    if (ring) cap = INBOX_BYTES_HEAP;
  }
  LOG_F("[Inbox] буфер входящих %u байт%s\n", (unsigned)cap,
        cap == INBOX_BYTES_PSRAM ? " (PSRAM)" : "");
}

uint16_t inboxPush(const uint8_t* bleMsg, size_t len) {
  if (!ring || len == 0 || len > INBOX_MAX_RECORD) return 0;
  uint16_t evicted = 0;
  xSemaphoreTake(lock, portMAX_DELAY);
  while (cap - used < REC_HDR + len && count) {
    dropOldestLocked();
    evicted++;
  }
  size_t tail = (head + used) % cap;
  uint8_t hdr[REC_HDR];
  uint32_t now = millis();
  hdr[0] = (uint8_t)len;
  memcpy(hdr + 1, &now, 4);
  ringWrite(tail, hdr, REC_HDR);
  ringWrite((tail + REC_HDR) % cap, bleMsg, len);
  used += REC_HDR + len;
  count++;
  xSemaphoreGive(lock);
  return evicted;
}

bool inboxEmpty() { return count == 0; }
uint16_t inboxCount() { return count; }
size_t inboxCapacityBytes() { return cap; }

size_t inboxPeek(uint8_t* out, size_t outSize, uint32_t* ageMs) {
  if (!ring) return 0;
  xSemaphoreTake(lock, portMAX_DELAY);
  if (!count) { xSemaphoreGive(lock); return 0; }
  uint8_t hdr[REC_HDR];
  ringRead(head, hdr, REC_HDR);
  size_t len = hdr[0];
  uint32_t at;
  memcpy(&at, hdr + 1, 4);
  if (len > outSize) len = outSize;
  ringRead((head + REC_HDR) % cap, out, len);
  if (ageMs) *ageMs = millis() - at;
  xSemaphoreGive(lock);
  return len;
}

void inboxDrop() {
  if (!ring) return;
  xSemaphoreTake(lock, portMAX_DELAY);
  dropOldestLocked();
  xSemaphoreGive(lock);
}
