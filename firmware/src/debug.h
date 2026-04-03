#pragma once

// === Debug logging ===
// Определить NDEBUG в platformio.ini для release сборки
// В release: Serial не инициализируется, все логи вырезаются компилятором

#ifdef NDEBUG
  #define LOG_D(...)   ((void)0)
  #define LOG_F(...)   ((void)0)
  // Заглушки для Serial.print/printf в библиотечных файлах
  #define DBG_PRINT(...)   ((void)0)
  #define DBG_PRINTF(...)  ((void)0)
  #define DBG_PRINTLN(...) ((void)0)
#else
  #define LOG_D(msg)       Serial.println(msg)
  #define LOG_F(fmt, ...)  Serial.printf(fmt, ##__VA_ARGS__)
  #define DBG_PRINT(...)   Serial.print(__VA_ARGS__)
  #define DBG_PRINTF(...)  Serial.printf(__VA_ARGS__)
  #define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
#endif
