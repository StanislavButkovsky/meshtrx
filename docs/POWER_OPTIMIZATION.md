# MeshTRX — Исследование оптимизации энергопотребления

> Дата: 2026-04-03
> Платформа: Heltec WiFi LoRa 32 V3/V4 (ESP32-S3 + SX1262)
> Firmware: RadioLib + NimBLE + FreeRTOS

---

## 1. Текущее энергопотребление (idle, без оптимизаций)

| Компонент | Потребление | Примечание |
|-----------|------------|------------|
| ESP32-S3 CPU (240 МГц, busy polling) | 30-50 мА | Два ядра, polling loops 1мс/50мс |
| LoRa SX1262 в постоянном RX | 5-6 мА | `radio.startReceive()` непрерывно |
| BLE NimBLE (advertising) | 5-10 мА | Всегда, без остановки |
| BLE NimBLE (connected, 12мс interval) | 10-15 мА | Агрессивный conn interval |
| OLED (спящий, Vext запитан) | 1-5 мА | GPIO36 не отключается |
| Serial UART (idle) | 5-10 мА | 115200 baud, всегда включён |
| Polling overhead (CPU wake) | 15-35 мА | loraTask 1мс, main loop 50мс |
| **Итого idle** | **~70-130 мА** | |

### Режимы работы

| Режим | Потребление |
|-------|------------|
| Idle (BLE connected, LoRa RX) | ~70-130 мА |
| Активный голос (TX) | +100-150 мА (LoRa TX 22 dBm) |
| Активный голос (RX) | ~80-140 мА |
| Repeater (WiFi AP) | ~190-320 мА |
| Deep sleep ESP32-S3 | <20 мкА |

---

## 2. Безопасные оптимизации (без потери пакетов)

### 2.1 Event-driven LoRa task (экономия 15-30 мА)

**Проблема**: `loraTaskFunc` опрашивает флаг каждые 1мс:
```cpp
while (true) {
    if (loraRxFlag) { ... }
    vTaskDelay(pdMS_TO_TICKS(1));  // busy wait
}
```

**Решение**: Использовать `ulTaskNotifyTake()` — задача спит пока DIO1 не разбудит:
```cpp
while (true) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000)); // спит до прерывания
    if (loraRxFlag) { ... }
}
```

Файл: `firmware/src/main.cpp`, loraTaskFunc (строка ~361)

### 2.2 BLE connection interval (экономия 5-10 мА)

**Проблема**: `updateConnParams(handle, 12, 12, 0, 200)` — 83 события/сек.

**Решение**: Увеличить interval, добавить slave latency:
```cpp
updateConnParams(handle, 80, 160, 4, 1000);
// min=100мс, max=200мс, latency=4 события, timeout=1000мс
```

Файл: `firmware/src/ble_service.cpp`, строка ~18

### 2.3 BLE advertising interval (экономия 2-3 мА)

**Текущее**: NimBLE defaults (~100мс).

**Решение**: Увеличить до 300-500мс:
```cpp
pAdvertising->setMinPreferred(240);  // 150мс
pAdvertising->setMaxPreferred(480);  // 300мс
```

### 2.4 Serial отключение в release (экономия 5-10 мА)

Добавить в platformio.ini:
```ini
build_flags = -DNDEBUG -DCORE_DEBUG_LEVEL=0
```
Или макрос `#ifdef NDEBUG` вокруг `Serial.begin()`.

### 2.5 OLED полное отключение Vext (экономия 1-5 мА)

**Проблема**: `u8g2.setPowerSave(1)` усыпляет дисплей, но Vext GPIO36 остаётся LOW (питание подано).

**Решение**: 
```cpp
void oledOff() {
    u8g2.setPowerSave(1);
    delay(50);
    digitalWrite(36, HIGH);  // полностью отключить Vext
}
```

### 2.6 Main loop увеличить delay (экономия 3-5 мА)

`delay(50)` → `delay(200)` — кнопка реагирует 200мс (приемлемо).

### Итого безопасных: ~30-60 мА экономии

---

## 3. LoRa RX Duty Cycle (продвинутая оптимизация)

### 3.1 Проблема

LoRa SX1262 в постоянном RX потребляет ~5-6 мА непрерывно.
Это ~50% от idle потребления.

### 3.2 SX1262 RX Duty Cycle (аппаратный режим)

SX1262 имеет встроенный режим RX Duty Cycle:
- Чип автоматически чередует RX окна и sleep
- При обнаружении преамбулы — остаётся в RX и принимает пакет
- MCU не участвует в переключениях

RadioLib API:
```cpp
radio.startReceiveDutyCycleAuto(
    senderPreambleLength,  // длина преамбулы отправителя
    minSymbols             // мин. символов для обнаружения
);
```

### 3.3 Требование: длинная преамбула

Формула: `sleepSymbols = preambleLength - 2 * minSymbols`

С текущей преамбулой 8 и minSymbols=8: `8 - 16 = -8` — **не работает** (RadioLib откатит на постоянный RX).

Нужно **увеличить преамбулу отправителя** для пакетов, которые будят спящий приёмник:

| Преамбула | Sleep символов | Duty cycle | Ток (средний) |
|-----------|---------------|------------|--------------|
| 8 (текущая) | — | 100% (постоянный RX) | ~5 мА |
| 32 | 16 (8.2мс) | ~36% | ~1.8 мА |
| 64 | 48 (24.6мс) | ~16% | ~0.8 мА |

Дополнительный airtime: преамбула 32 добавляет ~12мс к пакету (при SF7/BW250). Пренебрежимо.

### 3.4 CAD (Channel Activity Detection) — альтернатива

SX1262 CAD: радио сканирует канал ~1.5мс, ищет LoRa chirp-энергию.

```cpp
radio.startChannelScan();  // запуск CAD
// DIO1 → ISR
int result = radio.getChannelScanResult();
// RADIOLIB_LORA_DETECTED или RADIOLIB_CHANNEL_FREE
```

Цикл CAD-sleep-CAD при 20мс интервале: **~0.4 мА** — максимальная экономия.

Минусы CAD vs RX Duty Cycle:
- Ручное переключение CAD → RX (задержка ~2-5мс, риск потери начала пакета)
- RadioLib хардкодит CAD_GOTO_STDBY (нет автоперехода в RX)
- Более сложная state machine

### 3.5 Рекомендуемая гибридная стратегия

```
IDLE (нет активного вызова/файла):
  → RX Duty Cycle с длинной преамбулой (32 символа)
  → Потребление ~1.8 мА
  → "Будящие" пакеты: вызовы, beacons, первый текст — шлются с длинной преамбулой

ACTIVE (голос, файл, вызов активен):
  → Постоянный RX (startReceive, как сейчас)
  → Потребление ~5 мА
  → Голосовые пакеты с короткой преамбулой (8) для минимального airtime

Конец вызова / таймаут:
  → Обратно в RX Duty Cycle
```

State machine:
```
┌───────────────────┐
│  IDLE (Low Power)  │  startReceiveDutyCycleAuto(32, 8)
│  RX Duty Cycle     │  ~1.8 мА
└────────┬──────────┘
         │ DIO1: пакет принят
         ▼
┌───────────────────┐
│  ACTIVE RX         │  startReceive() — постоянный RX
│  Continuous RX     │  ~5 мА
└────────┬──────────┘
         │ Конец вызова / таймаут 10с без пакетов
         ▼
         └──→ Обратно в IDLE
```

### 3.6 Пакеты с длинной преамбулой (для пробуждения)

| Тип пакета | Преамбула | Причина |
|------------|-----------|---------|
| Beacon (PKT_TYPE_BEACON) | 32 | Будит спящих для обнаружения peer |
| Call ALL/PRIVATE/GROUP | 32 | Будит адресата для вызова |
| Call EMERGENCY | 32 | Будит всех |
| Text message (первый) | 32 | Будит адресата |
| File header (FILE_START) | 32 | Будит адресата |

| Тип пакета | Преамбула | Причина |
|------------|-----------|---------|
| Audio (CMD_AUDIO_RX) | 8 | Все уже в continuous RX |
| File chunks | 8 | Все уже в continuous RX |
| Text ACK | 8 | Отправитель в continuous RX |
| Call ACCEPT/REJECT | 8 | Отправитель в continuous RX |

### 3.7 Реализация в коде

```cpp
// lora_radio.h
#define LORA_PREAMBLE_SHORT  8    // для голоса/файлов (active mode)
#define LORA_PREAMBLE_LONG   32   // для будящих пакетов (idle wake)

enum LoRaPowerMode {
    LORA_POWER_CONTINUOUS_RX,   // постоянный RX (active)
    LORA_POWER_DUTY_CYCLE_RX,   // duty cycle RX (idle)
};

void loraSetPowerMode(LoRaPowerMode mode);
LoRaPowerMode loraGetPowerMode();
bool loraSendWakePacket(uint8_t* data, size_t len);  // с длинной преамбулой
```

```cpp
// lora_radio.cpp
void loraSetPowerMode(LoRaPowerMode mode) {
    if (mode == LORA_POWER_DUTY_CYCLE_RX) {
        radio.setPreambleLength(LORA_PREAMBLE_LONG);
        int state = radio.startReceiveDutyCycleAuto(LORA_PREAMBLE_LONG, 8);
        if (state != RADIOLIB_ERR_NONE) {
            // fallback на постоянный RX
            radio.startReceive();
        }
    } else {
        radio.setPreambleLength(LORA_PREAMBLE_SHORT);
        radio.startReceive();
    }
    currentPowerMode = mode;
}

bool loraSendWakePacket(uint8_t* data, size_t len) {
    radio.setPreambleLength(LORA_PREAMBLE_LONG);
    bool ok = loraSend(data, len);
    // Восстановить преамбулу по текущему режиму
    radio.setPreambleLength(
        currentPowerMode == LORA_POWER_DUTY_CYCLE_RX ? LORA_PREAMBLE_LONG : LORA_PREAMBLE_SHORT
    );
    return ok;
}
```

---

## 4. Итоговая таблица потребления

### После всех оптимизаций

| Компонент | Сейчас | После | Экономия |
|-----------|--------|-------|----------|
| LoRa RX (idle, duty cycle) | 5 мА | 1.8 мА | 3.2 мА |
| LoRa RX (active, continuous) | 5 мА | 5 мА | — |
| CPU (event-driven) | 30-50 мА | 5-10 мА | 25-40 мА |
| BLE (connected) | 10-15 мА | 3-5 мА | 7-10 мА |
| Serial (disabled) | 5-10 мА | 0 | 5-10 мА |
| OLED (Vext off) | 1-5 мА | 0 | 1-5 мА |
| **Итого idle** | **~70-130 мА** | **~10-20 мА** | **~80-85%** |
| **Итого active voice** | **~80-140 мА** | **~50-80 мА** | **~35-40%** |

### Время работы от батареи (LiPo 1000 мАч)

| Режим | Сейчас | После оптимизации |
|-------|--------|-------------------|
| Idle (ожидание) | ~8-14 часов | ~50-100 часов |
| Активный голос 50% | ~5-8 часов | ~15-25 часов |
| Deep sleep | ~50000 часов | ~50000 часов |

---

## 5. Приоритеты реализации

1. **Event-driven LoRa task** — наибольший эффект, простая замена polling на interrupt wait
2. **BLE conn interval / latency** — одна строка кода
3. **OLED Vext отключение** — одна строка кода
4. **Serial отключение** — флаг в platformio.ini
5. **LoRa RX Duty Cycle** — требует state machine, изменение преамбулы для будящих пакетов
6. **Main loop delay** — тривиальное изменение

---

## 6. Особенности для Heltec V4

### Отличия V4 от V3

| Параметр | V3 | V4 |
|----------|----|----|
| USB | CP2102 (UART bridge) | Native ESP32-S3 USB (CDC) |
| Flash | 8 MB integrated | 16 MB external |
| PA | Нет | GC1109 / KCT8103L (опционально) |
| Max TX power | 21 dBm | 28 dBm (с PA) |
| Solar | Нет | SH1.25-2P, 4.7-6V |
| GNSS | Нет | Разъём SH1.25-8P |
| OLED driver | SSD1306 | SSD1315 (API-совместим) |
| Pin count | 36 | 40 |

### PA энергопотребление (V4 с GC1109)

GC1109 PA добавляет:
- TX: +200-400 мА (в зависимости от выходной мощности)
- RX (LNA): +5-10 мА
- Sleep: <1 мкА (при `LORA_PA_POWER` GPIO7 = LOW)

**Для экономии энергии на V4:**
- Отключать PA через `GPIO7 = LOW` когда не нужна высокая мощность
- Использовать SX1262 direct output (21 dBm) для коротких дистанций
- Включать PA только для дальней связи

### Пины PA на V4

```cpp
#define LORA_PA_POWER  7    // Питание PA (HIGH = вкл)
#define LORA_PA_EN     2    // Enable / CSD
#define LORA_PA_TX_EN  46   // TX/RX переключатель (HIGH = TX)
```

### Upload на V4

V4 использует native USB (без CP2102):
```ini
# platformio.ini для V4
upload_protocol = esptool
upload_speed = 921600
# Не нужен 1200-baud reset
```
