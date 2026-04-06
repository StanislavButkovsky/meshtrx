# Задача: Файловый протокол v2 — буферизация на ESP32

> Статус: ПЛАН
> Приоритет: ВЫСОКИЙ
> Оценка: ~4-6 часов разработки
> Причина: текущий протокол теряет ~30% чанков, NACK/retry ненадёжен

---

## Проблема

Текущий протокол: телефон отправляет чанки по BLE → ESP32 немедленно ретранслирует по LoRa.
- Телефон не контролирует LoRa тайминги
- ~30% потерь чанков даже на близком расстоянии
- FILE_END/NACK ненадёжен
- Нет retry на уровне ESP32

## Решение

Файл сначала полностью загружается в RAM ESP32 (~230 КБ свободно), затем ESP32 сам управляет отправкой по LoRa с retry.

---

## Архитектура

### Отправитель (Sender)

```
Телефон                    ESP32 отправитель            LoRa
   │                            │                        │
   │── BLE: FILE_UPLOAD ──────>│                        │
   │   (заголовок + данные)     │ сохранить в RAM        │
   │                            │ busy = true            │
   │<── BLE: UPLOAD_ACK ──────│                        │
   │   (принято, начинаю)       │                        │
   │                            │── FILE_START ────────>│
   │                            │── CHUNK 0 ───────────>│
   │                            │── CHUNK 1 ───────────>│
   │                            │── ... ───────────────>│
   │                            │── FILE_END ──────────>│
   │                            │                        │
   │                            │<── ACK ──────────────│ всё получено
   │<── BLE: DELIVERED ───────│                        │
   │                            │ busy = false           │
   │                            │                        │
   │                            │<── NACK [5,12,99] ───│ пропуски
   │                            │── CHUNK 5 ───────────>│
   │                            │── CHUNK 12 ──────────>│
   │                            │── CHUNK 99 ──────────>│
   │                            │── FILE_END ──────────>│
   │                            │<── ACK ──────────────│
   │<── BLE: DELIVERED ───────│                        │
   │                            │ busy = false           │
```

### Приёмник (Receiver)

```
LoRa                      ESP32 приёмник               Телефон
  │                            │                        │
  │── FILE_START ────────────>│                        │
  │                            │ аллоцировать буфер     │
  │                            │ таймер 60 сек          │
  │── CHUNK 0 ───────────────>│ bitmap[0] = 1          │
  │── CHUNK 1 ───────────────>│ bitmap[1] = 1          │
  │── ... ───────────────────>│                        │
  │── FILE_END ──────────────>│                        │
  │                            │ проверить bitmap       │
  │                            │ все есть?              │
  │                            │  ├─ ДА: ACK           │
  │<── ACK ──────────────────│                        │
  │                            │── BLE: FILE_RECV ────>│
  │                            │   (заголовок + данные) │
  │                            │                        │
  │                            │  └─ НЕТ: NACK        │
  │<── NACK [5,12,99] ──────│                        │
  │── CHUNK 5 ───────────────>│ retry чанки            │
  │── CHUNK 12 ──────────────>│                        │
  │── CHUNK 99 ──────────────>│                        │
  │── FILE_END ──────────────>│                        │
  │                            │ проверить снова        │
  │                            │ ... (макс 3 NACK)      │
```

---

## Этапы реализации

### Этап 1: BLE протокол загрузки файла в ESP32

**Firmware (main.cpp, ble_service.h):**

Новые BLE команды:
```
0x30 = FILE_UPLOAD_START  (телефон→ESP): заголовок файла
       Формат: cmd(1) + file_type(1) + dest_mac(2) + total_size(4) + name(20) = 28 байт

0x31 = FILE_UPLOAD_DATA   (телефон→ESP): чанк данных файла
       Формат: cmd(1) + data(до 120 байт)

0x32 = FILE_UPLOAD_STATUS (ESP→телефон): статус загрузки/отправки
       Формат: cmd(1) + status(1) + progress(2)
       status: 0=ACCEPTED, 1=BUSY, 2=SENDING, 3=DELIVERED, 4=FAILED, 5=NO_MEMORY
```

**Android (MeshTRXService.kt):**
- `sendFile()` → загрузить файл в ESP32 чанками по BLE (120 байт, без LoRa)
- Ждать UPLOAD_STATUS = ACCEPTED
- Показать статус "Загрузка в модуль..."
- Ждать UPLOAD_STATUS = DELIVERED или FAILED (таймаут (настраиваемый, по умолчанию 60 сек))

**Задачи:**
- [ ] Добавить BLE команды 0x30, 0x31, 0x32 в ble_service.h
- [ ] Обработка FILE_UPLOAD_START в handleBleData — аллоцировать буфер
- [ ] Обработка FILE_UPLOAD_DATA — копировать в буфер
- [ ] Отправка FILE_UPLOAD_STATUS обратно телефону
- [ ] Android: переписать sendFile() на новый протокол
- [ ] Android: UI статус "Загрузка в модуль..." → "Отправка..." → "Доставлено"/"Ошибка"

### Этап 2: LoRa отправка из буфера ESP32

**Firmware (main.cpp):**

Новая FreeRTOS задача `fileSendTask`:
```cpp
void fileSendTask(void* param) {
    while (true) {
        // Ждать сигнал что файл загружен
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Отправить FILE_START (длинная преамбула)
        loraSendWake(FILE_START_packet);
        
        // Отправить все чанки с паузой 50мс
        for (int i = 0; i < totalChunks; i++) {
            loraSend(chunk[i]);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        // Отправить FILE_END
        loraSend(FILE_END_packet);
        
        // Ждать ACK/NACK (таймаут 5 сек)
        // При NACK — досылка пропущенных, макс 3 раунда
        // При ACK или таймаут (настраиваемый, по умолчанию 60 сек) — уведомить телефон
    }
}
```

**Задачи:**
- [ ] Создать fileSendTask (Core 0, приоритет 3)
- [ ] Буфер: `uint8_t* fileTxBuffer` + метаданные (size, chunks, dest, type, name)
- [ ] Флаг `fileTxBusy` — блокирует новые загрузки
- [ ] Отправка чанков из RAM с паузой 50мс
- [ ] FILE_END после последнего чанка
- [ ] Обработка ACK → DELIVERED → уведомить телефон
- [ ] Обработка NACK → досылка пропущенных чанков (макс 3 раунда)
- [ ] Таймаут 60 сек → FAILED → уведомить телефон, освободить буфер

### Этап 3: Приёмник — сборка и retry

**Firmware (main.cpp):**

Приёмник уже частично работает (bitmap, NACK). Доработать:

**Задачи:**
- [ ] Таймер 60 сек от первого чанка
- [ ] После FILE_END: если есть пропуски → NACK (макс 50 индексов)
- [ ] Макс 3 раунда NACK
- [ ] После ACK: передать файл телефону по BLE (как сейчас)
- [ ] При таймауте: освободить буфер, отбросить файл
- [ ] Не принимать новые FILE_START пока текущий не завершён

### Этап 4: Коллизии (приём + отправка одновременно)

**Правило: приём имеет приоритет.**

```
Состояние         │ Команда          │ Результат
──────────────────┼──────────────────┼────────────────────
IDLE              │ Upload от тел.   │ Принять, начать отправку
IDLE              │ FILE_START LoRa  │ Принять, начать приём
SENDING           │ Upload от тел.   │ Отклонить: BUSY
SENDING           │ FILE_START LoRa  │ Игнорировать (мы TX)
RECEIVING         │ Upload от тел.   │ Отклонить: BUSY
RECEIVING         │ FILE_START LoRa  │ Игнорировать (уже приём)
```

**Задачи:**
- [ ] Единый state: `FILE_STATE_IDLE / UPLOADING / SENDING / RECEIVING`
- [ ] Проверка state при FILE_UPLOAD_START → BUSY если не IDLE
- [ ] Проверка state при FILE_START LoRa → игнор если не IDLE
- [ ] Тесты коллизий

### Этап 5: Ограничения и защита

**Задачи:**
- [ ] Максимальный размер файла: 200 КБ (оставить 30 КБ для стека/heap)
- [ ] Проверка free heap перед аллокацией: `ESP.getFreeHeap() > fileSize + 30000`
- [ ] Отклонить файл если мало памяти: UPLOAD_STATUS = NO_MEMORY
- [ ] Освобождать буфер сразу после ACK/FAIL/таймаут
- [ ] Watchdog: не блокировать основной цикл дольше 100мс

---

## Настройки

Таймаут передачи файла — управляемый через настройки:

**Firmware (NVS):**
```cpp
// settings namespace
file_timeout_sec = 60  // по умолчанию 60 сек, диапазон 30-180
```

**Android (Settings JSON):**
```json
{"file_timeout": 60}
```

**Android UI (SettingsFragment):**
- Ползунок "Таймаут передачи файла" — 30/60/90/120/180 сек
- Подсказка: "Увеличьте при плохой связи или больших файлах"
- Отправляется в ESP32 через SET_SETTINGS JSON
- Сохраняется локально в SharedPreferences

**Задачи:**
- [ ] Firmware: загрузка `file_timeout_sec` из NVS в loadSettings()
- [ ] Firmware: обработка `file_timeout` в SET_SETTINGS JSON
- [ ] Android: SettingsFragment — ползунок/spinner таймаута
- [ ] Android: отправка в JSON при сохранении настроек
- [ ] Firmware: использовать значение вместо хардкода 60 сек

---

## Совместимость

- Старые BLE команды FILE_START (0x0D) / FILE_CHUNK (0x0E) — **удалить** после перехода
- LoRa пакеты FILE_START / FILE_CHUNK / FILE_END / FILE_ACK — **без изменений**
- Android: только MeshTRXService.sendFile() меняется, UI не трогаем

## Файлы для изменения

### Firmware:
- `ble_service.h` — новые команды 0x30-0x32
- `main.cpp` — handleBleData: FILE_UPLOAD_START/DATA, fileSendTask, state machine
- `lora_radio.h/cpp` — без изменений (loraSend/loraSendWake уже готовы)

### Android:
- `BleManager.kt` — новые CMD константы
- `MeshTRXService.kt` — переписать sendFile(), обработка UPLOAD_STATUS
- `FilesFragment.kt` — UI статусы (загрузка/отправка/доставлено)
- `VoiceFragment.kt` — PTT voice тоже через новый протокол (sendFile с type 0x05)
- `MessagesFragment.kt` — voice messages тоже (type 0x04)

---

## Метрики успеха

- [ ] Файл 25 КБ доставляется в 95%+ случаев (сейчас ~0%)
- [ ] Время доставки 25 КБ: < 45 сек (206 чанков × 50мс + retry)
- [ ] Таймаут корректно срабатывает на обоих сторонах
- [ ] Коллизия при одновременной отправке обрабатывается без crash
- [ ] Голосовые сообщения (до 4 КБ) доставляются за < 10 сек
