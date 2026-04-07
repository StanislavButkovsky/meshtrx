# MeshTRX — План реализации ESP32-S3 Light Sleep

> Дата: 2026-04-07
> Цель: снизить потребление с ~100 мА до ~10-15 мА (idle, BLE connected)
> Результат: 1200 мАч → 80-120 часов (3-5 суток) вместо текущих 12 часов

---

## 1. Ключевое ограничение

**Arduino framework 2.x НЕ поддерживает auto light sleep** (tickless idle).
- `esp_pm_configure()` — заглушка, возвращает `ESP_ERR_NOT_SUPPORTED`
- `CONFIG_PM_ENABLE` и `CONFIG_FREERTOS_USE_TICKLESS_IDLE` не скомпилированы
- BLE modem sleep (`CONFIG_BT_CTRL_MODEM_SLEEP`) отключён

### Два пути

| Подход | Сложность | Результат |
|--------|-----------|-----------|
| **A. Manual light sleep** (Arduino 2.x) | Средняя | ~15-25 мА (только когда BLE disconnected) |
| **B. Миграция на pioarduino** (Arduino 3.x + IDF 5.x) | Высокая | ~5-10 мА (BLE + light sleep) |

**Рекомендация**: начать с **Path A** (быстрый результат, без миграции), потом **Path B** (полная экономия).

---

## 2. Path A: Manual Light Sleep (текущий Arduino 2.x)

### Принцип

`esp_light_sleep_start()` работает в Arduino 2.x. CPU засыпает, просыпается по GPIO или таймеру. Но BLE **теряет соединение** при manual light sleep (BLE стек не знает о sleep).

**Поэтому manual light sleep только когда BLE ОТКЛЮЧЕН.**

### Текущее состояние (BLE disconnected)

Сейчас при `!bleConnected`:
```cpp
loraSetPowerMode(LORA_POWER_SLEEP);  // radio.standby() ~1.5 мА
vTaskDelay(pdMS_TO_TICKS(1000));     // CPU спит 1 сек... НО CPU ~30 мА!
```

CPU продолжает работать на 240 МГц, просто блокируется на vTaskDelay.

### Что нужно сделать

Заменить `vTaskDelay(1000)` на `esp_light_sleep_start()`:

```cpp
if (!bleConnected && !repeaterIsEnabled()) {
    loraSetPowerMode(LORA_POWER_SLEEP);
    
    // Настроить пробуждение
    gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);   // Кнопка USER
    esp_sleep_enable_gpio_wakeup();
    esp_sleep_enable_timer_wakeup(5000000);  // 5 сек для beacon check
    
    // CPU засыпает (~0.8 мА)
    esp_light_sleep_start();
    
    // Проснулись — проверить причину
    continue;  // цикл проверит bleConnected
}
```

### Ожидаемый результат Path A

| Состояние | Сейчас | После |
|-----------|--------|-------|
| BLE disconnected | ~35-40 мА | **~3-5 мА** |
| BLE connected | ~100 мА | ~100 мА (без изменений) |

**Время работы BLE disconnected: 1200 мАч ÷ 4 мА = ~300 часов = 12 суток**

### Файлы для изменения (Path A)

- `main.cpp` — loraTask: заменить vTaskDelay на esp_light_sleep_start
- Добавить `#include <esp_sleep.h>` и `#include <driver/gpio.h>`

### Тесты Path A

- [ ] Девайс без BLE: потребление < 5 мА (мультиметр)
- [ ] Beacon отправляется каждые 5 мин (другой девайс видит)
- [ ] Кнопка USER будит девайс (OLED включается)
- [ ] BLE подключение после wake работает
- [ ] Переход BLE connected → disconnected → light sleep плавный
- [ ] Нет watchdog crash при длительном sleep

---

## 3. Path B: Миграция на pioarduino (Arduino 3.x + IDF 5.x)

### Принцип

pioarduino позволяет пересобрать ESP-IDF библиотеки с `CONFIG_PM_ENABLE=y`. После этого `esp_pm_configure()` работает, FreeRTOS tickless idle автоматически засыпает CPU когда все задачи заблокированы.

BLE modem sleep координируется с power manager — BLE соединение **сохраняется** при light sleep.

### Что нужно

#### 3.1 platformio.ini

```ini
[env]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
framework = arduino

custom_sdkconfig =
    CONFIG_PM_ENABLE=y
    CONFIG_FREERTOS_USE_TICKLESS_IDLE=y
    CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP=3
    CONFIG_BT_CTRL_MODEM_SLEEP=y
    CONFIG_BT_CTRL_MODEM_SLEEP_MODE_1=y
    CONFIG_BT_CTRL_LPCLK_SEL_MAIN_XTAL=y
    CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP=y
    CONFIG_ESP_PHY_MAC_BB_PD=y
```

#### 3.2 Power management инициализация

```cpp
#include <esp_pm.h>
#include <esp_sleep.h>

void setupPowerManagement() {
    esp_pm_config_esp32s3_t pm = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 40,      // XTAL frequency
        .light_sleep_enable = true
    };
    esp_pm_configure(&pm);

    // GPIO wakeup
    gpio_wakeup_enable(GPIO_NUM_14, GPIO_INTR_HIGH_LEVEL);  // LoRa DIO1
    gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);    // User button
    esp_sleep_enable_gpio_wakeup();
    esp_sleep_enable_bt_wakeup();  // BLE connection events
}
```

#### 3.3 PM locks для активных операций

```cpp
esp_pm_lock_handle_t voiceLock;
esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "voice", &voiceLock);

// PTT START:
esp_pm_lock_acquire(voiceLock);  // запретить sleep во время голоса

// PTT END:
esp_pm_lock_release(voiceLock);  // разрешить sleep
```

#### 3.4 FREERTOS_UNICORE

BLE power save требует `CONFIG_FREERTOS_UNICORE=y`. Нужно **убрать привязку задач к ядрам**:

```cpp
// Было:
xTaskCreatePinnedToCore(loraTaskFunc, "lora", 16384, nullptr, 5, &loraTaskHandle, 0);
xTaskCreatePinnedToCore(bleTaskFunc, "ble", 4096, nullptr, 5, nullptr, 1);

// Станет:
xTaskCreate(loraTaskFunc, "lora", 16384, nullptr, 5, &loraTaskHandle);
xTaskCreate(bleTaskFunc, "ble", 4096, nullptr, 5, nullptr);
```

#### 3.5 Устранение busy loops

**loraSend TX wait** — заменить polling на task notification:
```cpp
// Было:
while (!loraTxDone && (millis() - start) < 500) { delay(1); }

// Станет:
static void IRAM_ATTR onTxDone(void) {
    loraTxDone = true;
    BaseType_t wake = pdFALSE;
    vTaskNotifyGiveFromISR(loraTxTaskHandle, &wake);
    portYIELD_FROM_ISR(wake);
}
// В loraSend:
ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
```

**Beacon location wait** — заменить polling на notification:
```cpp
// Было:
while (!locationReceived && (millis() - start) < 2000) {
    vTaskDelay(pdMS_TO_TICKS(100));
}

// Станет:
ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000));  // wake из BLE callback
```

### Ожидаемый результат Path B

| Состояние | Сейчас | После |
|-----------|--------|-------|
| BLE connected idle | ~100 мА | **~8-15 мА** |
| BLE disconnected | ~35-40 мА | **~1-3 мА** |
| PTT voice active | ~120-150 мА | ~120-150 мА |

**Время работы BLE connected: 1200 мАч ÷ 12 мА = ~100 часов ≈ 4 суток**

### Риски Path B

| Риск | Вероятность | Mitigation |
|------|-------------|------------|
| NimBLE API изменился в Arduino 3.x | Средняя | Тестировать BLE до интеграции |
| RadioLib несовместим | Низкая | RadioLib 6.4+ поддерживает IDF 5.x |
| U8g2 несовместим | Низкая | API стабильный |
| Codec2 float — другое поведение FPU | Низкая | Тестировать голос |
| UNICORE снижает производительность | Средняя | Одно ядро хватает для всех задач |
| BLE disconnect при heavy LoRa TX | Средняя | PM lock на время TX |

### Файлы для изменения (Path B)

- `platformio.ini` — platform + custom_sdkconfig
- `main.cpp` — убрать PinnedToCore, добавить PM locks, setupPowerManagement()
- `lora_radio.cpp` — loraSend TX notification вместо polling
- `beacon.cpp` — location wait notification
- `ble_service.cpp` — проверить NimBLE 2.x API совместимость

### Тесты Path B

- [ ] Сборка с pioarduino без ошибок
- [ ] BLE подключение работает
- [ ] BLE сохраняется при light sleep (не отключается)
- [ ] PTT голос без артефактов
- [ ] Файловая передача работает
- [ ] Вызовы проходят
- [ ] Beacon отправляется
- [ ] Потребление idle BLE connected < 15 мА
- [ ] Потребление idle BLE disconnected < 3 мА
- [ ] Нет watchdog crash при длительной работе
- [ ] V3 и V4 собираются и работают

---

## 4. Порядок реализации

### Фаза 1: Path A — Manual Light Sleep (1-2 часа)

1. Добавить `esp_light_sleep_start()` в loraTask при BLE disconnected
2. Настроить GPIO0 + timer wakeup
3. Тестировать: потребление, beacon, BLE reconnect

### Фаза 2: Path B — pioarduino миграция (4-8 часов)

1. Переключить platform на pioarduino
2. Добавить custom_sdkconfig
3. Проверить компиляцию всех библиотек
4. Убрать PinnedToCore → xTaskCreate
5. Добавить setupPowerManagement()
6. Добавить PM locks (PTT, file transfer)
7. Устранить busy loops (loraSend, beacon)
8. Тестирование всех функций

### Фаза 3: Тюнинг (2-3 часа)

1. Оптимизация BLE connection parameters для sleep
2. Оптимизация beacon interval vs wake frequency
3. Замер реального потребления мультиметром
4. Полевые тесты на расстоянии

---

## 5. Текущие блокеры (busy loops в коде)

| Файл | Строка | Проблема | Решение |
|------|--------|----------|---------|
| `lora_radio.cpp` | 134 | `delay(1)` TX polling loop | Task notification от onTxDone ISR |
| `main.cpp` | 288 | `delay(200)` main loop | Увеличить или event-driven |
| `beacon.cpp` | 197 | Location polling 100мс | Task notification из BLE callback |
| `main.cpp` | 1030 | ACK wait polling 100мс | Допустимо (transient) |
| `repeater.cpp` | 121 | 1мс polling | Только repeater mode, не критично |

---

## 6. Совместимость с существующим кодом

### SPI (LoRa radio) через light sleep
- ✅ SPI peripheral state **сохраняется** через light sleep
- ✅ SX1262 **продолжает слушать** (external chip, свой power)
- ✅ DIO1 будит ESP32 при приёме пакета
- ✅ `radio.readData()` работает сразу после wake
- ✅ ISR `IRAM_ATTR` уже правильно

### OLED через light sleep
- ✅ I2C state сохраняется
- ✅ GPIO36 (Vext) сохраняет состояние

### Battery ADC через light sleep
- ✅ ADC state сохраняется
- ✅ GPIO37 control pin сохраняет состояние

### WiFi.mode(WIFI_OFF) + light sleep
- ✅ Полностью совместимо

---

## 7. Heltec V3/V4 особенности

### Heltec V3
- Нет внешнего 32.768 кГц кристалла → используем main XTAL для BLE sleep clock
- Main XTAL остаётся запитанным в light sleep → +0.5-1 мА
- USB через CP2102 (UART) → Serial работает через light sleep

### Heltec V4
- Тоже нет внешнего 32K кристалла
- USB CDC (native) → отключается при light sleep!
- PA (GC1109) нужно выключать перед sleep (уже реализовано)
- `-DNDEBUG` обязателен для V4 light sleep (USB CDC несовместим)
