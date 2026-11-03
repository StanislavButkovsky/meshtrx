#include "usb_link.h"

static const uint8_t FRAME_START = 0xC1;
static const uint8_t FRAME_END   = 0xC0;
static const uint8_t ESC         = 0xDB;
static const size_t  MAX_PACKET  = 512;

static UsbLinkDataCallback dataCb = nullptr;
static volatile uint32_t lastFrameMs = 0;

static uint8_t rxBuf[MAX_PACKET + 2];
static size_t  rxLen = 0;
static bool    inFrame = false;
static bool    escNext = false;

#ifdef TEST_CONSOLE
static char     consoleRing[256];
static volatile uint16_t ringHead = 0, ringTail = 0;
#endif

static uint16_t crc16(const uint8_t* p, size_t n) {
  // CRC-16/CCITT-FALSE: ловит обрыв и перепутанные байты, если в рамку
  // вклинился чужой вывод
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= (uint16_t)(*p++) << 8;
    for (int i = 0; i < 8; i++) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
  }
  return crc;
}

void usbLinkInit(UsbLinkDataCallback cb) {
  dataCb = cb;
}

bool usbLinkActive() {
  return lastFrameMs && millis() - lastFrameMs < USB_LINK_TIMEOUT_MS;
}

static void frameDone() {
  if (rxLen < 2) return;
  uint16_t got = (uint16_t)rxBuf[rxLen - 2] << 8 | rxBuf[rxLen - 1];
  if (crc16(rxBuf, rxLen - 2) != got) return;
  lastFrameMs = millis();
  // Пустая рамка — «я на месте»: кабель не сообщает, слушают ли на том
  // конце, поэтому хост напоминает о себе раз в несколько секунд.
  if (rxLen > 2 && dataCb) dataCb(rxBuf, rxLen - 2);
}

void usbLinkPoll() {
  while (Serial.available()) {
    uint8_t b = (uint8_t)Serial.read();
    if (b == FRAME_START) {
      inFrame = true; escNext = false; rxLen = 0;
      continue;
    }
    if (!inFrame) {
#ifdef TEST_CONSOLE
      uint16_t next = (ringHead + 1) % sizeof(consoleRing);
      if (next != ringTail) { consoleRing[ringHead] = (char)b; ringHead = next; }
#endif
      continue;
    }
    if (b == FRAME_END) {
      inFrame = false;
      frameDone();
      continue;
    }
    if (escNext) { b ^= 0x20; escNext = false; }
    else if (b == ESC) { escNext = true; continue; }
    if (rxLen < sizeof(rxBuf)) rxBuf[rxLen++] = b;
    else inFrame = false;   // слишком длинная — не наша рамка
  }
}

#ifdef TEST_CONSOLE
int usbLinkConsoleRead() {
  if (ringTail == ringHead) return -1;
  char c = consoleRing[ringTail];
  ringTail = (ringTail + 1) % sizeof(consoleRing);
  return (uint8_t)c;
}
#endif

static size_t putEsc(uint8_t* out, size_t n, uint8_t b) {
  if (b == FRAME_START || b == FRAME_END || b == ESC) {
    out[n++] = ESC;
    out[n++] = b ^ 0x20;
  } else {
    out[n++] = b;
  }
  return n;
}

bool usbLinkSend(const uint8_t* data, size_t len) {
  if (!usbLinkActive() || len > MAX_PACKET) return false;
  // Рамка уходит одним вызовом write: Serial держит блокировку на весь буфер,
  // и строки журнала из других задач не разрежут её посередине.
  uint8_t out[2 * MAX_PACKET + 8];
  size_t n = 0;
  out[n++] = FRAME_START;
  for (size_t i = 0; i < len; i++) n = putEsc(out, n, data[i]);
  uint16_t crc = crc16(data, len);
  n = putEsc(out, n, crc >> 8);
  n = putEsc(out, n, crc & 0xFF);
  out[n++] = FRAME_END;
  return Serial.write(out, n) == n;
}
