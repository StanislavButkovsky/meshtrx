# MeshTRX — Оптимизация энергопотребления

> Дата: 2026-04-04 (обновлено)
> Платформа: Heltec WiFi LoRa 32 V3/V4 (ESP32-S3 + SX1262)
> Firmware: RadioLib + NimBLE + FreeRTOS

---

## 1. Исходное энергопотребление (до оптимизаций)

| Компонент | Потребление | Примечание |
|-----------|------------|------------|
| ESP32-S3 CPU (240 МГц, busy polling) | 30-50 мА | Два ядра, polling loops 1мс/50мс |
| LoRa SX1262 в постоянном RX | 5-6 мА | `radio.startReceive()` непрерывно |
| BLE NimBLE (advertising) | 5-10 мА | Всегда, без остановки |
| BLE NimBLE (connected, 12мс interval) | 10-15 мА | Агрессивный conn interval |
| OLED (спящий, Vext запитан) | 1-5 мА | GPIO36 не отключался |
| Serial UART (idle) | 5-10 мА | 115200 baud, всегда включён |
| **Итого idle (BLE connected)** | **~70-130 мА** | |

---

## 2. Реализованные оптимизации

### 2.1 Event-driven LoRa task (экономия 15-30 мА) ✅

Заменён busy polling 1мс на `ulTaskNotifyTake()` — задача спит до прерывания DIO1:
```cpp
// ISR: будит loraTask через vTaskNotifyGiveFromISR()
// loraTask: ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50))
```
При PTT active — таймаут 5мс (для TX очереди), иначе 50мс.

Файлы: `main.cpp`, `lora_radio.cpp`

### 2.2 BLE tuning (экономия 7-12 мА) ✅

- Connection interval: 12мс/12мс → **60мс/100мс**
- Slave latency: 0 → **2** (пропускает 2 события)
- Supervision timeout: 200мс → **500мс**
- Advertising: scan response **отключён**, interval увеличен (100-200мс)
- TX power: P9 → **P6** (+6dBm вместо +9dBm)

Файл: `ble_service.cpp`

### 2.3 OLED Vext полное отключение (экономия 1-5 мА) ✅

При sleep: `digitalWrite(36, HIGH)` — полностью снимает питание с OLED через Vext.
При wake: `digitalWrite(36, LOW)` + delay 50мс перед инициализацией дисплея.
Таймаут: 30 секунд без активности → экран выключается.

Файл: `oled_display.cpp`

### 2.4 Serial conditional (экономия 5-10 мА) ✅

`Serial.begin()` обёрнут в `#ifndef NDEBUG`. Макросы в `debug.h`:
- `LOG_D(msg)` → `Serial.println(msg)` или `((void)0)`
- `LOG_F(fmt, ...)` → `Serial.printf(...)` или `((void)0)`

Для release: раскомментировать `-DNDEBUG` в `platformio.ini`.

Файлы: `debug.h`, `main.cpp`

### 2.5 Main loop delay (экономия 3-5 мА) ✅

`delay(50)` → `delay(200)` — кнопка реагирует 200мс (приемлемо).

Файл: `main.cpp`

### 2.6 LoRa sleep при отсутствии BLE (экономия ~5 мА) ✅

Когда BLE не подключен (и не ретранслятор):
- LoRa переходит в SLEEP (~0.001 мА)
- Beacon TX продолжает отправляться раз в 5 мин (радио пробуждается на ~50мс)
- При подключении BLE — мгновенный переход в continuous RX

Ретранслятор **исключён** — всегда в continuous RX.

Файлы: `main.cpp`, `lora_radio.cpp`

### 2.7 Длинная преамбула 32 символа ✅

Все пакеты отправляются с преамбулой 32 (было 8). Добавляет ~12мс к airtime — пренебрежимо.
Обеспечивает надёжный приём любым устройством, включая те что используют duty cycle RX.

```cpp
#define LORA_PREAMBLE  32  // было 8
```

Файл: `lora_radio.h`

### 2.8 GC1109 PA управление для V4 (экономия ~5-10 мА) ✅

На V4 с GC1109 PA добавлено полное управление питанием PA через GPIO7:

| Состояние | GPIO7 (power) | GPIO2 (enable) | GPIO46 (CTX) | Потребление PA |
|-----------|:---:|:---:|:---:|---|
| **RX** (BLE connected) | HIGH | HIGH | LOW | ~5-10 мА |
| **TX** | HIGH | HIGH | HIGH | ~200-400 мА |
| **SLEEP** (BLE off) | LOW | LOW | LOW | ~0 мА |
| **Beacon TX** (из sleep) | HIGH→LOW | HIGH→LOW | LOW→HIGH→LOW | кратковременно |

```cpp
// lora_radio.h
#define PA_FEM_POWER 7   // питание PA
#define PA_FEM_EN    2   // enable (CSD)
#define PA_FEM_CTX   46  // TX/RX switch (CTX)

// lora_radio.cpp
void loraPaEnable()  { GPIO7=HIGH, GPIO2=HIGH, GPIO46=LOW }
void loraPaDisable() { GPIO46=LOW, GPIO2=LOW, GPIO7=LOW }
```

PA автоматически:
- Включается при переходе в CONTINUOUS_RX
- Выключается при переходе в SLEEP
- Временно включается для beacon TX из sleep

Файлы: `lora_radio.h`, `lora_radio.cpp`

---

## 3. State machine управления питанием LoRa

```
┌─────────────────────────────────┐
│  BLE DISCONNECTED               │
│  (не ретранслятор)              │
│                                 │
│  LoRa: SLEEP (~0 мА)           │
│  PA: OFF (V4)                   │
│  Beacon TX: раз в 5 мин        │
│  loraTask: спит 1 сек           │
└────────────┬────────────────────┘
             │ BLE connected
             ▼
┌─────────────────────────────────┐
│  BLE CONNECTED                  │
│                                 │
│  LoRa: CONTINUOUS RX (~5 мА)   │
│  PA: ON (V4)                    │
│  Полный приём/передача          │
└────────────┬────────────────────┘
             │ BLE disconnected
             ▼
             └──→ Обратно в SLEEP

┌─────────────────────────────────┐
│  РЕТРАНСЛЯТОР                   │
│                                 │
│  LoRa: CONTINUOUS RX (всегда)  │
│  PA: ON (V4, всегда)           │
│  WiFi AP: включён               │
│  Не зависит от BLE              │
└─────────────────────────────────┘
```

---

## 4. Итоговое энергопотребление

### V3 (без PA)

| Режим | Потребление | Время от 1200 мАч |
|-------|------------|-------------------|
| BLE connected, idle | ~20-28 мА | **43-60 часов** |
| BLE connected, голос | ~80-120 мА | ~10-15 часов |
| BLE disconnected (beacon-only) | ~6-10 мА | **120-200 часов** |
| Ретранслятор idle | ~25-35 мА | ~34-48 часов |

### V4 (с GC1109 PA)

| Режим | Потребление | Время от 1200 мАч |
|-------|------------|-------------------|
| BLE connected, idle (PA on) | ~25-38 мА | **32-48 часов** |
| BLE connected, TX 28dBm | ~300-400 мА | ~3-4 часа непрерывно |
| BLE disconnected (PA off) | ~6-10 мА | **120-200 часов** |
| Ретранслятор idle (PA on) | ~30-45 мА | ~27-40 часов |

### Сравнение с исходным

| Метрика | До оптимизации | После | Улучшение |
|---------|---------------|-------|-----------|
| Idle (BLE connected) | ~70-130 мА | ~20-28 мА | **4-5x** |
| BLE disconnected | ~70-130 мА (всё включено) | ~6-10 мА | **10-15x** |
| Время idle от 1200мАч | ~9-17 часов | ~43-60 часов | **3-4x** |

---

## 5. Что НЕ реализовано (оставлено на будущее)

### 5.1 LoRa RX Duty Cycle (аппаратный SX1262)

API реализован (`loraSetPowerMode(LORA_POWER_DUTY_CYCLE_RX)`), но **не активирован**.
Причина: все пакеты используют единую преамбулу 32, duty cycle не даёт дополнительной экономии
при текущей архитектуре (BLE disconnected → sleep, BLE connected → continuous RX).

Может быть полезен в будущем для режима "автономный ретранслятор без WiFi" —
слушать LoRa с duty cycle вместо continuous RX.

### 5.2 ESP32-S3 Light Sleep

ESP32-S3 может входить в light sleep между задачами (~0.8 мА CPU).
Требует изменения архитектуры FreeRTOS задач — tickless idle mode.
Потенциальная экономия: ~5-10 мА дополнительно.

### 5.3 NDEBUG release сборка

`-DNDEBUG` в platformio.ini **закомментирован** для удобства отладки.
Раскомментировать для production: экономия ~5-10 мА (отключение Serial UART).

### 5.4 Dynamic TX power

Снижать мощность TX при хорошем RSSI (близкие устройства).
Потенциальная экономия при TX: 50-200 мА на каждом пакете.

---

## 6. Файлы прошивки (затронутые оптимизацией)

| Файл | Изменения |
|------|-----------|
| `debug.h` | Новый — макросы LOG_D/LOG_F с NDEBUG |
| `main.cpp` | Event-driven loraTask, BLE sleep logic, Serial conditional |
| `lora_radio.h` | PA_FEM_POWER pin, LoRaPowerMode enum, loraPaEnable/Disable |
| `lora_radio.cpp` | Task notification ISR, power mode switching, PA management |
| `ble_service.cpp` | Conn interval, advertising, TX power |
| `oled_display.cpp` | Vext GPIO36 full power cut |
| `beacon.cpp` | loraSendWake() для beacon |
| `call_manager.cpp` | loraSendWake() для вызовов |
