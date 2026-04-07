# Задача: Миграция NimBLE на IDF 5.x (pioarduino)

> Статус: ПЛАН
> Приоритет: ВЫСОКИЙ
> Цель: разблокировать CPU scaling + light sleep → 4-5 суток от 1200 мАч
> Оценка: 4-6 часов

---

## Проблема

BLE controller на ESP32-S3 (Arduino 2.x, NimBLE-Arduino 1.4) требует 240 МГц.
Любое снижение CPU частоты или включение PM крашит BLE (Guru Meditation LoadProhibited).
Это блокирует:
- setCpuFrequencyMhz(160/80) — экономия ~20-30 мА
- CONFIG_PM_ENABLE + tickless idle — auto light sleep
- BLE modem sleep — CPU спит между BLE events

## Решение

Миграция на pioarduino (Arduino 3.x + ESP-IDF 5.x) с обновлённым NimBLE.
pioarduino без custom_sdkconfig уже собирается и работает (проверено).
NimBLE-Arduino 2.x поддерживает BLE modem sleep нативно.

---

## Этапы

### Этап 1: Подготовка (без hardware)

- [ ] Изучить NimBLE-Arduino 2.x API changes: https://github.com/h2zero/NimBLE-Arduino/blob/release/2.x/Migration_guide.md
- [ ] Составить список всех изменений для ble_service.cpp
- [ ] Проверить совместимость RadioLib 6.4, U8g2, ArduinoJson с pioarduino

### Этап 2: Миграция platformio.ini

- [ ] Переключить platform на pioarduino
- [ ] Обновить NimBLE-Arduino до 2.x: `h2zero/NimBLE-Arduino @ ^2.0.0`
- [ ] Добавить custom_sdkconfig для PM:
  ```
  CONFIG_PM_ENABLE=y
  CONFIG_FREERTOS_USE_TICKLESS_IDLE=y
  CONFIG_BT_CTRL_MODEM_SLEEP=y
  CONFIG_BT_CTRL_MODEM_SLEEP_MODE_1=y
  CONFIG_BT_CTRL_LPCLK_SEL_MAIN_XTAL=y
  CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP=y
  CONFIG_ESP_PHY_MAC_BB_PD=y
  ```

### Этап 3: Адаптация ble_service.cpp

Известные изменения API NimBLE 1.4 → 2.x:

```cpp
// Power
NimBLEDevice::setPower(ESP_PWR_LVL_P6);  →  NimBLEDevice::setPower(6);

// Advertising
pAdvertising->setScanResponse(false);     →  pAdvertising->enableScanResponse(false);
pAdvertising->setMinPreferred(160);       →  (убрать — или setPreferredParams)

// Callbacks — возможно изменились сигнатуры
// Проверить: onConnect, onDisconnect, onMTUChange, onWrite
```

Файл: `firmware/src/ble_service.cpp` (~100 строк)

- [ ] Обновить setPower
- [ ] Обновить Advertising API
- [ ] Обновить Callback signatures (если изменились)
- [ ] Проверить MTU handling
- [ ] Проверить setValue/notify

### Этап 4: Power management код

- [ ] Добавить esp_pm_configure() в setupPowerManagement()
- [ ] Добавить esp_sleep_enable_bt_wakeup()
- [ ] Добавить GPIO wakeup для DIO1 и кнопки USER
- [ ] Добавить PM locks для PTT и file transfer
- [ ] Опционально: setCpuFrequencyMhz(160) при инициализации

### Этап 5: Тестирование

**Автоматические (47 тестов):**
- [ ] `pio test -e native` — 20 тестов (CRC, bitmap, packet sizes)
- [ ] `pio test -e test_embedded` — 28 тестов (hardware init, LoRa, BLE, OLED, GPIO)

**Ручные:**
- [ ] BLE подключение с телефона
- [ ] BLE не отваливается при idle
- [ ] PTT голос работает
- [ ] Текстовые сообщения
- [ ] Файловая передача (File Transfer v2)
- [ ] Вызовы (ALL, PRIVATE)
- [ ] Beacon виден другим устройствам
- [ ] OLED работает
- [ ] Потребление idle BLE connected < 20 мА (мультиметр)
- [ ] Потребление idle BLE disconnected < 5 мА
- [ ] Нет crash при длительной работе (8+ часов)

### Этап 6: Прошивка обоих девайсов + полевой тест

- [ ] Прошить оба V3
- [ ] Проверить голос на расстоянии
- [ ] Проверить файловую передачу
- [ ] Замерить время работы от батареи

---

## Файлы для изменения

| Файл | Изменения |
|------|-----------|
| `platformio.ini` | platform → pioarduino, NimBLE 2.x, custom_sdkconfig |
| `ble_service.cpp` | API адаптация (~10-15 строк) |
| `ble_service.h` | Возможно обновить includes |
| `main.cpp` | setupPowerManagement(), PM locks |

## Риски

| Риск | Вероятность | Mitigation |
|------|-------------|------------|
| NimBLE 2.x API breaking changes | Высокая | Migration guide + 28 embedded тестов |
| BLE modem sleep нестабилен | Средняя | Тестировать отдельно, можно отключить |
| RadioLib несовместим | Низкая | 6.4+ поддерживает IDF 5.x |
| Codec2 поведение изменилось | Низкая | Тест голоса |
| Heltec V3 board definition отсутствует | Низкая | Уже проверено — pioarduino имеет board |

## Ожидаемый результат

| Метрика | Сейчас | После |
|---------|--------|-------|
| CPU idle | 240 МГц (~50 мА) | 80 МГц + light sleep (~3 мА) |
| BLE connected idle | ~100 мА | ~10-15 мА |
| BLE disconnected | ~35 мА | ~2-3 мА |
| Время работы 1200 мАч (BLE) | ~12 часов | ~80-120 часов (3-5 суток) |
| Время работы 1200 мАч (автономно) | ~34 часа | ~400-600 часов (17-25 суток) |
