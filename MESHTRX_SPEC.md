# MeshTRX — Полная спецификация проекта
> Цель: реализовать весь проект с нуля по этому документу.
> Проект: **MeshTRX** — децентрализованная голосовая mesh-сеть на LoRa + BLE.
> Домен: **meshtrx.com** (зарегистрирован)
> Android пакет: **com.meshtrx.app**
> BLE имя устройства: **MeshTRX-XXXX**
> GitHub: **github.com/StanislavButkovsky/meshtrx**

---

## 1. Обзор проекта

Система двусторонней голосовой связи и обмена данными на основе LoRa-радио.  
Два устройства Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262) связываются по LoRa на расстоянии до 5+ км.  
Каждое устройство подключается к смартфону Android по Bluetooth BLE.  

Функционал: PTT голос (Codec2 3200 bps) · текстовые сообщения · передача файлов и фото-миниатюр · 23 канала · управление мощностью 1–22 дБм · настройки duty cycle через приложение · режим ретранслятора с WiFi мониторингом.

### Схема системы

```
[Телефон A] <--BLE--> [Heltec V3 A] <--LoRa 868МГц--> [Heltec V3 B] <--BLE--> [Телефон B]
```

---

## 2. Аппаратная часть

### Устройство: Heltec WiFi LoRa 32 V3
- MCU: ESP32-S3FN8 (WiFi + Bluetooth 5.0 BLE)
- LoRa: Semtech SX1262
- Дисплей: OLED 0.96" (128×64, I2C)
- USB: CP2102 USB-UART (для прошивки и отладки)
- Частоты LoRa: 863–870 МГц (EU диапазон)
- Питание: USB-C или LiPo аккумулятор

### Пины (из официальной схемы HTIT-WB32LA(F)_V3)
```
LoRa SX1262:
  SCK   = GPIO9
  MISO  = GPIO11
  MOSI  = GPIO10
  NSS   = GPIO8   (CS)
  RST   = GPIO12
  DIO1  = GPIO14
  BUSY  = GPIO13

OLED (I2C):
  SDA   = GPIO17
  SCL   = GPIO18
  RST   = GPIO21

Прочее:
  LED   = GPIO35
  USER BUTTON = GPIO0
  VBAT ADC    = GPIO1 (через делитель)
```

---

## 3. Структура проекта

```
meshtrx/
├── MESHTRX_SPEC.md      # этот файл
├── firmware/                       # прошивка ESP32
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp
│       ├── ble_service.h / .cpp
│       ├── lora_radio.h / .cpp
│       ├── audio_codec.h / .cpp    # Codec2 обёртка
│       ├── oled_display.h / .cpp
│       └── packet.h                # формат пакета
├── android/                        # Android приложение
│   └── MeshTRX/
│       ├── build.gradle (project)
│       ├── app/
│       │   ├── build.gradle
│       │   └── src/main/
│       │       ├── AndroidManifest.xml
│       │       ├── java/com/meshtrx/app/
│       │       │   ├── SplashActivity.kt
│       │       │   ├── MainActivity.kt
│       │       │   ├── BleManager.kt
│       │       │   ├── AudioEngine.kt
│       │       │   ├── Codec2Wrapper.kt
│       │       │   ├── LocationHelper.kt
│       │       │   ├── model/
│       │       │   │   └── Models.kt
│       │       │   └── ui/
│       │       │       ├── VoiceFragment.kt
│       │       │       ├── MessagesFragment.kt
│       │       │       ├── FilesFragment.kt
│       │       │       ├── MapFragment.kt
│       │       │       ├── SettingsFragment.kt
│       │       │       ├── RadarView.kt
│       │       │       ├── PttButtonView.kt
│       │       │       └── CallPickerSheet.kt
│       │       ├── res/layout/
│       │       │   └── activity_main.xml
│       │       └── jni/             # Codec2 нативная библиотека
│       │           ├── CMakeLists.txt
│       │           └── codec2_jni.cpp
├── web/                           # публичный сайт (Next.js)
│   └── src/
│       ├── app/                   # страницы: /, /download, /flash, /docs, /about
│       ├── components/            # React компоненты
│       └── lib/                   # константы, i18n, flash утилиты
└── scripts/
    ├── install_deps.sh              # установка всех зависимостей
    ├── flash_firmware.sh            # заливка прошивки на ESP32
    └── build_android.sh             # сборка APK
```

---

## 4. Прошивка ESP32 (PlatformIO + Arduino framework)

### 4.1 platformio.ini
```ini
[env:heltec_wifi_lora_32_V3]
platform = espressif32
board = heltec_wifi_lora_32_V3
framework = arduino
monitor_speed = 115200
upload_speed = 921600

lib_deps =
  jgromes/RadioLib @ ^6.4.0
  h2zero/NimBLE-Arduino @ ^1.4.0
  olikraus/U8g2 @ ^2.35.0

build_flags =
  -DARDUINO_USB_MODE=1
  -DARDUINO_USB_CDC_ON_BOOT=1
```

### 4.2 Формат пакета LoRa (packet.h)
```cpp
// Максимальный размер пакета LoRa SF7 BW250: 222 байта (то же что BW500)
// Два типа пакетов:

// --- Аудио пакет (71 байт) ---
#pragma pack(push, 1)
struct LoRaAudioPacket {
  uint8_t  type;        // байт 0: 0xA0 = аудио
  uint8_t  channel;     // байт 1: номер канала (0–22)
  uint8_t  seq;         // байт 2: порядковый номер
  uint8_t  flags;       // байт 3: бит0=PTT_START, бит1=PTT_END, бит2=ROGER_BEEP, бит3=VOX
  uint8_t  ttl;         // байт 4: Time-To-Live (по умолчанию 2; ретранслятор -1; при 0 — не ретранслировать)
  uint8_t  sender[2];   // байты 5-6: последние 2 байта MAC (идентификатор)
  uint8_t  payload[64]; // байты 7–70: Codec2 3200 bps (8 фреймов × 8 байт)
};
#pragma pack(pop)
// Итого LoRaAudioPacket: 7 (заголовок) + 64 (payload) = 71 байт

// --- Текстовый пакет (макс. 91 байт) ---
#pragma pack(push, 1)
struct LoRaTextPacket {
  uint8_t  type;        // байт 0: 0xB0 = текст
  uint8_t  channel;     // байт 1: номер канала (0–22)
  uint8_t  seq;         // байт 2: порядковый номер (для ACK)
  uint8_t  ttl;         // байт 3: Time-To-Live
  uint8_t  sender[2];   // байты 4-5: последние 2 байта MAC
  uint8_t  text[85];    // байты 6–90: UTF-8 текст, null-terminated
};
#pragma pack(pop)

// --- Файловый пакет — заголовок передачи (старт сессии) ---
#pragma pack(push, 1)
struct LoRaFileHeader {
  uint8_t  type;         // байт 0: 0xC0 = FILE_START
  uint8_t  channel;      // байт 1: канал
  uint8_t  session_id;   // байт 2: ID сессии
  uint8_t  ttl;          // байт 3: Time-To-Live
  uint8_t  sender[2];    // байты 4-5: MAC
  uint8_t  file_type;    // байт 6: 0x01=фото, 0x02=текст, 0x03=бинарный
  uint16_t total_chunks; // байты 7-8: общее число чанков
  uint32_t total_size;   // байты 9-12: полный размер в байтах
  uint8_t  name[20];     // байты 13-32: имя файла, null-terminated
};
#pragma pack(pop)

// --- Файловый пакет — чанк данных ---
#pragma pack(push, 1)
struct LoRaFileChunk {
  uint8_t  type;         // байт 0: 0xC1 = FILE_CHUNK
  uint8_t  channel;      // байт 1: канал
  uint8_t  session_id;   // байт 2: ID сессии
  uint8_t  ttl;          // байт 3: Time-To-Live
  uint16_t chunk_index;  // байты 4-5: номер чанка (0-based)
  uint8_t  data[200];    // байты 6-205: данные чанка
};
#pragma pack(pop)

// --- Файловый пакет — ACK / NACK (не ретранслируется, TTL=0 всегда) ---
#pragma pack(push, 1)
struct LoRaFileAck {
  uint8_t  type;         // байт 0: 0xC2 = FILE_ACK
  uint8_t  session_id;   // байт 1: ID сессии
  uint8_t  status;       // байт 2: 0x00=OK, 0x01=NACK
  uint16_t chunk_index;  // байты 3-4: номер чанка
};
#pragma pack(pop)

// --- Файловый пакет — конец передачи ---
#pragma pack(push, 1)
struct LoRaFileEnd {
  uint8_t  type;         // байт 0: 0xC3 = FILE_END
  uint8_t  session_id;   // байт 1: ID сессии
  uint8_t  ttl;          // байт 2: Time-To-Live
  uint16_t crc16;        // байты 3-4: CRC16 всего файла
};
#pragma pack(pop)

#define PKT_TYPE_AUDIO      0xA0
#define PKT_TYPE_TEXT       0xB0
#define PKT_TYPE_FILE_START 0xC0
#define PKT_TYPE_FILE_CHUNK 0xC1
#define PKT_TYPE_FILE_ACK   0xC2
#define PKT_TYPE_FILE_END   0xC3
#define PKT_TYPE_BEACON     0xD0
#define PKT_FLAG_PTT_START  0x01
#define PKT_FLAG_PTT_END    0x02
#define PKT_FLAG_ROGER_BEEP 0x04
#define PKT_FLAG_VOX        0x08

#define FILE_TYPE_PHOTO     0x01
#define FILE_TYPE_TEXT      0x02
#define FILE_TYPE_BINARY    0x03
#define CHUNK_SIZE          120

// TTL значения:
#define TTL_DEFAULT         2    // обычный пакет — пройдёт через 2 ретранслятора
#define TTL_MAX             4    // максимум (цепочка 4 ретранслятора)
#define TTL_NO_RELAY        0    // не ретранслировать (ACK, локальные команды)

// Режим ретранслятора (сохраняется в NVS):
// #define REPEATER_MODE  — если true, устройство работает как Store&Forward ретранслятор

// --- Beacon / пинг присутствия (38 байт) ---
// Отправляется периодически всеми устройствами в эфир
#pragma pack(push, 1)
struct LoRaBeaconPacket {
  uint8_t  type;          // байт 0:    0xD0 = BEACON
  uint8_t  channel;       // байт 1:    текущий канал (0–22)
  uint8_t  device_id[4];  // байты 2-5: уникальный ID (последние 4 байта MAC)
  uint8_t  call_sign[9];  // байты 6-14: позывной/имя, null-terminated, макс. 8 символов
  int32_t  lat_e7;        // байты 15-18: широта × 1e7 (напр. 59.4370000 → 594370000)
  int32_t  lon_e7;        // байты 19-22: долгота × 1e7 (напр. 24.7453600 → 247453600)
  int16_t  altitude_m;    // байты 23-24: высота в метрах (или 0x7FFF если нет GPS)
  uint8_t  tx_power;      // байт 25:   текущая мощность TX в дБм
  uint8_t  battery;       // байт 26:   уровень батареи 0–100% (или 0xFF если USB)
  uint8_t  flags;         // байт 27:   бит0=GPS_VALID, бит1=VOX_ON, бит2=BUSY(передаёт)
  uint32_t uptime_sec;    // байты 28-31: время работы устройства в секундах
  uint16_t beacon_seq;    // байты 32-33: порядковый номер пинга (для дедупликации)
  uint16_t crc16;         // байты 34-35: CRC16 пакета
};
#pragma pack(pop)
// Итого: 36 байт — компактно, передаётся ~за 5 мс при SF7 BW250

#define BEACON_FLAG_GPS_VALID  0x01  // координаты актуальны
#define BEACON_FLAG_VOX_ON     0x02  // VOX режим активен
#define BEACON_FLAG_BUSY       0x04  // устройство сейчас передаёт голос/файл
#define BEACON_FLAG_REPEATER   0x08  // устройство работает как ретранслятор

// Интервалы пинга (секунды):
#define BEACON_INTERVAL_1MIN   60
#define BEACON_INTERVAL_3MIN   180
#define BEACON_INTERVAL_5MIN   300   // по умолчанию
#define BEACON_INTERVAL_15MIN  900
#define BEACON_INTERVAL_30MIN  1800
#define BEACON_INTERVAL_1HOUR  3600
#define BEACON_INTERVAL_NEVER  0     // отключён

// ═══════════════════════════════════════════════════════════
// --- Система вызовов (Call System) ---
// ═══════════════════════════════════════════════════════════

// --- ALL CALL — общий вызов всем на канале (32 байт) ---
#pragma pack(push, 1)
struct LoRaCallAll {
  uint8_t  type;          // байт 0: 0xE0 = CALL_ALL
  uint8_t  channel;       // байт 1: канал
  uint8_t  ttl;           // байт 2: TTL (по умолчанию 2)
  uint8_t  sender[4];     // байты 3-6: device_id отправителя
  uint8_t  call_sign[9];  // байты 7-15: позывной, null-terminated
  int32_t  lat_e7;        // байты 16-19: широта × 1e7 (0 если нет GPS)
  int32_t  lon_e7;        // байты 20-23: долгота × 1e7 (0 если нет GPS)
  uint8_t  message[8];    // байты 24-31: короткое сообщение вызова, null-terminated
};                        // Пример message: "На связь!" или пусто
#pragma pack(pop)

// --- PRIVATE CALL — личный вызов конкретному устройству (36 байт) ---
#pragma pack(push, 1)
struct LoRaCallPrivate {
  uint8_t  type;          // байт 0: 0xE1 = CALL_PRIVATE
  uint8_t  channel;       // байт 1: канал
  uint8_t  ttl;           // байт 2: TTL
  uint8_t  sender[4];     // байты 3-6: device_id звонящего
  uint8_t  target[4];     // байты 7-10: device_id адресата
  uint8_t  call_sign[9];  // байты 11-19: позывной звонящего
  uint8_t  call_seq;      // байт 20: номер вызова (для ACCEPT/REJECT)
  int32_t  lat_e7;        // байты 21-24: широта × 1e7 (0 если нет GPS)
  int32_t  lon_e7;        // байты 25-28: долгота × 1e7 (0 если нет GPS)
  uint8_t  message[7];    // байты 29-35: короткое сообщение, null-terminated
};
#pragma pack(pop)

// --- GROUP CALL — вызов группе (20 байт + маска) ---
#pragma pack(push, 1)
struct LoRaCallGroup {
  uint8_t  type;          // байт 0: 0xE2 = CALL_GROUP
  uint8_t  channel;       // байт 1: канал
  uint8_t  ttl;           // байт 2: TTL
  uint8_t  sender[4];     // байты 3-6: device_id
  uint16_t group_id;      // байты 7-8: ID группы (0xFFFF = ad-hoc)
  uint8_t  member_count;  // байт 9: число участников (1–8)
  uint8_t  members[4][4]; // байты 10-41: device_id каждого участника (макс. 8 × 4 байта)
  uint8_t  call_sign[9];  // байты 42-50: позывной звонящего
  uint8_t  group_name[9]; // байты 51-59: имя группы, null-terminated
};
#pragma pack(pop)

// --- EMERGENCY / SOS — тревожный вызов (30 байт) ---
#pragma pack(push, 1)
struct LoRaCallEmergency {
  uint8_t  type;          // байт 0: 0xE3 = CALL_EMERGENCY
  uint8_t  channel;       // байт 1: канал (ретранслируется на все каналы!)
  uint8_t  ttl;           // байт 2: всегда TTL_MAX=4
  uint8_t  sender[4];     // байты 3-6: device_id
  uint8_t  call_sign[9];  // байты 7-15: позывной
  int32_t  lat_e7;        // байты 16-19: координаты (0 если нет GPS)
  int32_t  lon_e7;        // байты 20-23: координаты
  uint8_t  sos_seq;       // байт 24: номер SOS (для дедупликации повторов)
  uint8_t  flags;         // байт 25: бит0=GPS_VALID, бит1=REPEAT(автоповтор)
  uint8_t  message[4];    // байты 26-29: "SOS!" или короткий текст
};
#pragma pack(pop)

// --- Ответы на вызов (8 байт) ---
#pragma pack(push, 1)
struct LoRaCallResponse {
  uint8_t  type;          // байт 0: 0xE4=ACCEPT, 0xE5=REJECT, 0xE6=CANCEL
  uint8_t  channel;       // байт 1: канал
  uint8_t  ttl;           // байт 2: TTL
  uint8_t  sender[4];     // байты 3-6: device_id отвечающего
  uint8_t  call_seq;      // байт 7: номер вызова (совпадает с call_seq из CALL_PRIVATE)
};
#pragma pack(pop)

#define PKT_TYPE_CALL_ALL       0xE0
#define PKT_TYPE_CALL_PRIVATE   0xE1
#define PKT_TYPE_CALL_GROUP     0xE2
#define PKT_TYPE_CALL_EMERGENCY 0xE3
#define PKT_TYPE_CALL_ACCEPT    0xE4
#define PKT_TYPE_CALL_REJECT    0xE5
#define PKT_TYPE_CALL_CANCEL    0xE6

#define CALL_TIMEOUT_SEC        30   // таймаут ожидания ответа
#define EMERGENCY_REPEAT_SEC    30   // интервал автоповтора SOS
#define MAX_GROUP_MEMBERS       8    // макс. участников группы
#define MAX_SAVED_GROUPS        8    // макс. сохранённых групп в NVS

### 4.3 LoRa параметры (lora_radio.cpp)
```
Частота:       863.150 МГц (канал 0)
Bandwidth:     250 кГц     (оптимальный баланс: 23 канала + скорость 10.9 кбит/с)
SpreadFactor:  7            (SF7; Codec2 требует 1.2 кбит/с — запас ×9)
CodingRate:    5            (CR 4/5)
SyncWord:      0x34         (приватная сеть, не конфликтует с LoRaWAN 0x34/0x12)
Power:         14 дБм       (EU868 лимит 25мВт; вне EU можно до 22 дБм)
Preamble:      8 символов
CRC:           включён
Шаг сетки:    300 кГц      (BW250 занимает ~300 кГц; каналы не перекрываются)

Флаги конфигурации (lora_radio.h):
  #define ENFORCE_DUTY_CYCLE  false   // true = соблюдать EU868 1%; false = без ограничений
  #define TX_POWER_DBM        14      // дБм: 1–22 (SX1262 аппаратный диапазон)
  #define DUTY_CYCLE_PERCENT  1       // применяется только если ENFORCE_DUTY_CYCLE true

  Пояснение ENFORCE_DUTY_CYCLE:
    true  → RadioLib автоматически делает паузы между TX (соблюдение ETSI EN 300 220)
    false → передача без ограничений по времени (для частной сети / тестирования)
    Значение меняется командой BLE 0x0A SET_SETTINGS с телефона (сохраняется в NVS)

  Пояснение TX_POWER_DBM:
    SX1262 физически поддерживает 1–22 дБм
    EU868 регуляторный лимит: 14 дБм (25 мВт)
    Для максимальной дальности вне EU: 22 дБм
    Значение меняется командой BLE 0x0A SET_SETTINGS с телефона (сохраняется в NVS)

Почему BW=250 кГц:
  - BW500 → 11 каналов, задержка пакета ~15 мс
  - BW250 → 23 канала,  задержка пакета ~37 мс  ← выбрано
  - Разница 22 мс на слух в PTT режиме незаметна
  - Codec2 3200 bps умещается с запасом при BW250+SF7

КАНАЛЫ — 23 канала, сетка 300 кГц, диапазон 863.15–869.85 МГц:

  Группа A — основные (863–866 МГц):
    CH  0:  863.150 МГц  ← канал по умолчанию
    CH  1:  863.450 МГц
    CH  2:  863.750 МГц
    CH  3:  864.050 МГц
    CH  4:  864.350 МГц
    CH  5:  864.650 МГц
    CH  6:  864.950 МГц
    CH  7:  865.250 МГц
    CH  8:  865.550 МГц
    CH  9:  865.850 МГц

  Группа B — расширенные (866–869 МГц):
    CH 10:  866.150 МГц
    CH 11:  866.450 МГц
    CH 12:  866.750 МГц
    CH 13:  867.050 МГц
    CH 14:  867.350 МГц
    CH 15:  867.650 МГц
    CH 16:  867.950 МГц
    CH 17:  868.250 МГц
    CH 18:  868.550 МГц
    CH 19:  868.850 МГц

  Группа C — специальные (869 МГц, повышенный duty cycle):
    CH 20:  869.150 МГц
    CH 21:  869.450 МГц
    CH 22:  869.750 МГц

  Итого: 23 канала × 300 кГц = 6.9 МГц (укладывается в 863–870 МГц)
  Duty cycle: 1% для всех каналов (EU868 ISM)

  Реализация в коде:
    const float CHANNELS[23];
    // Формула: CHANNELS[i] = 863.150 + i * 0.300  (МГц)
    // Генерировать в цикле, не хардкодить каждый

  РЕГИОН_ФЛАГ в прошивке: #define REGION_EU868 (по умолчанию)
  Для смены региона: изменить массив CHANNELS[] и NUM_CHANNELS в lora_radio.cpp
  US915 каналы (903.9–905.3 МГц) — раскомментировать блок US в lora_radio.cpp
```

### 4.4 BLE сервис (ble_service.cpp)
```
Имя устройства: "MeshTRX-XXXX" (последние 4 символа MAC)
Service UUID:   6E400001-B5A3-F393-E0A9-E50E24DCCA9E  (Nordic UART Service)
RX Char UUID:   6E400002-B5A3-F393-E0A9-E50E24DCCA9E  (Write, телефон→ESP32)
TX Char UUID:   6E400003-B5A3-F393-E0A9-E50E24DCCA9E  (Notify, ESP32→телефон)

MTU: запросить 128 байт при подключении

Протокол BLE-пакета (поверх NUS):
  Байт 0: тип сообщения
    0x01 = AUDIO_DATA     (телефон→ESP: отправить в LoRa)
    0x02 = AUDIO_DATA     (ESP→телефон: получено из LoRa)
           BLE аудио пакет: 68 байт (cmd[1] + flags[1] + sender[2] + payload[64])
    0x03 = PTT_START      (телефон→ESP: начать передачу)
    0x04 = PTT_END        (телефон→ESP: закончить передачу)
    0x05 = SET_CHANNEL    (телефон→ESP: байт 1 = номер канала 0–22)
    0x06 = STATUS_UPDATE  (ESP→телефон: байт 1=канал, байты 2-3=RSSI int16, байт 4=SNR int8)
    0x07 = SEND_MESSAGE   (телефон→ESP: байт 1=seq, байты 2-N = UTF-8 текст, макс. 80 байт)
    0x08 = RECV_MESSAGE   (ESP→телефон: байт 1=RSSI, байты 2-N = UTF-8 текст + "\x00" + sender_id)
    0x09 = MESSAGE_ACK    (ESP→телефон: байт 1=seq подтверждённого сообщения)
    0x0A = SET_SETTINGS   (телефон→ESP: JSON строка с настройками, см. ниже)
    0x0B = GET_SETTINGS   (телефон→ESP: запрос текущих настроек)
    0x0C = SETTINGS_RESP  (ESP→телефон: JSON строка с текущими настройками)
    0x0D = FILE_START     (телефон→ESP: начало передачи файла, байты = LoRaFileHeader)
    0x0E = FILE_CHUNK     (телефон→ESP: чанк файла, байты = LoRaFileChunk.data + chunk_index)
    0x0F = FILE_RECV      (ESP→телефон: получен файл из LoRa, байт 1=file_type, байты 2-N=данные или путь)
    0x10 = FILE_PROGRESS  (ESP→телефон: прогресс передачи, байт 1=session_id, байты 2-3=chunks_done, байты 4-5=chunks_total)
    0x11 = SET_TX_MODE    (телефон→ESP: байт 1: 0x00=PTT, 0x01=VOX)
    0x12 = VOX_STATUS     (ESP→телефон: байт 1: 0=idle, 1=attack, 2=active, 3=hangtime)
    0x13 = VOX_LEVEL      (телефон→ESP: байты 1-2: RMS uint16, каждые 40мс в режиме VOX)
    0x14 = GET_LOCATION   (ESP→телефон: запрос текущих координат телефона)
    0x15 = LOCATION_UPDATE(телефон→ESP: байты 1-4=lat_e7 int32, байты 5-8=lon_e7 int32,
                           байты 9-10=alt_m int16, байт 11=accuracy_m uint8)
    0x16 = BEACON_SENT    (ESP→телефон: уведомление что наш пинг отправлен, байты 1-2=seq)
    0x17 = PEER_SEEN      (ESP→телефон: получен пинг от другого устройства)
           Формат: байты 1-4=device_id, байты 5-13=call_sign[9],
                   байты 14-17=lat_e7, байты 18-21=lon_e7,
                   байты 22-23=rssi int16, байт 24=snr int8,
                   байт 25=tx_power, байт 26=battery, байт 27=flags,
                   байты 28-31=uptime_sec
    0x18 = CALL_ALL       (телефон→ESP: отправить общий вызов; байты 1-8=message)
    0x19 = CALL_PRIVATE   (телефон→ESP: байты 1-4=target_id (полный 4-байтовый deviceId), байты 5-12=message)
    0x1A = CALL_GROUP     (телефон→ESP: байт 1=group_index или 0xFF=ad-hoc,
                           если ad-hoc: байт 2=count, байты 3..=device_ids)
    0x1B = CALL_EMERGENCY (телефон→ESP: активировать SOS; байты 1-4=lat_e7, 5-8=lon_e7)
    0x1C = CALL_ACCEPT    (телефон→ESP: принять входящий вызов; байт 1=call_seq)
    0x1D = CALL_REJECT    (телефон→ESP: отклонить вызов; байт 1=call_seq)
    0x1E = CALL_CANCEL    (телефон→ESP: отменить исходящий вызов/SOS)
    0x1F = INCOMING_CALL  (ESP→телефон: входящий вызов)
           Формат: байт 1=call_type(0=ALL,1=PRIVATE,2=GROUP,3=EMERGENCY),
                   байты 2-5=sender_id, байты 6-14=call_sign,
                   байты 15-22=координаты (для SOS), байт 23=call_seq
    0x20 = CALL_STATUS    (ESP→телефон: статус вызова)
           Формат: байт 1=status(0=CALLING,1=ACCEPTED,2=REJECTED,3=TIMEOUT,4=CANCELLED)
                   байты 2-5=responder_id
    0x21 = GROUP_LIST     (телефон→ESP: запрос списка групп)
    0x22 = GROUP_LIST_RESP(ESP→телефон: JSON со списком сохранённых групп)
    0x23 = GROUP_SAVE     (телефон→ESP: сохранить группу в NVS; JSON)
    0x24 = GROUP_DELETE   (телефон→ESP: удалить группу; байт 1=group_index)
    0x25 = PIN_CHECK      (телефон→ESP: 4 байта PIN uint32 LE)
    0x26 = PIN_RESULT     (ESP→телефон: байт 1: 0=FAIL, 1=OK)
    0x27 = FILE_DATA      (ESP→телефон: чанк данных принятого файла)
    0x28 = SET_REPEATER   (телефон→ESP: байт 1=enable, далее ssid\0, pass\0, ip\0)
           Устройство сохраняет WiFi config в NVS и перезагружается
           В режиме ретранслятора: LoRa+BLE+WiFi, веб-монитор на http://IP
  Байты 1–N: данные

SET_SETTINGS JSON формат:
  {
    "duty_cycle": false,       // ENFORCE_DUTY_CYCLE
    "tx_power": 14,            // дБм, 1–22
    "beacon_interval": 300,    // интервал beacon в секундах (0=выключен)
    "callsign": "ALPHA"        // позывной (до 8 символов ASCII)
  }
  ESP32 сохраняет в NVS (Non-Volatile Storage) и применяет немедленно

SETTINGS_RESP JSON формат:
  {
    "duty_cycle": false,
    "tx_power": 14,
    "beacon_interval": 300,
    "callsign": "ALPHA"        // текущий позывной на устройстве
  }
  Android при подключении сверяет callsign телефона с устройством.
  Если отличается — синхронизирует на ESP32.

**ВАЖНО**: При форматировании deviceId из байтов в hex-строку (Kotlin/Java),
  обязательно использовать `data[N].toInt() and 0xFF` для избежания sign extension.
  Без этого байты >= 0x80 дают "FFFFFFXX" вместо "XX"
```

### 4.5 Логика main.cpp
```
Инициализация:
  1. Serial (115200)
  2. OLED (U8g2, I2C на SDA=17, SCL=18, RST=21)
  3. SPI для LoRa (SCK=9, MISO=11, MOSI=10)
  4. RadioLib SX1262
  5. NimBLE сервер
  6. Показать на OLED: имя BLE, канал, статус

Основной цикл (две задачи FreeRTOS):

  Задача 1 — loraTask (Core 0, приоритет 5):
    - Режим RX по умолчанию (прерывание на DIO1)
    - При получении пакета type=0xA0 (аудио):
        * Проверить channel == текущий канал
        * Извлечь payload (48 байт Codec2)
        * Отправить через BLE Notify на телефон (тип 0x02)
        * Обновить RSSI/SNR
        * Обновить OLED
    - При получении пакета type=0xB0 (текст):
        * Проверить channel == текущий канал
        * Извлечь текст и sender_id
        * Отправить через BLE Notify на телефон (тип 0x08)
        * Показать на OLED: "MSG: [первые 16 символов]" на 3 секунды
        * Отправить ACK обратно в LoRa (маленький пакет 0xB0 с флагом ACK)
    - В режиме TX (флаг pttActive):
        * Брать аудио пакеты из очереди txAudioQueue
        * Отправлять через LoRa
        * Возвращаться в RX
    - При получении текста в txTextQueue:
        * Дождаться конца TX (если идёт аудио)
        * Отправить текстовый пакет
        * Вернуться в RX

  Задача 2 — bleTask (Core 1, приоритет 5):
    - Обрабатывать входящие BLE Write:
        0x01 AUDIO_DATA   → положить в txAudioQueue
        0x03 PTT_START    → установить pttActive=true, переключить LoRa в TX
        0x04 PTT_END      → установить pttActive=false, переключить LoRa в RX
        0x05 SET_CHANNEL  → сменить канал LoRa (0–22)
        0x07 SEND_MESSAGE → положить текст в txTextQueue
        0x0A SET_SETTINGS → распарсить JSON, применить duty_cycle/tx_power/region,
                            сохранить в NVS через Preferences library
        0x0B GET_SETTINGS → отправить 0x0C SETTINGS_RESP с текущим JSON
                            (включает duty_cycle, tx_power, beacon_interval, callsign)
        0x0D FILE_START   → инициализировать сессию приёма файла, сохранить метаданные
        0x0E FILE_CHUNK   → добавить чанк в буфер txFileBuffer, если набрали 1 пакет LoRa → TX
    - Каждые 500мс отправлять STATUS_UPDATE (0x06) если телефон подключён

  Задача 3 — fileTask (Core 0, приоритет 3, только при активной передаче файла):
    - Брать чанки из txFileBuffer
    - Отправлять LoRaFileChunk через LoRa с паузой между пакетами
    - Ждать FILE_ACK (таймаут 2 сек) — при NACK повторить чанк (макс. 3 попытки)
    - При завершении отправить LoRaFileEnd с CRC16
    - Отправлять FILE_PROGRESS (0x10) на телефон каждые 5 чанков

  Задача 4 — beaconTask (Core 1, приоритет 2):
    - Читать BEACON_INTERVAL из NVS (по умолчанию 300 сек)
    - Если BEACON_INTERVAL == 0 → задача спит, пинги не отправляются
    - По таймеру:
        1. Запросить у телефона актуальные координаты (BLE команда 0x14 GET_LOCATION)
        2. Подождать ответ 0x15 LOCATION_UPDATE до 2 сек
        3. Собрать LoRaBeaconPacket:
             device_id  ← последние 4 байта WiFi MAC
             call_sign  ← из NVS (настройки)
             lat_e7, lon_e7 ← от телефона (или 0 если нет ответа)
             tx_power   ← текущее значение
             battery    ← ADC GPIO1 / делитель напряжения (схема: R13/R14)
             flags      ← GPS_VALID если получили координаты, BUSY если TX активен
             uptime_sec ← esp_timer_get_time() / 1e6
             beacon_seq ← инкремент
             crc16      ← CRC16-CCITT от байт 0..33
        4. Дождаться конца TX (если идёт аудио/файл) — не прерывать
        5. Отправить LoRaBeaconPacket в LoRa
        6. Уведомить телефон: 0x16 BEACON_SENT
    - При получении чужого LoRaBeaconPacket (в loraTask):
        1. Извлечь device_id, call_sign, координаты, RSSI, SNR
        2. Отправить на телефон команду 0x17 PEER_SEEN (см. BLE протокол)
        3. На OLED: "SEEN: [callsign] -85dBm" на 2 секунды

  При получении файловых пакетов из LoRa (в loraTask):
    - 0xC0 FILE_START: создать сессию, аллоцировать буфер в PSRAM (если есть) или heap
    - 0xC1 FILE_CHUNK: записать чанк в буфер по chunk_index, отправить ACK
    - 0xC3 FILE_END:   проверить CRC16, если ОК → отправить через BLE 0x0F FILE_RECV
                        на OLED: "FILE: [имя] [размер]КБ"

OLED экран — НОРМАЛЬНЫЙ РЕЖИМ (обновлять при каждом событии):
  Строка 1: "MeshTRX"
  Строка 2: "CH:00 863.15MHz"
  Строка 3: "RSSI:-85 PWR:14"
  Строка 4: "BLE: подключён"
  Строка 5: ■■■□□  DC:OFF (уровень + duty cycle статус)

OLED экран — РЕЖИМ РЕТРАНСЛЯТОРА (полностью другой layout):
  Строка 1: "** REPEATER **"       ← жирный заголовок, мигает при TX
  Строка 2: "CH:00 863.15MHz"
  Строка 3: "PWR:14  DC:OFF"
  Строка 4: "RX:■■■□□ -85dBm"     ← RSSI последнего принятого пакета
  Строка 5: "FWD:1234 DROP:5"      ← счётчики: ретранслировано / отброшено (TTL=0 или дубль)
```

### 4.10 Режим ретранслятора Store & Forward (repeater.h / repeater.cpp)
```
Активируется флагом REPEATER_MODE=true в NVS.
Переключается через меню на кнопке USER (GPIO0) без телефона.
Работает полностью автономно — BLE не нужен, телефон не нужен.
Питание: USB 5V, потребление ~80 мА в режиме ожидания.

─── Переключение режима кнопкой USER (GPIO0) ───────

  Удержание кнопки USER > 3 секунд:
    → переключить REPEATER_MODE (false → true или true → false)
    → сохранить в NVS
    → перезагрузить устройство (esp_restart())

  OLED при переключении (1 сек перед перезагрузкой):
    Нормальный → Ретранслятор:  "REPEATER MODE ON"  "Restarting..."
    Ретранслятор → Нормальный:  "NORMAL MODE"        "Restarting..."

─── Инициализация в режиме ретранслятора ───────────

  1. Serial (115200) — для отладки
  2. OLED: показать "** REPEATER **" + канал
  3. SPI + RadioLib SX1262 — настроить RX
  4. NimBLE НЕ запускать (BLE выключен для экономии)
  5. Инициализировать дедупликационный кеш (кольцевой буфер 64 записи)
  6. Запустить repeaterTask

─── repeaterTask (Core 0, приоритет 5) ─────────────

  Основной цикл:
    1. Ждать пакет из LoRa RX (прерывание DIO1)
    2. Проверить CRC — если ошибка → DROP, инкремент drop_count, продолжить
    3. Извлечь: type, channel, sender[2], seq, ttl
    4. Проверить channel == текущий канал → если нет → DROP
    5. Проверить дедупликацию:
         ключ = sender[0..1] + seq + type (4 байта)
         искать в кольцевом буфере dedupCache[64]
         если найден → DROP (дубль, уже ретранслировали)
         если не найден → добавить в кеш (вытеснить старейший)
    6. Проверить TTL:
         если ttl == 0 → DROP (достигло предела хопов)
         уменьшить ttl на 1
    7. Задержка перед TX: случайная 10–50 мс (равномерное распределение)
       → защита от коллизии если рядом несколько ретрансляторов
    8. Переключить SX1262 в TX
    9. Отправить пакет с уменьшенным TTL (байт TTL перезаписать)
    10. Переключить SX1262 обратно в RX
    11. Инкремент fwd_count
    12. Обновить OLED строки 4–5

  Обработка beacon пакетов (0xD0):
    → ретранслировать как обычный пакет (с TTL декрементом)
    → дополнительно: обновить внутренний список peers ретранслятора
       (для отображения на OLED)
    → На OLED строка 4: "SEEN:[callsign]" на 2 сек

  Что НЕ ретранслировать (DROP без счётчика):
    → 0xC2 FILE_ACK (TTL=0 по определению, локальные квитанции)
    → пакеты с type неизвестного формата

─── Дедупликационный кеш ───────────────────────────

  Структура:
    struct DedupEntry {
      uint8_t  sender[2];   // MAC отправителя
      uint8_t  seq;         // порядковый номер
      uint8_t  type;        // тип пакета
      uint32_t timestamp;   // millis() при добавлении
    };
    DedupEntry dedupCache[64];  // кольцевой буфер
    uint8_t dedupHead = 0;      // индекс следующей записи

  Время жизни записи: 30 секунд
    → при поиске игнорировать записи старше 30 сек
    → позволяет корректно ретранслировать повторные отправки после паузы

─── OLED ретранслятора — детальный layout ──────────

  Дисплей 128×64, шрифт 6×8, 8 строк × 21 символ

  Строка 0 (y=0):  "** REPEATER **"       центр, инвертированный фон
  Строка 1 (y=8):  "CH:00  863.15MHz"
  Строка 2 (y=16): "PWR:14dBm  DC:OFF"
  Строка 3 (y=24): "────────────────────" горизонтальный разделитель
  Строка 4 (y=32): "RX: -085dBm  SNR:+8"  ← последний принятый пакет
  Строка 5 (y=40): "TTL:2→1  [тип пакета]" ← тип: AUD/TXT/FIL/BCN
  Строка 6 (y=48): "FWD:001234  DRP:005"   ← счётчики (сбрасываются при рестарте)
  Строка 7 (y=56): "UP: 02d 14h 23m 05s"   ← время работы

  Анимация при TX (строка 0 мигает): чередовать нормальный/инвертированный фон
    каждые 200 мс пока идёт TX пакета (~37 мс) → визуально мигнёт 1 раз

─── Настройки ретранслятора в NVS ──────────────────

  "repeater_mode"    : bool  (false по умолчанию)
  "repeater_channel" : int   (0–22, канал для ретрансляции)
  "repeater_power"   : int   (1–22 дБм)
  "repeater_dc"      : bool  (duty cycle)
  "repeater_name"    : char[9] (позывной ретранслятора для beacon)

  Beacon в режиме ретранслятора:
    → ретранслятор ТОЖЕ отправляет свой пинг (если beacon_interval > 0)
    → в поле call_sign: "RPT-XXXX" где XXXX — последние 4 символа MAC
    → координаты: 0 (нет телефона), GPS_VALID=false
    → battery: читать с АЦП (если есть LiPo) или 0xFF (USB)
    → в пинге флаг бит 3 = REPEATER (добавить в BEACON_FLAG_*)

─── Статистика (сбрасывается кнопкой 1 сек) ────────

  uint32_t fwd_count    — ретранслировано пакетов
  uint32_t drop_count   — отброшено (CRC/TTL/дубль/чужой канал)
  uint32_t audio_fwd    — из них аудио пакетов
  uint32_t text_fwd     — текстовых
  uint32_t file_fwd     — файловых чанков
  uint32_t beacon_fwd   — beacon пакетов
  int16_t  min_rssi     — минимальный RSSI за сессию
  int16_t  max_rssi     — максимальный RSSI за сессию

  Кнопка USER 1 сек (в режиме ретранслятора):
    → сбросить счётчики
    → OLED: "STATS CLEARED" на 1 сек
```
```
VOX (Voice Operated eXchange) — автоматически начинает TX когда микрофон
улавливает голос, и заканчивает TX после паузы тишины.

Алгоритм:
  1. Анализировать уровень входящего аудио (RMS от AudioEngine)
  2. Если RMS > VOX_THRESHOLD → активировать TX (как PTT_START)
  3. Если RMS < VOX_THRESHOLD непрерывно VOX_HANGTIME мс → деактивировать TX (PTT_END)
  4. После деактивации TX — воспроизвести тон конца передачи (Roger Beep)

Параметры:
  VOX_THRESHOLD  : uint16_t, 0–32767, по умолчанию 800  (регулируется из настроек)
  VOX_HANGTIME   : uint32_t, мс,      по умолчанию 800  (пауза перед концом TX)
  VOX_ATTACK     : uint32_t, мс,      по умолчанию 50   (задержка перед стартом TX)

Структура в прошивке:
  enum VoxState { VOX_IDLE, VOX_ATTACK, VOX_ACTIVE, VOX_HANGTIME };
  
  void voxProcess(uint16_t rms):
    VOX_IDLE:
      if rms > threshold → запустить таймер attack, перейти в VOX_ATTACK
    VOX_ATTACK:
      if таймер истёк и rms > threshold → активировать TX, перейти в VOX_ACTIVE
      if rms < threshold → сбросить, вернуться в VOX_IDLE (защита от щелчков)
    VOX_ACTIVE:
      if rms < threshold → запустить таймер hangtime, перейти в VOX_HANGTIME
    VOX_HANGTIME:
      if rms > threshold → вернуться в VOX_ACTIVE (продолжение речи)
      if таймер истёк → деактивировать TX → воспроизвести Roger Beep → VOX_IDLE

Режимы управления TX (взаимоисключающие):
  MODE_PTT : ручное управление (кнопка в приложении)
  MODE_VOX : автоматическое (VOX алгоритм)
  Переключение командой BLE 0x11 SET_TX_MODE

VOX на ESP32:
  - RMS считается на Core 0 в loraTask из буфера входящего аудио от BLE
  - voxProcess() вызывается каждые 40мс (один фрейм Codec2)
  - При активации VOX → выставить флаг voxActive, запустить TX
  - Уведомить телефон командой 0x12 VOX_STATUS (байт 1: 0=idle, 1=active)

VOX на Android:
  - AudioEngine считает RMS каждого буфера 320 сэмплов
  - Если режим VOX: отправлять VOX уровень на ESP32 командой 0x13 VOX_LEVEL
    байт 1-2: RMS uint16 (каждые 40мс)
  - ESP32 применяет алгоритм и управляет TX
  - Альтернатива: алгоритм VOX можно реализовать полностью на Android,
    тогда APP самостоятельно отправляет PTT_START / PTT_END по уровню микрофона
    (проще, рекомендуется для v1)
```

### 4.11 Система вызовов (call_manager.h / call_manager.cpp)
```
Единый модуль управляющий всеми типами вызовов.

─── Состояния ───────────────────────────────────────

  enum CallState {
    CALL_IDLE,           // нет активного вызова
    CALL_OUTGOING,       // исходящий, ждём ответа (таймер 30 сек)
    CALL_INCOMING,       // входящий, ждём нашего действия
    CALL_ACTIVE,         // вызов принят, идёт разговор
    CALL_EMERGENCY_TX,   // SOS отправляется (автоповтор каждые 30 сек)
    CALL_EMERGENCY_RX,   // получен SOS от другого устройства
  };

─── Обработка входящих (в loraTask) ─────────────────

  0xE0 ALL CALL:
    → Уведомить BLE (0x1F type=ALL) + OLED мигает + LED

  0xE1 PRIVATE CALL:
    → Если target == наш device_id → BLE 0x1F + OLED + callState=INCOMING
    → Иначе → тихое уведомление на телефон

  0xE2 GROUP CALL:
    → Если наш device_id в members[] → BLE 0x1F + OLED

  0xE3 EMERGENCY:
    → Всегда! → callState=CALL_EMERGENCY_RX
    → BLE 0x1F + OLED красный инвертированный экран
    → Мигание LED до получения CALL_CANCEL

  0xE4 ACCEPT / 0xE5 REJECT / 0xE6 CANCEL:
    → Обновить callState, уведомить телефон 0x20 CALL_STATUS

─── Отправка (из bleTask) ───────────────────────────

  0x18 CALL_ALL:
    → LoRaCallAll → TX → callState=IDLE (ответа не ждём)

  0x19 CALL_PRIVATE:
    → LoRaCallPrivate с уникальным call_seq → TX
    → callState=OUTGOING → таймер 30 сек (TIMEOUT если нет ответа)

  0x1A CALL_GROUP:
    → LoRaCallGroup → TX → callState=IDLE

  0x1B CALL_EMERGENCY:
    → LoRaCallEmergency с TTL_MAX=4 → TX
    → callState=EMERGENCY_TX → автоповтор каждые 30 сек
    → OLED: полноэкранный SOS режим

  0x1C/0x1D/0x1E ACCEPT/REJECT/CANCEL:
    → LoRaCallResponse → TX → callState=IDLE

─── OLED индикация вызовов ──────────────────────────

  ALL CALL входящий:
    Стр.0: ">>> ALL CALL <<<"    (мигает инверсия 500мс)
    Стр.1: "From: ALPHA"
    Стр.2: "На связь!"
    Стр.3: "-085dBm  TTL:1"

  PRIVATE CALL входящий:
    Стр.0: ">>> ВЫЗОВ <<<"       (мигает)
    Стр.1: "От: BRAVO"
    Стр.2: "[ПРИНЯТЬ / ОТКЛ]"   (подсказка кнопок в APP)
    Стр.3: "-075dBm  TTL:2"

  EMERGENCY входящий — весь экран инвертирован:
    Стр.0: "!!! S O S !!!"       (быстрое мигание)
    Стр.1: "CHARLIE"
    Стр.2: "59.437N 24.745E"
    Стр.3: "RSSI:-080  TTL:3"

  SOS ACTIVE исходящий — весь экран инвертирован:
    Стр.0: "!! SOS ACTIVE !!"
    Стр.1: "Повтор через: 28s"
    Стр.2: "Отправлено: 3"
    Стр.3: "[USER кнопка=отмена]"

─── Группы в NVS ────────────────────────────────────

  JSON в NVS ключ "groups":
  { "groups": [
      { "id":1, "name":"Команда А",
        "members":["AABBCCDD","EEFF0011","22334455"] },
      ...
  ]}
  Макс: 8 групп × 8 участников
  Синхронизация: только локально (каждый создаёт группы сам)

─── Защита SOS от случайного нажатия ────────────────

  В APP: двойное подтверждение (кнопка → диалог "Отправить SOS?") 
  На плате: удержание USER кнопки 5 сек (отличается от 3 сек = REPEATER)
  Отмена SOS: кнопка CANCEL в APP или USER кнопка 2 сек во время SOS

─── Приоритет передачи ──────────────────────────────

  EMERGENCY > CALL_ALL/PRIVATE/GROUP > FILE > TEXT > AUDIO > BEACON
  EMERGENCY прерывает текущий TX (кроме чужого RX — ждать конца пакета)

```

### 4.8 Roger Beep — тон конца передачи (roger_beep.h / roger_beep.cpp)
```
Roger Beep — короткий тональный сигнал, передаётся в LoRa сразу после
окончания речи (PTT отпущен или VOX завис). Принимающая сторона
воспроизводит его через динамик телефона — сигнал "передача окончена".

Типы тонов (выбираются в настройках):
  BEEP_NONE       : тон отключён
  BEEP_SHORT      : один тон 440 Гц, 150 мс  (классическая рация)
  BEEP_MORSE_K    : тон-тире-тон (морзе "K" = "иду на приём"), ~500 мс
  BEEP_TWO_TONE   : 880 Гц 80мс → 440 Гц 120мс  (профессиональный стиль)
  BEEP_CHIRP      : свип 600→1200 Гц за 200 мс  (современный)

Реализация (ESP32 сторона):
  - Генерировать PCM синус в памяти (предпросчитать при инициализации)
  - Кодировать через Codec2 → упаковать в LoRaAudioPacket с флагом ROGER_BEEP
  - Отправить 1–3 пакета (в зависимости от длины тона) сразу после PTT_END
  - Флаг в пакете: LoRaAudioPacket.flags |= PKT_FLAG_ROGER_BEEP (бит 2)

  Генерация синуса (пример для 440 Гц, 8000 Гц PCM):
    for i in 0..319: pcm[i] = (int16_t)(amplitude * sin(2π * 440 * i / 8000))
  
  Предпросчитанные буферы хранить в PROGMEM / flash (не в RAM)

Реализация (Android сторона):
  - При получении пакета с PKT_FLAG_ROGER_BEEP:
    * Декодировать Codec2 как обычно — это и есть тон
    * AudioEngine воспроизводит через AudioTrack (как обычное аудио)
  - Дополнительно: показать визуальный индикатор "⊙ конец передачи" на 1 сек

Флаги LoRaAudioPacket (обновлённые):
  PKT_FLAG_PTT_START   = 0x01
  PKT_FLAG_PTT_END     = 0x02
  PKT_FLAG_ROGER_BEEP  = 0x04
  PKT_FLAG_VOX         = 0x08  // пакет отправлен через VOX (не ручной PTT)
```

### 4.9 Codec2 на ESP32
```
Использовать библиотеку: https://github.com/drowe67/codec2
Портированная версия для ESP32: включить только файлы encode/decode
Режим: CODEC2_MODE_1200 (1200 бит/с)
  - Размер фрейма: 40мс аудио = 8 байт Codec2
  - В одном LoRa пакете: 48 байт = 6 фреймов = 240мс аудио
Частота дискретизации: 8000 Гц (моно, 16-бит PCM)

ВАЖНО: Codec2 портировать как статическую библиотеку в lib/codec2/
Файлы нужны минимальные: codec2.c, kiss_fft.c, nlp.c, lpc.c,
  postfilter.c, quantise.c, pack.c, sine.c, phase.c, interp.c
```

---

## 5. Android приложение (Kotlin + Android Studio)

### 5.1 Конфигурация (build.gradle)
```gradle
minSdk 26  (Android 8.0 — нужен для BLE MTU)
targetSdk 34
compileSdk 34

dependencies:
  implementation "androidx.lifecycle:lifecycle-viewmodel-ktx:2.7.0"
  implementation "androidx.lifecycle:lifecycle-livedata-ktx:2.7.0"
  implementation "org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3"
  implementation "com.google.android.gms:play-services-location:21.0.1"  // FusedLocationProvider
  implementation "org.osmdroid:osmdroid-android:6.1.17"                  // карта OpenStreetMap
  // Codec2 через JNI (собирается из C-кода в jni/)
```

### 5.2 Разрешения (AndroidManifest.xml)
```xml
<uses-permission android:name="android.permission.BLUETOOTH_SCAN"
    android:usesPermissionFlags="neverForLocation" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.BLUETOOTH_ADVERTISE" />
<uses-permission android:name="android.permission.RECORD_AUDIO" />
<uses-permission android:name="android.permission.BLUETOOTH"
    android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN"
    android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.READ_MEDIA_IMAGES" />          <!-- Android 13+ -->
<uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE"
    android:maxSdkVersion="32" />                                                  <!-- Android 12- -->
<uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE"
    android:maxSdkVersion="28" />                                                  <!-- для сохранения принятых файлов -->
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />        <!-- GPS для beacon -->
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />
<uses-permission android:name="android.permission.INTERNET" />                    <!-- osmdroid тайлы -->
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />        <!-- osmdroid -->
<uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />

<!-- FileProvider для sharing принятых файлов -->
<provider
    android:name="androidx.core.content.FileProvider"
    android:authorities="${applicationId}.fileprovider"
    android:exported="false"
    android:grantUriPermissions="true">
  <meta-data android:name="android.support.FILE_PROVIDER_PATHS"
      android:resource="@xml/file_paths" />
</provider>
```

### 5.3 BleManager.kt
```
Функции:
  - startScan(): сканировать BLE устройства с именем "MeshTRX-*"
  - connect(device): подключиться, запросить MTU 128, включить Notify на TX Char
  - sendAudioData(bytes: ByteArray): записать в RX Char (тип 0x01)
  - sendPttStart(): записать 0x03
  - sendPttEnd(): записать 0x04
  - setChannel(ch: Int): записать [0x05, ch]
  - sendMessage(seq: Int, text: String): записать [0x07, seq, ...UTF8]
  - sendSettings(settings: DeviceSettings): сериализовать в JSON, записать [0x0A, ...JSON]
  - requestSettings(): записать [0x0B]
  - sendFile(header: FileTransferHeader, data: ByteArray):
      1. Отправить [0x0D, ...LoRaFileHeader байты]
      2. Нарезать data на чанки по 200 байт
      3. Для каждого чанка: отправить [0x0E, chunk_index_low, chunk_index_high, ...data]
      4. Ждать FILE_PROGRESS нотификации
  - onAudioReceived: callback (ByteArray)
  - onStatusUpdate: callback (channel, rssi, snr)
  - onMessageReceived: callback (text, rssi, senderId)
  - onMessageAck: callback (seq)
  - onSettingsReceived: callback (DeviceSettings)
  - onFileReceived: callback (fileType, fileName, data: ByteArray)
  - onFileProgress: callback (sessionId, done, total)

Обработка MTU:
  - При получении больших аудио-данных собирать фрагменты по sequence number
  - Отправлять чанки по (MTU-3) байт с задержкой 20мс между пакетами
```

### 5.4 AudioEngine.kt
```
Запись:
  - AudioRecord: частота 8000 Гц, моно, PCM 16-bit
  - Размер буфера: 320 сэмплов = 40мс (один фрейм Codec2)
  - В потоке: читать буфер → кодировать Codec2 → накопить 6 фреймов (48 байт)
    → передать в BleManager.sendAudioData()

Воспроизведение:
  - AudioTrack: режим STREAM, 8000 Гц, моно, PCM 16-bit
  - Принимать 48 байт Codec2 → декодировать 6 фреймов по 320 сэмплов
    → записать в AudioTrack
  - Буферизация: очередь на 3 пакета (720мс) для сглаживания jitter

Codec2Wrapper.kt:
  - Загружать нативную библиотеку: System.loadLibrary("codec2jni")
  - encode(pcm: ShortArray): ByteArray   — 320 сэмплов → 8 байт
  - decode(encoded: ByteArray): ShortArray — 8 байт → 320 сэмплов
```

### 5.5 UI — activity_main.xml
```
Макет: BottomNavigation с 4 вкладками + отдельный экран Settings

─── ВКЛАДКА 1: ГОЛОС ────────────────────────────────

  [Верхняя панель]
    - TextView: "MeshTRX"
    - ImageView: иконка BLE (серая/синяя)
    - IconButton: шестерёнка → открыть SettingsActivity

  [Статус блок]
    - TextView: имя устройства ESP32
    - TextView: "Канал: 0 | 863.15 МГц  |  14 дБм"
    - ProgressBar горизонтальный (RSSI)
    - TextView: "RSSI: -85 dBm | SNR: 8 dB"
    - Chip: "DC: OFF" (зелёный) или "DC: ON" (жёлтый) — статус duty cycle

  [Выбор канала]
    - Spinner 23 канала (генерировать программно)
    - Button "Применить"

  [Центр — PTT кнопка / VOX индикатор]
    - В режиме PTT: Button круглая 160dp "ГОВОРИТЬ"
    - В режиме VOX: VU-meter + статус VOX
    - Switch в углу: "PTT / VOX"

  [Панель вызовов — под PTT кнопкой]
    - Button "Общий вызов" (ALL CALL) → диалог с полем короткого сообщения
    - Button "Вызвать..." → открыть CallPickerSheet (выбор из списка пиров)
    - Button "SOS" (красный, маленький) → двойное подтверждение

─── ЭКРАН / SHEET: ВХОДЯЩИЙ ВЫЗОВ ──────────────────

  Показывается поверх любого экрана (как системный звонок):
  - Полноэкранный overlay с анимацией пульса
  - Большой текст: "ВХОДЯЩИЙ ВЫЗОВ"
  - Имя/позывной звонящего (крупно)
  - Тип: "Общий канал" / "Личный вызов" / "Группа: Команда А"
  - RSSI звонящего
  - Две кнопки: [ПРИНЯТЬ] зелёная  [ОТКЛОНИТЬ] красная
    (для ALL CALL — только [ОТВЕТИТЬ PTT] и [ЗАКРЫТЬ])
  - Вибрация: паттерн ···---··· (SOS-подобный для вызова)
  - Звук: системный рингтон или тон (настраивается)
  - AUTO-DISMISS: через 30 сек без ответа (TIMEOUT)

─── ЭКРАН SOS (исходящий) ───────────────────────────

  Полноэкранный красный overlay:
  - "!! SOS АКТИВЕН !!" мигающий текст
  - Countdown: "Следующий повтор: 28 сек"
  - Счётчик: "Отправлено: 3 раза"
  - Координаты: "59.4370°N 24.7453°E"
  - Большая кнопка [ОТМЕНИТЬ SOS] — с подтверждением

─── ЭКРАН SOS (входящий) ────────────────────────────

  Красный overlay поверх всего:
  - "!! SOS !!" + имя отправителя
  - Координаты + кнопка "Показать на карте"
  - Расстояние от нас (если у нас тоже есть GPS)
  - Кнопка [Закрыть уведомление] (SOS продолжается на устройстве)

─── CallPickerSheet (выбор адресата) ────────────────

  BottomSheet со списком пиров:
  - Вкладка "Устройства": список видимых пиров с RSSI
    Tap → PRIVATE CALL к этому устройству
  - Вкладка "Группы": список сохранённых групп
    Tap → GROUP CALL
    LongTap → редактировать группу
  - FAB "Новая группа" → GroupEditSheet

  [Нижний статус + кнопка подключения]

─── ВКЛАДКА 2: СООБЩЕНИЯ (ЧАТ) ─────────────────────

  [RecyclerView — история]
    - Пузырьки: текст + вложения
    - Если сообщение содержит фото: ImageView 120×90dp внутри пузырька
    - Статусы: отправляется / доставлено ✓ / ошибка ✗

  [Панель ввода]
    - ImageButton: скрепка → открыть FilePickerBottomSheet
    - EditText: "Сообщение..." (макс. 84 символа)
    - Счётчик символов
    - Button "Отправить"

─── ВКЛАДКА 3: ФАЙЛЫ ────────────────────────────────

  [Кнопка "Отправить фото"] → PhotoPickerFragment
  [Кнопка "Отправить файл"] → системный Intent
  [История переданных файлов — RecyclerView]

─── ВКЛАДКА 4: СЕТЬ (PEERS) ────────────────────────

  [Заголовок]
    - TextView: "Устройства на канале X"
    - TextView: "Обновлено: 2 мин назад" (время последнего пинга)
    - Button "Карта" → открыть MapActivity (иконка карты)

  [Список пиров — RecyclerView]
    Строка каждого устройства:
    ┌─────────────────────────────────────────────────┐
    │  🟢 [ПОЗЫВНОЙ]              [RSSI бар] -85 dBm  │
    │  ID: AABBCCDD  · 14дБм · 🔋78%                  │
    │  📍 59.437°N 24.745°E  · alt: 34м               │
    │  Видно: 2 мин назад  ·  ↑ 1ч 23мин (uptime)    │
    └─────────────────────────────────────────────────┘

    Индикатор статуса (цветной кружок):
      🟢 зелёный  — активен (lastSeen < 1 × interval)
      🟡 жёлтый   — возможно вне зоны (lastSeen 1–2 × interval)
      🔴 красный  — не отвечает (lastSeen > 2 × interval)
      🔵 синий    — сейчас передаёт (флаг BUSY)

    RSSI бар: 5 делений (≥-70 / -80 / -90 / -100 / <-110 dBm)

    Tap на строку → PeerDetailSheet:
      - Полная информация об устройстве
      - Кнопка "Написать" → открыть чат с фильтром по callsign
      - Кнопка "Показать на карте"

  [Нижняя строка]
    - TextView: "Моё устройство: [позывной] · ID: XXXX"
    - TextView: "Следующий пинг через: 4:32"
    - Button "Отправить пинг сейчас" (ручной beacon)

─── ЭКРАН КАРТА / РАДАР (MapFragment, 5-я вкладка) ────────

  Два режима переключаются табами сверху: [Карта] [Радар]

  === РЕЖИМ КАРТА ===

  Библиотека: OpenStreetMap через osmdroid (v6.1.17)
    (бесплатно, без API ключа, работает офлайн с кешем тайлов)

  Слои карты:
    - OSM тайлы (онлайн + офлайн кеш)
    - Маркер "Я": синяя точка с кругом точности GPS
    - Маркеры peers: цветные точки с callsign подписью

  Маркер peer:
    - Цвет по давности: зелёный (<5мин), жёлтый (<10мин), красный (>10мин)
    - Подпись: "[callsign] -85dBm"
    - Tap → InfoWindow:
        Позывной, RSSI, SNR
        Координаты (59.4370°N 24.7453°E)
        Батарея%, мощность, давность
        [Написать] [Закрыть]

  Линии расстояния:
    - От "Я" до каждого peer: пунктирная линия
    - Подпись: расстояние в км (Haversine)

  Управление:
    - FAB "Моя позиция" → центр на себе
    - FAB "Все устройства" → zoom to fit
    - Кнопка "Скачать район" → CacheManager скачивает тайлы текущей области
      (для работы без интернета)
    - Автомасштаб: при первом открытии zoom вмещает всех peers

  === РЕЖИМ РАДАР ===

  Полностью оффлайн, не требует карт/тайлов/интернета.
  Тактический военный стиль.

  RadarView.kt (кастомный Canvas View):
    - Фон: чёрный (#000000)
    - Круги дистанции: тёмно-зелёные (#1a3a1a), концентрические
      Автомасштаб: кольца подбираются по самому дальнему peer
      Примеры: 100м, 500м, 1км, 5км, 10км, 50км
      Подписи расстояния на кольцах
    - Центр: своя позиция, зелёная точка с пульсацией
    - Peers: яркие зелёные точки (#4ade80) по реальному азимуту и расстоянию
      Подпись: callsign + расстояние
      Размер точки: по RSSI (сильнее = больше)
    - Линия севера: яркая метка "N" сверху
    - Компас: ориентация по магнитному датчику телефона
      TYPE_ROTATION_VECTOR → азимут
      Север всегда сверху (или поворот по направлению телефона — настройка)
    - Перекрестие по центру: тонкие линии через весь экран
    - Sweep-эффект (опционально): вращающаяся линия как у реального радара

  Tap на peer → тот же InfoWindow что и на карте

  === GPS (LocationHelper.kt) ===

  - FusedLocationProviderClient для координат телефона
  - Обновления: каждые 30 сек (пока приложение активно)
  - Координаты сохраняются в ServiceState.myLat / ServiceState.myLon
  - Отправляются на ESP32 для включения в beacon
  - При запросе от ESP32 (0x14 GET_LOCATION): ответ последними координатами
  - Разрешение: ACCESS_FINE_LOCATION
  - Настройка в Settings: "Включать GPS в пинг" (приватность)

  === Навигация ===

  5 вкладок в BottomNavigationView:
    PTT | Сообщения | Файлы | Карта | Настройки

  distanceKm() уже есть в Models.kt (Haversine)

─── ЭКРАН НАСТРОЕК (SettingsActivity / SettingsFragment) ────

  [Секция: VOX]
    - Switch "VOX режим (автоматическая передача по голосу)"
      Подсказка: "Вкл — микрофон всегда активен, TX запускается голосом"
    - SeekBar "Порог VOX (чувствительность)": 0–100%
      Перевод в uint16: threshold = value * 327
      Подпись: "Низкий — реагирует на шёпот и шум; Высокий — только громкая речь"
      Кнопка "Тест" → показать live VU-meter на 5 секунд с текущим порогом
    - SeekBar "Задержка конца передачи (hangtime)": 300–2000 мс
      Подпись: "Пауза после голоса перед остановкой TX"

  [Секция: Roger Beep]
    - RadioGroup "Тон конца передачи":
        ○ Нет
        ● Короткий (440 Гц, 150 мс)  ← по умолчанию
        ○ Морзе "K" (~500 мс)
        ○ Two-tone (профессиональный)
        ○ Chirp (свип 600→1200 Гц)
    - Button "Воспроизвести" → проиграть выбранный тон локально через динамик

  [Секция: Передача файлов]
    - SeekBar "Качество миниатюры": 10–80% JPEG
    - Switch "Автоподтверждение каждого чанка"
    - TextView: "Макс. рекомендуемый файл: 20 КБ"

  [Секция: Радио]
    - SeekBar "Мощность TX": 1–22 дБм
    - Switch "Соблюдать Duty Cycle (EU868 1%)"
    - Spinner "Регион": EU868 / US915 / AS923

  [Секция: Интерфейс]
    - EditText "Позывной / имя" (макс. 8 символов)

  [Секция: Beacon / пинг присутствия]
    - Spinner "Период пинга":
        Никогда (выкл)
        1 минута
        3 минуты
        5 минут (по умолчанию)
        15 минут
        30 минут
        1 час
    - TextView: "Пинг содержит: позывной, ID, координаты телефона, уровень батареи"
    - Switch "Включить GPS в пинг"
      Подсказка: "Выкл — координаты не передаются (приватность)"
    - Button "Отправить пинг сейчас" → команда ESP32 немедленно

  [Кнопка "Применить и сохранить"]
    - Отправить SET_SETTINGS (0x0A) на ESP32
    - Показать Toast "Настройки применены"

─── КОМПОНЕНТ: PhotoPickerFragment ─────────────────

  Запуск: ActivityResultContracts.PickVisualMedia (Android 13+)
          или Intent(Intent.ACTION_PICK) для старых версий

  После выбора фото → ImageProcessor.kt:
    1. Загрузить Bitmap через ContentResolver
    2. Определить ориентацию через ExifInterface, повернуть если нужно
    3. Масштабировать до макс. 160×120 пикселей (сохранить пропорции)
    4. Сжать в JPEG с качеством из настроек (по умолчанию 40%)
    5. Получить ByteArray — показать предпросмотр и размер в КБ
    6. Если размер > 20 КБ → предложить снизить качество (SeekBar в диалоге)
    7. Подтверждение → вызвать BleManager.sendFile()

  ImageProcessor.kt — методы:
    - preparePhoto(uri: Uri, quality: Int): Result<ByteArray>
    - estimateSize(width: Int, height: Int, quality: Int): Int
    - rotateBitmap(bitmap: Bitmap, exifOrientation: Int): Bitmap
```

### 5.6 MainViewModel.kt
```
LiveData:
  - connectionState: BleState (DISCONNECTED, SCANNING, CONNECTING, CONNECTED)
  - currentChannel: Int (0–22)
  - rssi: Int (дБм)
  - snr: Int
  - isPttActive: Boolean
  - isReceiving: Boolean
  - scanResults: List<BleDevice>
  - statusMessage: String
  - messages: LiveData<List<ChatMessage>>
  - unreadCount: Int
  - deviceSettings: LiveData<DeviceSettings>
  - fileTransfers: LiveData<List<FileTransfer>>

data class DeviceSettings(
  val dutyCycleEnabled: Boolean = false,
  val txPowerDbm: Int = 14,
  val region: String = "EU868",
  val photoQuality: Int = 40,
  val callSign: String = "",
  val ackEveryChunk: Boolean = true,
  val voxEnabled: Boolean = false,
  val voxThreshold: Int = 800,
  val voxHangtime: Int = 800,
  val rogerBeep: Int = 1,
  val beaconIntervalSec: Int = 300    // 0=никогда, 60/180/300/900/1800/3600
)

data class FileTransfer(
  val sessionId: Int,
  val fileName: String,
  val fileType: Int,                   // FILE_TYPE_*
  val totalSize: Int,
  val chunksTotal: Int,
  val chunksDone: Int,
  val isOutgoing: Boolean,
  val status: FileStatus,
  val localPath: String?,              // путь после сохранения (входящие)
  val thumbnail: Bitmap?               // для фото
)

enum class FileStatus { PENDING, TRANSFERRING, DONE, FAILED }

data class ChatMessage(
  val id: Long,
  val text: String?,                   // null если это файл/фото
  val fileTransferId: Int?,            // ссылка на FileTransfer если вложение
  val isOutgoing: Boolean,
  val senderId: String,
  val channel: Int,
  val rssi: Int?,
  val status: MessageStatus,
  val time: String
)

enum class MessageStatus { SENDING, SENT, DELIVERED, FAILED }

data class Peer(
  val deviceId: String,               // hex строка "AABBCCDD"
  val callSign: String,               // позывной/имя
  val lat: Double?,                   // null если GPS недоступен
  val lon: Double?,
  val altitudeM: Int?,
  val rssi: Int,                      // дБм последнего принятого пакета
  val snr: Int,
  val txPower: Int,
  val batteryPct: Int?,               // null если USB
  val isBusy: Boolean,                // сейчас передаёт?
  val isGpsValid: Boolean,
  val uptimeSec: Long,
  val lastSeenMs: Long,               // System.currentTimeMillis() при получении
  val beaconSeq: Int
)

LiveData (дополнительно к существующим):
  - peers: LiveData<List<Peer>>       // список устройств на канале
  - myLocation: LiveData<Location?>   // текущие координаты телефона

Методы (дополнительно):
  - onPeerSeen(peer: Peer)  → обновить peers (upsert по deviceId), удалять если
                              lastSeen > 3 × beaconInterval (устарел)
  - requestMyLocation()     → FusedLocationProviderClient.getCurrentLocation()
  - onLocationRequested()   → ответить ESP32 командой 0x15 LOCATION_UPDATE
  - getPeersOnMap(): List<Peer>  → только peers с isGpsValid=true
```

---

## 6. Codec2 JNI (android/jni/)

### CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.22)
project(codec2jni)

# Исходники Codec2 (скопировать из репозитория drowe67/codec2)
set(CODEC2_SOURCES
    codec2/codec2.c
    codec2/kiss_fft.c
    codec2/nlp.c
    codec2/lpc.c
    codec2/postfilter.c
    codec2/quantise.c
    codec2/pack.c
    codec2/sine.c
    codec2/phase.c
    codec2/interp.c
    codec2/lsp.c
    codec2/mbest.c
)

add_library(codec2jni SHARED codec2_jni.cpp ${CODEC2_SOURCES})
target_include_directories(codec2jni PRIVATE codec2/)
target_link_libraries(codec2jni android log)
```

### codec2_jni.cpp — экспортируемые функции
```cpp
// Java_com_meshtrx_app_Codec2Wrapper_nativeInit(mode: Int): Long  → handle
// Java_com_meshtrx_app_Codec2Wrapper_nativeEncode(handle: Long, pcm: ShortArray): ByteArray
// Java_com_meshtrx_app_Codec2Wrapper_nativeDecode(handle: Long, encoded: ByteArray): ShortArray
// Java_com_meshtrx_app_Codec2Wrapper_nativeFree(handle: Long)
// mode = 0 → CODEC2_MODE_1200
```

---

## 7. Скрипты автоматизации (scripts/)

### install_deps.sh
```bash
Должен установить:
  1. PlatformIO Core (pip install platformio)
  2. Android SDK командной строки (если нет ANDROID_HOME)
  3. Gradle wrapper
  4. Скачать Codec2 исходники (git clone https://github.com/drowe67/codec2)
     и скопировать нужные .c и .h файлы в firmware/lib/codec2/ и android/jni/codec2/
  5. Проверить наличие: python3, pip, java 17+, git, adb
  6. Вывести итоговый статус: что установлено, что отсутствует
```

### flash_firmware.sh
```bash
Аргументы: [PORT] (по умолчанию auto-detect /dev/ttyUSB* или /dev/ttyACM*)
Шаги:
  1. Найти подключённый Heltec (lsusb | grep CP2102 → определить порт)
  2. cd firmware && pio run --target upload --upload-port $PORT
  3. После прошивки: pio device monitor --port $PORT --baud 115200
Использование: ./scripts/flash_firmware.sh
               ./scripts/flash_firmware.sh /dev/ttyUSB0
```

### build_android.sh
```bash
Шаги:
  1. cd android/MeshTRX
  2. ./gradlew assembleDebug
  3. Если подключён телефон (adb devices): предложить установить APK
     adb install app/build/outputs/apk/debug/app-debug.apk
  4. Вывести путь к APK файлу
```

---

## 8. Задержка и производительность

```
Цепочка передачи (оценка, BW=250 кГц SF7):
  Микрофон → AudioRecord буфер:    40 мс (один Codec2 фрейм)
  Накопление 6 фреймов:           240 мс
  BLE Write:                       10–20 мс
  LoRa TX (54 байт, SF7 BW250):    ~37 мс  (было ~15 мс при BW500)
  LoRa RX + обработка:             5 мс
  BLE Notify:                      10–20 мс
  AudioTrack jitter buffer:        120 мс (3 пакета)
  Codec2 decode + воспроизведение: 40 мс
  ─────────────────────────────────────────
  Итого сквозная задержка:        ~520 мс  (было ~500 мс при BW500)

Разница BW500 vs BW250: +22 мс — в PTT режиме незаметна.
Выигрыш: 23 канала вместо 11.
Для уменьшения задержки: уменьшить jitter buffer до 1 пакета (рискованно при плохом сигнале).
```

---

## 9. Порядок реализации (актуальный статус)

```
=== ВЫПОЛНЕНО ===

Шаг 1: Прошивка ESP32 (firmware/) ✓
  → packet.h, lora_radio, ble_service, oled_display, audio_codec
  → vox, roger_beep, beacon, repeater, call_manager, main.cpp
  → Codec2 1200bps (6 байт/фрейм, 8 фреймов/пакет)
  → BLE PIN авторизация на уровне приложения (0x25/0x26)
  → OLED: авто-сон 60с, батарея, PIN по кнопке USER
  → LED: TX=горит, RX=вспышка, BLE=5с, idle=выключен
  → Позывной задаётся с телефона через SET_SETTINGS callsign

Шаг 2: Android приложение (android/MeshTRX/) ✓
  → Архитектура: MeshTRXService (Foreground Service) + ServiceState singleton
  → 4 вкладки: VoiceFragment, MessagesFragment, FilesFragment, SettingsFragment
  → BleManager: scan, connect, PIN check, auto-connect
  → AudioEngine: запись/воспроизведение 8kHz, Codec2 JNI, RMS
  → VoxEngine: state machine IDLE→ATTACK→ACTIVE→HANGTIME
  → CallPickerSheet: список peers с сортировкой (RSSI/имя/расстояние)
  → Recent calls: последние 5 вызовов с redial
  → Peers persistence: сохранение/загрузка, cleanup по таймауту
  → Позывной: задаётся в Settings, сохраняется + отправляется на ESP32

Шаг 3: Дизайн UI ✓
  → Тёмная тактическая тема (#141414) по MESHTRX_DESIGN_SPEC.md
  → PttButtonView: кастомная круглая кнопка с состояниями IDLE/TX/RX/VOX
  → Анимация волн: расходящиеся круги при TX пропорционально RMS
  → Colors.kt: полная палитра (green/blue/red/amber semantic)
  → Шапка: позывной крупно + имя устройства полутоном
  → RSSI цветовой индикатор, кнопки вызовов стилизованные

=== В РАБОТЕ / ПЛАН ===

Шаг 4: Foreground Service фоновое аудио
  → Аудио продолжает играть при свёрнутом приложении
  → Notification actions: Mute, Disconnect

Шаг 5: Система вызовов в Android
  → IncomingCallActivity (полноэкранный, поверх lockscreen)
  → Зумер/вибрация через CallRinger, авто-закрытие через 10 сек
  → Одна кнопка OK (закрыть + стоп зумер)
  → Режимы: "Слушать всех" / "Только мои" с управлением громкостью

Шаг 6: Roger Beep (асинхронный)
  → Отправка через очередь, не из BLE callback

Шаг 7: Передача файлов/фото
  → ImageProcessor, FileTransfer protocol

Шаг 8: Карта + GPS
  → MapActivity (osmdroid), маркеры peers, Haversine

Шаг 9: scripts/
  → install_deps.sh, flash_firmware.sh, build_android.sh

Шаг 2: firmware/
  → packet.h (структура пакета + флаги VOX/ROGER_BEEP)
  → lora_radio.h/.cpp (RadioLib SX1262, ENFORCE_DUTY_CYCLE, TX_POWER)
  → ble_service.h/.cpp (NimBLE NUS, команды 0x01–0x13)
  → oled_display.h/.cpp (U8g2, VOX статус на дисплее)
  → audio_codec.h/.cpp (обёртка Codec2)
  → vox.h/.cpp (VOX state machine)
  → roger_beep.h/.cpp (генерация PCM тонов)
  → beacon.h/.cpp (beaconTask, CRC16)
  → repeater.h/.cpp (repeaterTask, дедупликационный кеш)
  → call_manager.h/.cpp (система вызовов, все типы, OLED индикация, NVS группы)
  → main.cpp (FreeRTOS задачи, главный цикл; ветвление по REPEATER_MODE)
  → platformio.ini

Шаг 3: android/jni/
  → Скопировать Codec2 исходники
  → codec2_jni.cpp
  → CMakeLists.txt

Шаг 4: android/MeshTRX/
  → build.gradle (project + app)
  → AndroidManifest.xml + res/xml/file_paths.xml
  → Codec2Wrapper.kt
  → AudioEngine.kt
  → BleManager.kt  (0x01–0x10)
  → DeviceSettings.kt / FileTransfer.kt / ChatMessage.kt
  → ImageProcessor.kt  (resize, JPEG compress, EXIF rotate)
  → MainViewModel.kt
  → MainActivity.kt (BottomNavigation, 4 фрагмента)
  → VoiceFragment.kt + fragment_voice.xml
  → ChatFragment.kt + fragment_chat.xml
  → ChatAdapter.kt (текст + фото-вложения в пузырьках)
  → FilesFragment.kt + fragment_files.xml
  → FileTransferAdapter.kt (список с прогресс-барами)
  → SettingsFragment.kt + fragment_settings.xml
  → PhotoPickerFragment.kt (выбор + предпросмотр + сжатие)
  → VoxEngine.kt (RMS расчёт, VOX state machine на стороне Android)
  → RogerBeepPlayer.kt (локальное воспроизведение тона)
  → VoxIndicatorView.kt (кастомный VU-meter View)
  → LocationManager.kt
  → GeoUtils.kt (Haversine)
  → Peer.kt / Group.kt / CallState.kt
  → CallManager.kt (логика вызовов, таймеры, состояния)
  → IncomingCallOverlay.kt (полноэкранный overlay входящего вызова)
  → SosActiveOverlay.kt (полноэкранный SOS экран)
  → CallPickerSheet.kt (выбор адресата из пиров/групп)
  → GroupEditSheet.kt (создание/редактирование группы)
  → PeersFragment.kt + fragment_peers.xml (список устройств)
  → PeersAdapter.kt (RecyclerView с RSSI барами и статус-кружками)
  → PeerDetailBottomSheet.kt (детали устройства)
  → MapActivity.kt + activity_map.xml (osmdroid карта)
  → ImagePreviewDialog.kt (полноэкранный просмотр принятых фото)

Шаг 5: scripts/flash_firmware.sh
Шаг 6: scripts/build_android.sh

Шаг 7: Проверка
  → Запустить flash_firmware.sh на первом Heltec
  → Запустить flash_firmware.sh на втором Heltec
  → Запустить build_android.sh → установить APK
  → Протестировать связь
```

---

## 10. Известные сложности и решения

| Проблема | Решение |
|---|---|
| LoRa RX и TX не одновременно | PTT режим — один говорит, другой слушает |
| BLE MTU ограничен | Фрагментация пакетов по seq номеру, задержка 20мс |
| ESP32-S3 и Codec2 (float) | Использовать FPU, режим 1200bps минимально затратный |
| Jitter LoRa | Jitter buffer на 3 пакета в AudioEngine |
| Android BLE на разных версиях | minSdk 26, обрабатывать оба API |
| EU868 duty cycle 1% | ENFORCE_DUTY_CYCLE=false по умолчанию; управляется из настроек |
| TX мощность > 14 дБм нарушает EU | Предупреждение в UI при значениях 15–22 дБм |
| Текст теряется при плохом сигнале | ACK + повтор до 3 раз с таймаутом 2 сек |
| Коллизия аудио и файла | Файловая передача ждёт окончания PTT; приоритет: аудио > текст > файл |
| Большие фото не проходят | ImageProcessor сжимает до ≤20 КБ, предпросмотр с ползунком качества |
| Потеря чанка файла при LoRa RX | FILE_ACK с NACK → повтор чанка до 3 раз |
| ESP32 heap для файлового буфера | Использовать PSRAM если есть (Heltec V3 имеет 8МБ), иначе heap 200КБ макс |
| VOX срабатывает на фоновый шум | Порог регулируется; кнопка "Тест" в настройках помогает калибровать |
| VOX не отпускает TX при тишине | Hangtime 800мс по умолчанию; увеличить если обрезает конец фраз |
| Roger Beep перекрывает начало ответа | Beep короткий (150мс); принимающая сторона слышит его до ответа |
| LoRa broadcast — PRIVATE CALL слышат все | Адресность только в вызове; содержание голоса broadcast — документировать пользователю |
| Случайный SOS | Двойное подтверждение в APP + 5 сек удержание кнопки на плате |
| Два входящих вызова одновременно | Приоритет: EMERGENCY > PRIVATE > GROUP > ALL; второй вызов в очередь |
| GROUP CALL без синхронизации групп | Группы хранятся локально; участники видят вызов по своему device_id в members[] |
| CALL_TIMEOUT если адресат вне зоны | 30 сек таймаут + уведомление "Нет ответа" |
| Ретранслятор слышит свой TX | Half-duplex SX1262 — пока TX, RX выключен; пакеты в этот момент теряются |
| Петля пакетов A→R1→R2→R1→... | TTL декремент + дедупликационный кеш 64 записи × 30 сек |
| Переключение режима без телефона | Удержание USER кнопки > 3 сек → переключение + перезагрузка |
| Ретранслятор не знает координат | GPS_VALID=false в beacon; на карте показывается без позиции |
| Координаты телефона устарели | ESP32 запрашивает свежие через 0x14 перед каждым пингом |
| GPS недоступен / выключен | флаг GPS_VALID=false, координаты 0; на карте не показываем |
| Много устройств → много пингов | Случайный jitter ±15% от interval, чтобы не коллидировать |
| Устаревшие peers в списке | Удалять если lastSeen > 2 × beaconInterval, но не раньше 10 мин |
| osmdroid без интернета | Кешировать тайлы при первом запуске; офлайн режим по кнопке |

---

## 11. Тестирование

```
Тест 1 — Loopback (одно устройство):
  - Прошить одно устройство
  - Подключить телефон по BLE
  - Отправить тестовый пакет через BLE → ESP32 должен его отправить в LoRa
    и принять обратно (SX1262 поддерживает приём собственных пакетов при настройке)
  - Проверить что данные вернулись на телефон

Тест 2 — Два устройства в одной комнате:
  - Прошить оба устройства
  - Подключить два телефона
  - PTT на телефоне A → голос должен выйти из телефона B
  - Отправить текстовое сообщение с телефона A → появится на телефоне B + ACK
  - Проверить RSSI на OLED и в приложении
  - Сменить канал на обоих устройствах → убедиться что связь работает на новом канале

Тест 3 — Дальность:
  - Одно устройство на балконе/улице
  - Второе в помещении
  - Проверить связь, записать RSSI
```

---

## 12. Режимы прослушивания и система вызовов (v2)

### 12.1 Два режима приёма

```
РЕЖИМ 1: "Слушать всех" (по умолчанию)
  - Динамик на нормальной громкости
  - Слышим все переговоры в канале
  - Работает в фоне как медиаплеер (Foreground Service)
  - При свёрнутом приложении — аудио продолжает воспроизводиться
  - ALL CALL → короткий звук + notification
  - PRIVATE/GROUP для меня → зумер + overlay
  - EMERGENCY → всегда: полный экран + максимальная громкость

РЕЖИМ 2: "Только мои вызовы" (тихий режим)
  - Громкость канала = 0 (пакеты принимаются, аудио не воспроизводится)
  - Приложение продолжает принимать пакеты в фоне
  - При входящем PRIVATE CALL / GROUP CALL для нашего device_id:
      * Зумер вызова (если звук) или вибрация (если телефон на вибрации)
      * Громкость динамика автоматически поднимается
      * Overlay "Входящий вызов" на экране
      * После принятия → слышим канал нормально
      * После завершения (10 сек тишины) → обратно в тихий режим
  - ALL CALL → игнорируется (без звука)
  - EMERGENCY → ВСЕГДА прорывается: полный экран + максимальная громкость/вибрация
```

### 12.2 Приоритет звука вызовов

```
EMERGENCY (SOS)   → всегда: максимальная громкость или вибрация, полный экран
PRIVATE CALL      → всегда: зумер/вибрация (по настройке телефона), overlay
GROUP CALL (для меня) → всегда: зумер/вибрация, overlay
ALL CALL          → только в режиме "Слушать всех": короткий звук, notification
Обычное аудио     → только в режиме "Слушать всех": нормальная громкость
```

### 12.3 Зумер/вибрация входящего вызова

```
Определяется автоматически по AudioManager:
  - Телефон в обычном режиме → звуковой зумер (рингтон)
  - Телефон на вибрации → вибрация паттерн (как телефонный звонок)
  - Телефон в беззвучном → вибрация (для PRIVATE/EMERGENCY)

Паттерны вибрации:
  PRIVATE CALL:   [0, 500, 200, 500, 200, 500]  (три импульса)
  GROUP CALL:     [0, 300, 200, 300]              (два импульса)
  EMERGENCY/SOS:  [0, 200, 100] × непрерывно      (тревога)
  ALL CALL:       [0, 200]                         (один короткий)
```

### 12.4 Foreground Service (MeshTRXService)

```
Задачи:
  - Держит BLE соединение при свёрнутом приложении
  - Продолжает декодировать и воспроизводить аудио
  - Управляет AudioFocus (другие приложения приглушаются)
  - Persistent notification: "MeshTRX — CH:0 · Подключено · Слушаю всех"
  - Notification actions: [Mute] [Disconnect]
  - При входящем вызове — heads-up notification с кнопками [Принять] [Отклонить]

Жизненный цикл:
  - Запускается при подключении BLE
  - Останавливается при отключении BLE
  - startForeground() с FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE + MICROPHONE
```

### 12.5 Процесс вызова

```
ОТПРАВИТЕЛЬ:
  1. Нажимает "Вызвать" → выбор типа:
     - "Общий канал" (ALL CALL) → сообщение опционально
     - "Личный вызов" → выбор адресата из списка peers
     - "Группа" → выбор группы
     - "SOS" → двойное подтверждение
  2. Список peers сортируется: по RSSI (сила сигнала) и по расстоянию (GPS)
  3. ESP32 отправляет пакет вызова в LoRa
  4. Для PRIVATE: ожидание ACCEPT/REJECT/TIMEOUT (30 сек)

ПРИНИМАЮЩИЙ:
  1. ESP32 получает пакет вызова → отправляет на телефон (BLE 0x1F)
  2. Android проверяет: тип вызова + режим прослушивания + адресат
  3. Решение:
     - Показать overlay (PRIVATE/GROUP для меня, EMERGENCY)
     - Показать notification (ALL CALL в режиме "Слушать всех")
     - Игнорировать (ALL CALL в режиме "Только мои")
  4. Пользователь принимает/отклоняет
  5. ESP32 отправляет ACCEPT/REJECT в LoRa
```

---

## 13. Структура Android приложения (v2 — мультиэкран)

### 13.1 Навигация: BottomNavigation с 4 вкладками

```
┌─────────────────────────────────────────────┐
│              [Содержимое вкладки]            │
│                                              │
├──────┬──────────┬────────┬──────────────────┤
│ PTT  │ Сообщ.   │ Файлы  │ Настройки        │
└──────┴──────────┴────────┴──────────────────┘
```

### 13.2 Вкладка 1: PTT (Голос)

```
Тёмная тема (#141414), дизайн по MESHTRX_DESIGN_SPEC.md

┌─────────────────────────────────────────────┐
│ [ПОЗЫВНОЙ]              ● Подключено        │  ← шапка #1a1a1a
│ MeshTRX-7E88                                 │  ← имя устройства #555555
├─────────────────────────────────────────────┤
│ [CH 0 — 863.15 MHz  ▼]  [PTT ○ VOX]        │  ← одинаковая высота 40dp
├─────────────────────────────────────────────┤
│                                              │
│          ╔═══════════════╗                  │
│          ║               ║                  │
│          ║   ГОВОРИТЬ    ║  PttButtonView   │
│          ║   (кастомная) ║  200×200dp круг  │
│          ║               ║  волны при TX    │
│          ╚═══════════════╝                  │
│                                              │
│         ● ожидание  ← строка статуса        │
│                                              │
│  ○ Слушать всех    ○ Только мои             │
│                                              │
│  [ОБЩИЙ]  [ВЫЗВАТЬ]  │  [  SOS  ]          │
│                                              │
│  ПОСЛЕДНИЕ                                   │
│  ┌─ ▌ TX-6770    17:01 · Лич · -24dBm  📞 ┐│
│  ├─ ▌ Общий      16:44 · Общ             📞 ┤│
│  └─ ▌ SOS        16:30 · SOS             📞 ┘│
├─────────────────────────────────────────────┤
│ [PTT]  [Чат]  [Файлы]  [Настройки]         │  ← навбар #111111
└─────────────────────────────────────────────┘

PttButtonView состояния:
  IDLE:     зелёный (#1a3a1a), надпись "ГОВОРИТЬ"
  TX:       красный (#4a0a0a), надпись "ПЕРЕДАЧА", волны по RMS
  RX:       синий (#0a2a4a), надпись "ПРИЁМ"
  VOX_IDLE: зелёный, надпись "VOX"
  VOX_TX:   красный, надпись "VOX TX", волны по RMS

"Вызвать" → CallPickerSheet (BottomSheet):
  Табы: [Станции] [Группы]
  Сортировка: [По сигналу] [По имени] [По расстоянию]
  Список: позывной, RSSI бар, статус цвет, время, кнопка вызова

VOX настройки (порог, задержка) — в экране Настройки
```

### 13.3 Вкладка 2: Сообщения

```
┌─────────────────────────────────────────────┐
│ Сообщения           [● Всем] [○ Личные]     │
│─────────────────────────────────────────────│
│ (список чатов / общий канал)                 │
│                                              │
│  14:32 TX-6770: Привет! [-75dBm]            │
│  14:33 → Я: Принял                          │
│  14:35 TX-A1B2: Как слышно? [-88dBm]        │
│                                              │
│─────────────────────────────────────────────│
│ Личные сообщения → выбор адресата из peers:  │
│   [TX-6770 ▼] [Сообщение...        ] [>]    │
│                                              │
│ Канальные → без адресата:                    │
│   [Сообщение (макс 84)...           ] [>]    │
└─────────────────────────────────────────────┘
```

### 13.4 Вкладка 3: Файлы

```
┌─────────────────────────────────────────────┐
│ Файлы               [Отправить ▼]           │
│─────────────────────────────────────────────│
│ Кому: [● Всем] [○ Выбрать станцию ▼]       │
│                                              │
│ [📷 Фото]  [📎 Файл]                        │
│                                              │
│ История:                                     │
│ ← photo_001.jpg  12KB  ✓  TX-6770  14:20   │
│ → report.txt      2KB  ✓  →всем    14:18   │
│ ← photo_002.jpg  18KB  ▇▇▇░ 65%   TX-A1B2 │
└─────────────────────────────────────────────┘
```

### 13.5 Вкладка 4: Настройки

```
┌─────────────────────────────────────────────┐
│ Настройки                                    │
│─────────────────────────────────────────────│
│ Станция                                      │
│   Имя/позывной: [TX-7E88    ] (макс 8)      │
│   Device ID: DB527E88                        │
│   PIN: 2392                                  │
│                                              │
│ Радио                                        │
│   Мощность TX: [▇▇▇▇▇░░░░] 14 dBm          │
│   Duty Cycle EU868: [OFF]                    │
│   Регион: [EU868 ▼]                          │
│                                              │
│ VOX                                          │
│   Порог: [▇▇▇░░░░░░░] 800                   │
│   Задержка: [▇▇▇▇░░░░░] 800 мс             │
│                                              │
│ Beacon                                       │
│   Интервал: [5 мин ▼]                        │
│   GPS в пинге: [ON]                          │
│                                              │
│ Roger Beep                                   │
│   Тип: [● Короткий ○ Морзе ○ Two-tone]      │
│                                              │
│ [Применить и сохранить]                      │
└─────────────────────────────────────────────┘
```

---

## 14. Этапы реализации v2

```
ЭТАП 1: Foreground Service + фоновое аудио
  → MeshTRXService (Foreground Service)
  → Перенос BLE/Audio логики в Service
  → Notification с контролами
  → Аудио продолжает играть при свёрнутом приложении

ЭТАП 2: Мультиэкран (4 вкладки)
  → BottomNavigationView + Fragments
  → VoiceFragment (PTT/VOX/канал/вызовы)
  → MessagesFragment (чат)
  → FilesFragment (передача файлов)
  → SettingsFragment (конфигурация)

ЭТАП 3: Режимы прослушивания
  → Переключатель "Слушать всех / Только мои"
  → Программное управление громкостью (AudioManager)
  → Автоматический подъём громкости при PRIVATE вызове

ЭТАП 4: Система вызовов в Android
  → CallManager на Android стороне
  → IncomingCallOverlay (полноэкранный)
  → Зумер/вибрация по состоянию AudioManager
  → PeerPickerSheet (список станций с сортировкой)
  → Notification actions для вызовов

ЭТАП 5: Roger Beep (асинхронный)
  → Очередь для roger beep в firmware
  → Отправка после PTT_END без блокировки BLE

ЭТАП 6: Передача файлов/фото
  → ImageProcessor (resize, JPEG compress)
  → FileTransfer protocol через BLE→LoRa
  → Прогресс-бары, повтор при ошибке

ЭТАП 7: Карта + GPS
  → MapActivity (osmdroid OpenStreetMap)
  → Маркеры peers с RSSI
  → LocationManager + координаты в beacon
  → Расстояние Haversine
```

---

*Спецификация v2.0 — добавлены: режимы прослушивания (слушать всех / только мои), зумер/вибрация по настройке телефона, Foreground Service для фонового аудио, мультиэкранная навигация (4 вкладки), PeerPickerSheet с сортировкой, этапы реализации v2.*
