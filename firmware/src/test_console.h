#pragma once
#ifdef TEST_CONSOLE

#include <stdint.h>
#include <stddef.h>

// ================================================================
// Тестовая консоль — управление устройством через UART.
// Компилируется только в dev-сборках (-DTEST_CONSOLE).
//
// Формат ответов — машинно-читаемый, одной строкой:
//   EVT <NAME> key=value key=value
// чтобы стенд на ПК (firmware/test/harness) парсил их без эвристик.
// ================================================================

// Мощность для тестов на столе: устройства лежат рядом, поэтому
// уводим TX в аппаратный минимум SX1262, чтобы канал был хоть немного
// похож на реальный, а не «два передатчика в 10 см».
#define TEST_TX_POWER_DBM   (MIN_RADIO_DBM + PA_GAIN_DB)

void testConsoleInit();
void testConsoleTick();   // вызывать из loop()

// Эмуляция потерь канала: true → пакет считается потерянным и не обрабатывается
bool testConsoleShouldDrop(uint8_t pktType);

// Хуки, вызываемые из основной прошивки
void testConsoleOnLoRaRx(const uint8_t* data, int len, int16_t rssi, int8_t snr);
void testConsoleOnFileRxDone(const char* name, uint32_t size, uint16_t chunksTotal,
                             uint16_t chunksUnique, const uint8_t* buf);
void testConsoleOnFileTxDone(bool delivered, uint32_t ms, uint8_t nackRounds);

bool testConsoleIsActive();  // TESTMODE ON — принудительно минимальная мощность

// === Хуки, реализованные в main.cpp (доступ к состоянию прошивки) ===
bool testHookSendText(uint16_t dest, const char* text);
bool testHookSendAudio(uint16_t count, uint16_t gapMs);
bool testHookSendAudioOne(uint8_t flags);   // один аудиопакет (для фоновой нагрузки)
bool testHookPtt(bool on);
bool testHookSendFile(uint8_t fileType, uint32_t size, uint16_t dest);
bool testHookCall(const char* kind, const uint8_t* target4);
bool testHookCallResponse(const char* kind, uint8_t seq);
bool testHookSetChannel(uint8_t ch);
void testHookInfo();
uint8_t testHookCurrentChannel();
uint32_t testHookBootCount();

// Детерминированный паттерн тестового файла — общий для TX и проверки на RX
static inline uint8_t testFilePatternByte(uint32_t i) {
  return (uint8_t)((i * 31u + 7u) & 0xFF);
}

#endif // TEST_CONSOLE
