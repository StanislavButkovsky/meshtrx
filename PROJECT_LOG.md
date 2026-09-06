# MeshTRX — Лог проекта

> Файл обновляется автоматически по мере работы над проектом.
> Последнее обновление: 2026-08-28 (v4.4.4 в продакшене, мост в Telegram на сервере)

---

## Обзор

**MeshTRX** — децентрализованная голосовая mesh-сеть на LoRa + BLE.
- Домен: meshtrx.com
- Пакет Android: com.meshtrx.app
- BLE имя: MeshTRX-XXXX

---

## Аппаратура

- **Устройство**: Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)
- **MAC подключённого**: 10:51:DB:52:7E:88
- **USB порт**: /dev/ttyUSB0 (CP2102)
- **Android телефон**: Huawei HRX6R20C15006006 (подключён по USB, авторизован для ADB)

---

## Выполненные работы

### 1. Прошивка ESP32 (firmware/) — ГОТОВО ✓

**Дата**: 2026-03-17

**Файлы**:
- `platformio.ini` — PlatformIO конфиг (Heltec V3, RadioLib 6.6.0, NimBLE 1.4.3, U8g2 2.36.18, ArduinoJson 7.4.3)
- `src/packet.h` — все структуры пакетов LoRa (аудио 0xA0, текст 0xB0, файлы 0xC0-C3, beacon 0xD0, вызовы 0xE0-E6)
- `src/lora_radio.h/.cpp` — SX1262, 23 канала (863.15-869.75 МГц, шаг 300 кГц), SF7, BW250
- `src/ble_service.h/.cpp` — NimBLE NUS, 36+ BLE команд (0x01-0x24)
- `src/oled_display.h/.cpp` — SSD1306 128x64, Vext GPIO36 питание, два режима + авто-сон 60 сек
- `src/audio_codec.h/.cpp` — обёртка Codec2 1200bps (6 байт/фрейм, 8 фреймов/пакет = 48 байт)
- `src/vox.h/.cpp` — VOX state machine (IDLE→ATTACK→ACTIVE→HANGTIME)
- `src/roger_beep.h/.cpp` — 5 типов тонов (временно отключён — вызывал BLE disconnect)
- `src/beacon.h/.cpp` — beacon задача, CRC16-CCITT, PEER_SEEN
- `src/repeater.h/.cpp` — Store&Forward ретранслятор, дедупликация 64×30сек
- `src/call_manager.h/.cpp` — ALL/PRIVATE/GROUP/EMERGENCY вызовы
- `src/main.cpp` — FreeRTOS задачи (LoRa Core 0, BLE Core 1, Beacon Core 1)
- `lib/codec2/` — Codec2 1.2.0 (минимальный набор для mode 1200) + все codebook-и

**Размер**: RAM 23.2%, Flash ~20%

**Исправления при сборке** (хронологически):
1. `codec2/version.h` — создан вручную
2. `beacon.h`, `call_manager.h` — `#include <stddef.h>` для `size_t`
3. `audio_codec.cpp` — `#include <Arduino.h>` для Serial
4. `vox.cpp` — `timerStart` → `voxTimerStart` (конфликт с ESP32 HAL)
5. `lora_radio.cpp` — убран `radio.setDutyCycle()` (не существует в RadioLib)
6. `ble_service.cpp` — callback-и NimBLE 1.4.x (`NimBLEConnInfo&` → `ble_gap_conn_desc*`)
7. `main.cpp` — `containsKey()` → `doc["key"].is<T>()` (ArduinoJson 7)
8. `platformio.ini` — `ARDUINO_USB_CDC_ON_BOOT=0`, `ARDUINO_LOOP_STACK_SIZE=16384`
9. `oled_display.cpp` — Vext GPIO36 LOW для питания OLED
10. `main.cpp` — `nvs_flash_init()` / `nvs_flash_erase()`
11. Codebook-и сгенерированы утилитой `generate_codebook` (codebook.c, codebookd.c, codebookjmv.c, codebookge.c, codebooknewamp1*.c, codebooknewamp2*.c)
12. `audio_codec.h` — FRAME_BYTES=6 (не 8), FRAMES_PER_PKT=8 (Codec2 1200 = 48 бит = 6 байт)
13. `main.cpp` — roger beep отключён при PTT_END (блокировал BLE → disconnect)
14. `main.cpp` — LED индикация: TX=горит, BLE=вспышка/5сек, нет BLE=мигание/300мс
15. `oled_display.cpp` — авто-сон экрана через 60 сек, `oledWake()`/`oledSleepTick()`/`oledOff()`
16. `main.cpp` — кнопка USER короткое нажатие = будит экран; OLED обновляется каждые 500мс из bleTask

### 2. Переименование MeshTalk → MeshTRX — ГОТОВО ✓

**Дата**: 2026-03-17

Заменено во всех файлах прошивки и Android. Позывной: MT- → TX-.

### 3. Android приложение (android/MeshTRX/) — ГОТОВО ✓

**Дата**: 2026-03-17

**Файлы**:
- `build.gradle` (project) — Gradle 8.5, Kotlin 1.9.22, AGP 8.2.2
- `app/build.gradle` — minSdk 26, targetSdk 34, NDK 26.1.10909125, Codec2 JNI CMake
- `AndroidManifest.xml` — BLE, Audio, Location, FileProvider
- `app/src/main/jni/CMakeLists.txt` — нативная сборка Codec2 (24 .c файла)
- `app/src/main/jni/codec2_jni.cpp` — JNI: init/encode/decode/free
- `app/src/main/jni/codec2/` — полная копия Codec2 + все codebook-и
- `Codec2Wrapper.kt` — Kotlin обёртка JNI (6 байт/фрейм, 8 фреймов/пакет)
- `BleManager.kt` — BLE scan/connect/disconnect, NUS, @Synchronized send(), все команды
- `AudioEngine.kt` — запись/воспроизведение 8kHz моно, Codec2, RMS, режимы PTT и VOX мониторинг
- `VoxEngine.kt` — VOX state machine на Android (IDLE→ATTACK→ACTIVE→HANGTIME)
- `MainViewModel.kt` — MVVM, LiveData, PTT/VOX режимы, peers, messages, BLE data handling
- `MainActivity.kt` — UI: PTT, VOX switch, VU-meter, порог/hangtime ползунки, чат, каналы
- `activity_main.xml` — layout с VOX контролами
- `res/values/` — темы Material, строки
- `res/xml/file_paths.xml` — FileProvider
- `gradlew` + `gradle-wrapper.jar` — Gradle wrapper 8.5

**Исправления при сборке**:
1. NDK 25 повреждён → указан NDK 26.1.10909125 в build.gradle
2. Недостающие codebook.c и codebookd.c (lsp_cb, lsp_cbd) — сгенерированы
3. `AudioEngine.kt` — `poll(TimeUnit, Long)` → `poll(Long, TimeUnit)`
4. `MainViewModel.kt` — `ByteArray.indexOf(Byte, Int)` → `indexOfFirst`
5. `Codec2Wrapper.kt` — FRAME_BYTES=6, FRAMES_PER_PACKET=8 (краш ArrayIndexOutOfBounds)
6. `MainViewModel.kt` — `sendPttEnd()` через Thread с задержкой 100мс (BLE disconnect fix)
7. `BleManager.kt` — `send()` @Synchronized + try-catch

**APK**: собирается, устанавливается через ADB, BLE подключение работает.

### 4. VOX система — ГОТОВО ✓

**Дата**: 2026-03-17

Реализован на стороне Android (рекомендация спека для v1):
- `VoxEngine.kt` — state machine: IDLE→ATTACK(50мс)→ACTIVE→HANGTIME(800мс)→IDLE
- `AudioEngine.kt` — режим `startVoxMonitoring()`: микрофон слушает постоянно, RMS идёт в VoxEngine, кодирование только когда `sendAudio=true`
- `MainViewModel.kt` — переключение PTT/VOX, callbacks onTxActivated/onTxDeactivated
- `MainActivity.kt` — SwitchMaterial PTT/VOX, VU-meter ProgressBar, SeekBar порога (0-5000) и hangtime (200-2000мс)
- В режиме VOX кнопка PTT скрывается, показываются VU-meter и настройки

### 5. OLED авто-сон + LED индикация — ГОТОВО ✓

**Дата**: 2026-03-17

**OLED**:
- Экран гаснет через 60 сек после последней активности (setPowerSave)
- Кнопка USER (GPIO0) короткое нажатие — будит экран на 60 сек
- Входящие сообщения/пиры автоматически будят экран (oledShowMessage вызывает oledWake)
- Обновление каждые 500мс из bleTask (канал, RSSI, SNR, BLE статус, PTT/VOX)

**LED (GPIO35)**:
- TX активен: горит постоянно
- BLE подключён: короткая вспышка каждые 5 сек
- Ожидание BLE: быстрое мигание каждые 300мс

**Кнопка USER (GPIO0)**:
- Короткое нажатие: включить OLED на 60 сек
- Удержание >3 сек: переключить нормальный/ретранслятор режим
- Удержание >5 сек (при SOS): отмена SOS

---

## Известные проблемы / TODO

1. **Roger Beep** — отключён, вызывает BLE disconnect (нужно отправлять асинхронно через очередь)
2. **NVS ошибки** — `nvs_open failed: NOT_FOUND` при первом запуске (не критично, пространства создаются при первой записи)
3. **Полный тест голоса** — требует второе устройство Heltec V3
4. **Android UI** — пока один экран, по спеку нужны 4 вкладки (Голос, Сообщения, Файлы, Сеть) + Settings + карта
5. **Передача файлов** — не реализовано в Android
6. **Карта OpenStreetMap** — не реализована
7. **Система вызовов** — реализована в firmware, UI в Android не сделан

---

## Установленные инструменты

- PlatformIO 6.1.19
- CMake (pip3)
- Android SDK: /home/datasub/Android/Sdk (build-tools 34/35, platforms 31/34/35, NDK 26, CMake 3.22.1)
- ADB: работает, телефон авторизован
- Gradle 8.5 (wrapper)

---

## Текущий план: следующие этапы

### ЭТАП: Фоновое аудио — СЛЕДУЮЩИЙ
- Аудио продолжает играть при свёрнутом приложении
- Notification actions: Mute, Disconnect

### ЭТАП: Система вызовов UI
- IncomingCallOverlay, зумер/вибрация
- Управление громкостью по режиму прослушивания

### ЭТАП: Roger Beep (асинхронный)
### ЭТАП: Передача файлов/фото
### ЭТАП: Карта + GPS

### 6. BLE PIN авторизация на уровне приложения — ГОТОВО ✓

**Дата**: 2026-03-18

- BLE security убран (NimBLE pairing не работал стабильно на Huawei)
- PIN проверяется на уровне приложения: ESP32 команда 0x25 PIN_CHECK / 0x26 PIN_RESULT
- PIN генерируется из MAC, показывается на OLED по нажатию кнопки USER
- Авторизованные устройства сохраняются в SharedPreferences (повторный PIN не нужен)
- Долгое нажатие "Подключить" → "Новое устройство" → сброс авторизаций → скан 5 сек → список → PIN
- Автоподключение к последнему устройству при запуске/onResume

### 7. Спецификация v2 обновлена — ГОТОВО ✓

**Дата**: 2026-03-18

Добавлено в MESHTRX_SPEC.md:
- §12: Режимы прослушивания (слушать всех / только мои)
- §12.2: Приоритет звука вызовов
- §12.3: Зумер/вибрация по настройке AudioManager
- §12.4: Foreground Service (MeshTRXService)
- §12.5: Процесс вызова (отправитель/принимающий)
- §13: Структура Android v2 — мультиэкран (4 вкладки с wireframes)
- §14: Этапы реализации v2 (7 этапов)

### 8. Архитектура v2: Service + 4 вкладки — ГОТОВО ✓

**Дата**: 2026-03-18

- MeshTRXService (Foreground Service) владеет BLE, Audio, VOX
- ServiceState singleton для service-to-UI коммуникации
- 4 вкладки: VoiceFragment, MessagesFragment, FilesFragment, SettingsFragment
- BottomNavigationView с show/hide фрагментов
- MainViewModel удалён, логика в Service
- model/Models.kt: BleState, TxMode, ListenMode, Peer, ChatMessage, RecentCall

### 9. CallPickerSheet + Recent Calls + Peer Persistence — ГОТОВО ✓

**Дата**: 2026-03-18

- CallPickerSheet BottomSheet: табы Станции/Группы, RecyclerView
- Сортировка: по сигналу / по имени / по расстоянию (Haversine)
- Peer из PEER_SEEN beacon (полные данные + координаты) и из текстовых сообщений (минимальные)
- Peers сохраняются в SharedPreferences, загружаются при старте
- Auto-cleanup по таймауту (настраивается 15мин-24ч, по умолчанию 60мин)
- Peers в группах защищены от cleanup
- Recent calls: последние 5 на Voice экране с redial кнопкой, сохраняются

### 10. Тёмная тактическая тема — ГОТОВО ✓

**Дата**: 2026-03-18

По MESHTRX_DESIGN_SPEC.md:
- Colors.kt: полная палитра цветов
- PttButtonView: кастомная круглая кнопка IDLE/TX/RX/VOX с анимацией расходящихся волн по RMS
- VOX: на кнопке надпись "VOX", волны при голосе, настройки перенесены в Settings
- Шапка: позывной крупно (#e8e8e8) + имя устройства полутоном (#555555)
- RSSI цветовой (зелёный/жёлтый/красный)
- Кнопки: ОБЩИЙ(синяя) ВЫЗВАТЬ(зелёная) SOS(красная) с разделителем
- Все фоны #141414, навбар #111111

### 11. Позывной из настроек — ГОТОВО ✓

**Дата**: 2026-03-18

- Позывной задаётся в Settings → "Применить"
- Сохраняется в SharedPreferences + отправляется на ESP32 через SET_SETTINGS callsign
- ESP32 парсит callsign из JSON, сохраняет в NVS через beaconSetCallSign()
- Шапка показывает пользовательский позывной, не сгенерированный TX-XXXX
- При запуске загружается автоматически

### 12. Система входящих вызовов — ГОТОВО ✓

**Дата**: 2026-03-18

- IncomingCallActivity: полноэкранный overlay поверх экрана блокировки
  - PRIVATE: кнопки Принять/Отклонить, зелёный акцент
  - ALL: кнопка Закрыть, синий акцент
  - GROUP: Принять/Отклонить, жёлтый акцент
  - EMERGENCY: красный экран, SOS, координаты, нельзя закрыть back
- CallRinger: зумер/вибрация по режиму AudioManager
  - Normal → рингтон + вибрация
  - Vibrate → только вибрация
  - Silent → вибрация для PRIVATE/EMERGENCY, ничего для ALL
  - Паттерны: EMERGENCY тревога, PRIVATE 3 импульса, GROUP 2, ALL 1
- Парсинг CMD_INCOMING_CALL (0x1F) с фильтрацией по ListenMode
- Парсинг CMD_CALL_STATUS (0x20) для отмены
- acceptCall()/rejectCall() через BLE
- Audio RX учитывает callActive (PRIVATE_ONLY + принятый вызов → аудио играет)
- Таймер 30 сек авто-dismiss
- CallType enum, IncomingCall data class

### 13. Roger Beep асинхронный — ГОТОВО ✓

**Дата**: 2026-03-18

- Roger Beep отправляется из loraTask (Core 0), не из BLE callback
- PTT_END ставит флаг `rogerBeepPending`, loraTask подхватывает
- pcmBuf[6400] сделан static (был 12.8 КБ на стеке → краш)
- loraTask стек увеличен 8192 → 16384 (Codec2 encode ~4-6 КБ)
- Исправлен Guru Meditation "Stack canary watchpoint triggered (lora)"
- PTT touch: requestDisallowInterceptTouchEvent — ScrollView не крадёт ACTION_UP
- Протестировано 30+ циклов PTT без единого краша

### 14. Оба устройства прошиты v2.7 — ГОТОВО ✓

**Дата**: 2026-03-18

- MeshTRX-7E88 и MeshTRX-6770 на одинаковой прошивке
- Roger Beep, 16K стек loraTask, async beep, все исправления

### 15. Общий хедер + disabled состояния — ГОТОВО ✓

**Дата**: 2026-03-18

- Хедер (позывной + имя устройства + статус подключения) перенесён в activity_main.xml
- Виден на всех 4 вкладках, observers в MainActivity
- Убран дублирующий хедер из fragment_voice.xml
- Без BLE подключения disabled (серые):
  - PTT кнопка, VOX переключатель, кнопки вызовов (Voice)
  - Поле ввода + кнопка отправки (Messages)
  - Кнопка "Применить" (Settings)
- Кнопка подключения в Settings всегда активна

### 16. Передача файлов/фото — ГОТОВО ✓

**Дата**: 2026-03-18

- ImageProcessor.kt: resize 160x120, JPEG 40%, EXIF rotate
- FilesFragment: фото picker, файл picker, предпросмотр, отправка
- Протокол: FILE_START header + FILE_CHUNK (200 байт, 200мс интервал)
- Прогресс: локальное обновление каждый чанк (setValue на main thread)
- Повторная отправка обновляет существующую запись (не дублирует)
- Нажатие на строку: диалог с предпросмотром + Поделиться/Повторить/Удалить
- Share через FileProvider + стандартный Android intent
- История: сохраняется в SharedPreferences, загружается при старте
- Настройки: срок хранения 7/14/30/90 дней
- Кнопки retry/delete в строке списка
- Приём файлов: парсинг FILE_RECV и FILE_PROGRESS от ESP32

### 17. Хранение файлов на диске + адресная передача — ГОТОВО ✓

**Дата**: 2026-03-19

**Хранение данных**:
- Данные файлов сохраняются в `files/transfers/` (internal storage)
- Имя файла: `{timestamp}_{filename}` для уникальности
- `localPath` хранится в SharedPreferences вместе с метаданными
- При удалении из списка — файл удаляется с диска
- При истечении срока хранения — файлы удаляются автоматически
- Retry/Share работают после перезапуска (загрузка с диска через `loadData()`)

**Адресная передача файлов**:
- Поле `dest[2]` добавлено в `LoRaFileHeader` и `LoRaFileChunk` (packet.h)
- `0x0000` = broadcast, иначе последние 2 байта MAC получателя
- ESP32 фильтрует: игнорирует пакеты не для себя и не broadcast
- Android: `FileDestPickerSheet` (BottomSheet + RecyclerView) — выбор "Всем" или peer
- Повторная отправка сохраняет адресат автоматически
- В деталях файла показывается кому/от кого

**Улучшения файлов**:
- Фото превью: 320×426, JPEG 70%, без метаданных (EXIF)
- Имя фото берётся из галереи (оригинальное имя файла)
- Лимит файла: 100 КБ, пауза между чанками: 100мс
- 60 КБ передаётся за ~35 сек (~1.7 КБ/с)
- LED мигает при передаче файлов (firmware: fileTxActive/fileRxActive)
- Кнопки retry/delete убраны из строки списка (остались в деталях)
- Превью в деталях: масштабируется пропорционально (70% ширины, 40% высоты экрана)
- Отображается для всех изображений (по расширению .jpg/.png/.webp/.bmp)

### 18. Мессенджер — полная переработка — ГОТОВО ✓

**Дата**: 2026-03-19

**UI**:
- RecyclerView с пузырями сообщений (chat bubbles)
- Исходящие: справа, синий фон (#1a3a5c), закруглённые углы (хвостик справа)
- Входящие: слева, тёмно-серый фон (#2a2a2a), позывной отправителя, хвостик слева
- Тёмная тема: #141414 фон, #111111 панели
- Кнопка отправки: зелёная с иконкой треугольника (ic_send)
- Кнопка "Кому": синяя с иконкой @ (как кнопка "ОБЩИЙ")

**Адресная отправка сообщений**:
- Кнопка `@` → FileDestPickerSheet (BottomSheet) → выбор "Всем" или peer
- "Кому: ..." отображается над полем ввода
- В мета-строке пузыря: "→ TX-ABCD" для адресных

**Фильтр**:
- Spinner сверху: "Все" + список контактов из истории
- Фильтрует по отправителю (входящие) или получателю (исходящие)
- Автообновляется при новых сообщениях

**Persistence**:
- Сообщения сохраняются в SharedPreferences (tab-delimited)
- Загружаются при старте сервиса
- Очистка по дате: 30 дней (настраивается)
- Поля: id, text, isOutgoing, senderId, senderName, destId, destName, rssi, status, time, timeMs

### 19. Карта + Радар + GPS — ГОТОВО ✓

**Дата**: 2026-03-19

**5-я вкладка** "Карта" с двумя режимами:

**Режим Карта (osmdroid)**:
- OpenStreetMap тайлы, автокеширование для оффлайн
- Маркер "Я" (GPS), маркеры peers с позывными и RSSI
- Пунктирные линии до peers с цветом по давности
- FAB: "Я" (центр + zoom 16), "Все" (zoom to fit)
- Автоцентр при первом получении GPS

**Режим Радар (кастомный Canvas)**:
- Тактический стиль: чёрный фон, зелёные кольца
- Peers по реальному азимуту и расстоянию (Haversine + bearing)
- Компас: TYPE_ROTATION_VECTOR, стороны света С/Ю/З/В
- Автомасштаб по самому дальнему peer
- Пульсирующая точка в центре
- Размер точек по RSSI, цвет по давности

**GPS (LocationHelper.kt)**:
- Android LocationManager (не GMS — Huawei без Play Services)
- GPS + Network провайдеры, 15 сек интервал
- Координаты в ServiceState.myLat/myLon
- GPS статус в углу на обоих режимах

### 20. Локализация ru/en — ГОТОВО ✓

**Дата**: 2026-03-19

- values/strings.xml (English) + values-ru/strings.xml (Русский)
- Переключатель языка в Settings → activity.recreate()
- LocaleHelper: attachBaseContext для runtime locale
- Все экраны: PTT, Chat, Files, Map/Radar, Settings, CallPicker, IncomingCall
- По умолчанию: русский

### 21. Файловая передача — полная отладка — ГОТОВО ✓

**Дата**: 2026-03-19

Баги найдены и исправлены при тестировании между двумя устройствами:
- **fileSessionId** не сохранялся отправителем → чанки отклонялись (v3.4)
- **loraStartReceive()** отсутствовал после приёма чанка → радио застревало
- **ACK убран** — блокировал приём следующего чанка
- **CHUNK_SIZE 200→120** — не влезал в BLE MTU 128, данные обрезались
- **BLE протокол** CMD_FILE_DATA для чанков данных ESP32→телефон
- **BLE отправка из bleTask** — из loraTask вызывало WDT crash
- **Bitmap уникальных чанков** — исключает подсчёт дубликатов
- **Имя файла** обрезается с сохранением расширения (19 символов)
- **calloc** вместо malloc, таймаут 10 сек на fileRxActive
- Интервал чанков: 100мс (оптимум, 98/98 без потерь)
- 11КБ фото передаётся за ~10 сек, данные целые

### 22. Голос — громкая связь, roger beep, громкость, шумоподавление — ГОТОВО ✓

**Дата**: 2026-03-19

- **Кнопка громкой связи** на PTT экране (круглая, зелёная/серая), по умолчанию вкл
- **Roger beep** воспроизводится локально на телефоне (80мс двухтональный)
  - ESP32 отправляет LoRa пакет с PKT_FLAG_PTT_END при отпускании PTT
  - Телефон получатель воспроизводит тон при получении флага
  - Не кодируется через Codec2, не искажается
- **Громкость приёма** — слайдер в Settings (50%-300%, по умолчанию 200%)
  - volumeBoost в AudioEngine, усиливает PCM данные
- **Шумоподавление** — тишина не передаётся (RMS порог 150)
- **SOS убран** — дублировал общий вызов
- BLE аудио формат: cmd + flags + senderId + payload (68 байт)

### 23. Codec2 3200, громкость, PTT RMS настройка — ГОТОВО ✓

**Дата**: 2026-03-20

**Codec2 1200 → 3200** — значительное улучшение качества речи:
- Фрейм: 160 сэмплов (20мс) / 8 байт (было 320/40мс/6 байт)
- Payload: 64 байт (было 48), пакет LoRa: 71 байт (было 55)
- Задержка: 160мс на пакет (было 320мс) — вдвое меньше
- Airtime: ~20мс (было ~15мс) при BW250/SF7
- codebook.c и codebookd.c добавлены в firmware (нужны для mode 3200)

**Громкость исправлена** — AudioTrack переключён с USAGE_VOICE_COMMUNICATION на USAGE_MEDIA:
- Убирает привязку к тихому STREAM_VOICE_CALL
- Отключает AEC (echo cancellation), который давил уровень
- Теперь воспроизведение через медиа-стрим — громкость сопоставима с плеером

**PTT RMS** — управление порогом шумоподавления в Settings:
- Слайдер 0-1000, по умолчанию 0 (отключён)
- ru: "PTT RMS: N (отсечка шума)", en: "PTT RMS: N (noise gate)"

**Исправление roger beep** — PTT_END пакет (нулевой payload) больше не декодируется:
- Раньше Codec2 декодировал 48 нулей → шум перед тоном
- Теперь при isLastPacket сразу roger beep без декодирования

**Файлы изменены** (firmware):
- `audio_codec.h` — FRAME_SAMPLES 160, FRAME_BYTES 8, PKT_BYTES 64
- `audio_codec.cpp` — CODEC2_MODE_3200
- `packet.h` — payload[64]
- `main.cpp` — BLE буферы 68 байт, memcpy 64
- `lib/codec2/library.json` — CODEC2_MODE_3200_EN=1
- `lib/codec2/codebook.c`, `codebookd.c` — добавлены

**Файлы изменены** (Android):
- `Codec2Wrapper.kt` — MODE_3200, FRAME_SAMPLES 160, FRAME_BYTES 8
- `AudioEngine.kt` — MODE_3200, FRAME_SAMPLES 160, USAGE_MEDIA, squelchThreshold=0 var
- `MeshTRXService.kt` — размер аудио BLE пакета 68
- `codec2_jni.cpp` — case 1 → CODEC2_MODE_3200
- `CMakeLists.txt` — CODEC2_MODE_3200_EN=1
- `fragment_settings.xml` — слайдер PTT RMS
- `SettingsFragment.kt` — обработчик PTT RMS
- `strings.xml` (en/ru) — строка ptt_rms

**Тестирование**: качество голоса "даже очень хорошо", громкость на уровне плеера.

### 24. v3.8 — UI, ретранслятор, батарея, кнопка — ГОТОВО ✓

**Дата**: 2026-03-20

**1. PTT экран — уникальные вызовы + фиксированный лейаут:**
- Дедупликация без ограничения 60 сек — уникальные записи по deviceId+callType
- Корневой ScrollView → LinearLayout, PTT кнопка не скроллится
- RecyclerView с weight=1 скроллится отдельно, до 20 записей

**2. Канал перемещён в Settings, listen mode на PTT:**
- Spinner канала из PTT → Settings (секция "Радио")
- Частота + канал в шапке справа зелёным: "CH 5 · 864.65 MHz"
- Toggle "Все"/"Мои" вместо channel spinner на PTT экране
- RadioGroup listen mode убран

**3. Подсветка активного таба:**
- Color state list: selected=#4ade80, unselected=#444444

**4. Ретранслятор — полная реализация:**
- BLE команда 0x28 SET_REPEATER (enable, ssid, pass, static_ip)
- BLE работает в режиме ретранслятора (для управления через приложение)
- WiFi веб-монитор: SoftAP "MeshTRX-Repeater" или STA с статическим IP
- Веб-страница: uptime, канал, TX power, счётчики fwd/drop, RSSI, смена канала
- UI в Settings: SSID, пароль, статический IP, кнопки вкл/выкл
- Кнопка 3сек для включения ретранслятора убрана — только через приложение
- Подтверждающий диалог перед включением

**5. Батарея — единый модуль:**
- `battery.h/cpp` — единая функция для main и beacon
- GPIO37 enable, усреднение 8 замеров
- Множитель 5.32 (калибровка по реальным замерам: 4.09V/4.20V)
- OLED: 2 знака после запятой (4.09v вместо 4.1v)

**6. Кнопка на девайсе:**
- Короткое нажатие (<1с) — включить экран
- Длинное (>1с) — показать PIN + имя устройства 10 сек
- Кнопка >3с для ретранслятора — убрана

**Спецификация обновлена**: аудио пакет 71 байт (Codec2 3200), BLE аудио 68 байт, файловый хедер 29 байт, CHUNK_SIZE 120, BLE 0x28

---

### 25. Публичный сайт meshtrx.com — ГОТОВО ✓

**Дата**: 2026-03-21

**Сайт проекта** на Next.js 14 + TypeScript + Tailwind CSS:
- Домены: meshtrx.com, meshtrx.ru (+ www), SSL Let's Encrypt
- Static export (`output: 'export'`), хостинг на VPS (nginx)
- Тёмная тема (#141414 + #4ade80), шрифт JetBrains Mono
- Двуязычный интерфейс (RU/EN) с переключателем в хедере

**Страницы**:
- **/** — лендинг: Hero с SVG mesh-диаграммой, статистика, 6 фичей, оборудование, роадмап, CTA
- **/download/** — APK карточка, QR-код, инструкция, changelog
- **/flash/** — Web Serial wizard (esptool-js), прошивка Heltec V3 через браузер
- **/docs/** — USER_GUIDE рендерится через react-markdown, sticky TOC
- **/about/** — описание, лицензия, GitHub, Telegram

**Стек**: Next.js 14, esptool-js, qrcode.react, react-markdown, remark-gfm

**Файлы**: `web/src/` — 32 файла (app, components, lib, content)

### 26. Энергооптимизация прошивки + BLE реконнект + GPS beacon fix — ГОТОВО ✓

**Дата**: 2026-03-21

**Энергооптимизация firmware:**
- Батарея читается раз в 5 сек вместо каждые 500мс (кеш `getCachedBattery()`)
- OLED таймаут: 60→30 сек
- LED вспышка BLE: 100→70мс (30% короче)
- Калибровка батареи: множитель 5.32→5.55 (среднее по 2 устройствам)
- Serial логи за макросами LOG_D/LOG_F, отключаются флагом `-DNDEBUG`

**BLE авто-реконнект (Android):**
- При потере связи — автоматическое переподключение (до 10 попыток)
- Нарастающая задержка: 3, 5, 7, 10 сек
- Таймаут подключения 10 сек (раньше — бесконечное ожидание)
- `gatt.close()` при disconnect для освобождения BLE стека
- Статус "Переподключение #N через Xс..." в UI
- Ручной disconnect останавливает реконнект

**GPS в beacon (Android):**
- Добавлен обработчик `CMD_GET_LOCATION` (0x14) в MeshTRXService
- ESP32 запрашивает координаты перед отправкой beacon — теперь Android отвечает
- Peers с GPS-координатами отображаются на радаре и карте

### 27. v4.2 — Радар улучшения, GPS в вызовах, splash screen, иконка — ГОТОВО ✓

**Дата**: 2026-03-21

**Радар:**
- Кнопки масштаба +/− (правый нижний угол), диапазон 100м–100км
- Автомасштаб по дальней станции с ненулевым RSSI, по умолчанию 5 км
- Кнопка переключения контраста ☾/☀ (левый нижний угол): обычный и высококонтрастный режимы для улицы
- Яркость точек и текста по уровню RSSI (сильный = ярче, слабый = тусклее)
- Позывной отображается на точке абонента
- Точки фиксированного малого размера

**GPS в пакетах вызовов:**
- GPS координаты добавлены в LoRa пакеты ALL/PRIVATE/GROUP вызовов (lat_e7/lon_e7)
- Android отправляет GPS на ESP32 при подключении BLE и при каждом обновлении (раз в 10 сек)
- Исправлен баг: ESP32 отбрасывал GPS от телефона (проверка len < 12 вместо < 11)
- Peer добавляется/обновляется при входящем вызове, сообщении, файле и PTT_END
- Sender ID добавлен в BLE аудио пакет (66→68 байт) и файловый хедер (27→29 байт)

**Android UI:**
- Фиксированная книжная ориентация (portrait)
- Splash screen: камуфляжный фон + логотип с fade-in анимацией
- Иконка приложения из logo.png с прозрачным фоном (все плотности mdpi-xxxhdpi)
- Кнопки "Слушать всех"/"Только мои": autoSizeText 8-12sp (подстройка под экран)

---

## Исследования (2026-04-01)

### Roger Beep — мёртвый код в firmware

- `rogerBeepPending` объявлен, проверяется в loraTask (main.cpp:344), но **нигде не выставляется в true**
- Единственный живой вызов `sendRogerBeep()` — из VOX деактивации (main.cpp:1013)
- Roger beep на приёмной стороне реализован локально в Android (`AudioEngine.playEncodedPacket` при PKT_FLAG_PTT_END)
- **Решение**: удалить roger_beep.h/cpp и все связанные вызовы из firmware

### VOX на ESP32 — мёртвый код в firmware

- VOX state machine полностью реализована на стороне Android (`VoxEngine.kt`)
- Android считает RMS в `AudioEngine`, передаёт в `VoxEngine.process(rms)`
- При активации VOX Android сам отправляет PTT_START/PTT_END через BLE
- `BLE_CMD_VOX_LEVEL` (0x13) на ESP32 принимает RMS, но Android его не отправляет — мёртвый путь
- ESP32-сторона `vox.h/cpp` не задействована в реальном потоке данных
- **Решение**: удалить vox.h/cpp и все связанные обработчики из firmware main.cpp

### Качество голоса на расстоянии >300м

Проблема: речь рвётся, становится "как робот" на расстояниях >300м (пригород).
Причина: потеря LoRa пакетов → пропуски аудио по 320мс.

**Текущие параметры**: Codec2 3200 bps, 8 фреймов/пакет (320мс), BW250, SF7, CR 4/5, 71 байт/пакет, ~37мс airtime.

**Codec2 1200 bps**: протестирован на близком расстоянии — очень плохое качество речи, отклонён.

**Варианты улучшения**:
1. **CR 4/5 → 4/7** — больше FEC, airtime ~60мс (было 37), одна строчка кода
2. **8 → 4 фрейма/пакет** — потеря = 160мс (не 320), пакет 39 байт, быстрее airtime
3. **Редундантность** — в каждый пакет дублировать последний фрейм предыдущего (+8 байт)
4. **SF7 → SF8** — +3дБ чувствительности (~×1.5 дальности), airtime ×2
5. **BW250 → BW500** — НЕ поможет, -3дБ чувствительности (хуже дальность)

**Нужно проверить при тестировании**: RSSI/SNR на OLED при "роботизации", Serial лог — есть ли CRC ошибки.

---

### 28. v4.3 — Устойчивость, cleanup, надёжная доставка — ГОТОВО ✓

**Дата**: 2026-04-01

**Устойчивость голоса:**
- CR 4/5 → 4/7 (больше FEC, airtime ~25мс вместо ~18мс)
- 8 → 4 фрейма/пакет: payload 64→32 байт, пакет 71→39 байт, потеря = 80мс (было 320)
- Изменения: firmware (lora_radio.h, audio_codec.h, packet.h, main.cpp) + Android (Codec2Wrapper, AudioEngine, MeshTRXService)

**Хедер UI:**
- Иконка BLE (зелёная/серая) вместо текста "Подключено"/"Отключено"
- Батарея устройства: иконка с уровнем (full/75/50/25/empty) + напряжение, цвет по уровню
- STATUS_UPDATE расширен: 6-й байт = напряжение × 10

**PTT экран — инфо абонента:**
- Слева от PTT кнопки: позывной, device ID, RSSI/SNR при приёме аудио
- ServiceState: lastRxCallSign, lastRxDeviceId, lastRxRssi, lastRxSnr, isReceiving
- PTT кнопка переключается в RX состояние при приёме

**Удалён мёртвый код из firmware:**
- roger_beep.h/cpp — удалены (rogerBeepPending нигде не выставлялся в true)
- vox.h/cpp — удалены (VOX работает на стороне Android)
- Вычищены все ссылки из main.cpp (includes, переменные, settings, BLE handlers)
- RAM: 34.2% → 30.3% (высвободили ~13КБ)

**Надёжная доставка текстовых сообщений:**
- Адресные: LoRa ACK (PKT_TYPE_TEXT_ACK 0xB1), retry ×3 с таймаутом 2 сек, UI: SENDING→DELIVERED/FAILED
- Broadcast: blind repeat ×2 с рандомной задержкой 100-300мс
- Дедупликация входящих текстов: кеш 16 записей × 30 сек (sender+seq)
- LoRaTextPacket расширен: dest[2] для адресности
- BLE SEND_MESSAGE: новый формат [0x07, seq, dest_lo, dest_hi, text...]

**Файловая передача — только адресная + NACK:**
- Broadcast (dest=0x0000) убран из FILE_START и FILE_CHUNK
- NACK bitmap: после FILE_END получатель отправляет ACK (OK) или NACK (список до 50 пропущенных чанков)
- LoRaFileAck переработан: status + dest + missing_count + missing[50]
- Android: "Отправить всем" скрыт в FileDestPickerSheet для файлов (showBroadcast=false)

### 29. v4.3.1 — Адресные вызовы, callSign sync, UI фиксы — ГОТОВО ✓

**Дата**: 2026-04-02

**Критический баг: String.format byte sign extension:**
- `String.format("%02X", data[N])` для байтов >= 0x80 давал "FFFFFFXX" вместо "XX"
- Все deviceId были искажены, matching между peers/calls мог не работать
- Исправлено во всех 5 местах (CMD_AUDIO_RX, CMD_RECV_MESSAGE, CMD_PEER_SEEN, CMD_FILE_RECV, CMD_INCOMING_CALL): добавлено `.toInt() and 0xFF`
- При загрузке старые peers с "FFFFFF" в ID отфильтровываются

**Адресный вызов (CALL_PRIVATE) не доходил:**
- targetId хранился как 4 hex (2 байта MAC), при отправке padStart → `[00,00,XX,XX]`
- ESP32 сравнивает memcmp все 4 байта → не совпадало с реальным deviceId
- Исправлено: targetId теперь полный 8 hex (4 байта), FileDestPickerSheet возвращает полный deviceId через onSelectedFull callback
- Исправлены все зависимые места: redial, incoming call auto-target, isReceiving matching, destMac для PTT файлов

**CallSign синхронизация телефон ↔ ESP32:**
- Firmware GET_SETTINGS теперь возвращает `callsign` в JSON ответе
- Android при SETTINGS_RESP: сверяет callsign телефона с ESP32, если отличается — синхронизирует
- Если на телефоне пусто — берёт callsign с устройства
- В CMD_PEER_SEEN: сохраняет существующий callSign если beacon приходит с пустым полем
- Убрана двойная отправка callsign в SettingsFragment (было setCallSign + sendSettings)

**UI улучшения:**
- Экран входящего вызова: одна кнопка OK (закрыть + стоп зумер), таймаут 10 сек (было 30)
- PTT кнопка блокируется при приёме адресного голосового файла (новое состояние isReceivingFile)
- Текст под PTT возвращается в "ожидание" после воспроизведения голосового
- PTT кнопка переходит в RX при воспроизведении адресного голосового
- Позывной адресата обновляется live при получении beacon от peers
- Stale-индикатор в списке звонков: если peer не виден >15 мин — ✗ красным
- Кнопка "Очистить список абонентов" в настройках (peers + recent calls)
- Дедупликация recent calls: один ALL, один PRIVATE на deviceId

**Файлы**: MeshTRXService.kt, ServiceState.kt, VoiceFragment.kt, FileDestPickerSheet.kt, SettingsFragment.kt, IncomingCallActivity.kt, fragment_settings.xml, strings.xml (en/ru), firmware/main.cpp

### 30. Оптимизация энергопотребления (Power Optimization) — ГОТОВО ✓

**Дата**: 2026-04-03 — 2026-04-04

**Исследование**: полный анализ потребления всех компонентов (LoRa, CPU, BLE, OLED, Serial, PA).
Исходное idle: ~70-130 мА. Документация: `docs/POWER_OPTIMIZATION.md`, `docs/HELTEC_V4_SPECS.md`.

**Event-driven LoRa task (экономия 15-30 мА):**
- Заменён busy polling 1мс на `ulTaskNotifyTake()` — задача спит до прерывания DIO1
- ISR `onRxDone` будит задачу через `vTaskNotifyGiveFromISR()`
- При PTT active — таймаут 5мс (для TX очереди), иначе 50мс

**BLE tuning (экономия 7-12 мА):**
- Connection interval: 12мс/12мс → 60мс/100мс, slave latency 0→2
- Advertising: scan response отключён, interval увеличен (100-200мс)
- TX power: P9 → P6 (+6dBm вместо +9dBm)

**OLED Vext полное отключение (экономия 1-5 мА):**
- При sleep: `digitalWrite(36, HIGH)` — полностью снимает питание Vext
- При wake: `digitalWrite(36, LOW)` + delay 50мс перед инициализацией дисплея

**Serial conditional (экономия 5-10 мА при NDEBUG):**
- `Serial.begin()` обёрнут в `#ifndef NDEBUG`
- Создан `debug.h` с макросами LOG_D/LOG_F/DBG_PRINT
- В platformio.ini: раскомментировать `-DNDEBUG` для release

**Main loop delay (экономия 3-5 мА):**
- `delay(50)` → `delay(200)` — кнопка реагирует 200мс (приемлемо)

**LoRa sleep при отсутствии BLE (экономия ~5 мА):**
- BLE не подключен (и не ретранслятор) → LoRa SLEEP (~0 мА)
- Beacon TX продолжает раз в 5 мин (радио просыпается на ~50мс)
- При подключении BLE → мгновенный переход в continuous RX
- Ретранслятор исключён — всегда continuous RX

**Длинная преамбула 32 символа:**
- `LORA_PREAMBLE` увеличен с 8 до 32 для всех пакетов
- Добавляет ~12мс к airtime — пренебрежимо
- Обеспечивает совместимость с RX Duty Cycle (если активируется в будущем)

**GC1109 PA управление для V4 (экономия ~5-10 мА):**
- Добавлен GPIO7 (PA_FEM_POWER) — питание PA
- `loraPaEnable()` / `loraPaDisable()` — полное вкл/выкл PA
- PA автоматически выключается в SLEEP, включается при continuous RX
- При beacon TX из sleep: PA временно включается и выключается
- V3 не затронут (#ifdef BOARD_V4)

**LoRa RX Duty Cycle API (реализовано, не активировано):**
- `loraSetPowerMode(LORA_POWER_DUTY_CYCLE_RX)` — аппаратный duty cycle SX1262
- Оставлен на будущее для автономного ретранслятора без WiFi

**Итого:**
- V3 idle BLE connected: **~20-28 мА** (было ~70-130 мА, улучшение **4-5x**)
- V3 idle BLE disconnected: **~6-10 мА** (улучшение **10-15x**)
- V4 idle BLE connected: **~25-38 мА** (с PA management)
- V4 idle BLE disconnected: **~6-10 мА** (PA выключен)
- Время работы 1200мАч BLE connected: **~43-60 часов** (было ~9-17)

**Файлы**: debug.h (новый), main.cpp, lora_radio.h, lora_radio.cpp, ble_service.cpp, oled_display.cpp, beacon.cpp, call_manager.cpp

### 31. File Transfer v2 — буферизация на ESP32 — ГОТОВО ✓

**Дата**: 2026-04-06

**Проблема**: старый протокол терял ~30% чанков — телефон слал BLE→LoRa напрямую, ESP32 был прозрачным мостом без контроля.

**Решение**: файл загружается в RAM ESP32 (~230 КБ свободно), ESP32 автономно управляет LoRa отправкой с retry.

**Новый BLE протокол:**
- `FILE_UPLOAD_START` (0x30): телефон→ESP32, заголовок файла (тип, dest, размер, имя)
- `FILE_UPLOAD_DATA` (0x31): телефон→ESP32, чанки данных по 120 байт (BLE delay 50мс)
- `FILE_UPLOAD_STATUS` (0x32): ESP32→телефон, статус (ACCEPTED/BUSY/SENDING/DELIVERED/FAILED/NO_MEMORY)

**Автономная LoRa отправка (fileSendTask, Core 1):**
- FILE_START с длинной преамбулой 32 (loraSendWake)
- Чанки с паузой 50мс, стандартная преамбула 8
- FILE_END после последнего чанка
- Ожидание ACK/NACK до 30 сек
- При NACK: досылка пропущенных чанков (макс 3 раунда)
- При ACK: DELIVERED на телефон
- При таймауте: FAILED на телефон

**ACK/NACK надёжность:**
- ACK/NACK отправляются с длинной преамбулой (loraSendWake) — решило проблему потери ACK
- Кеш ответа на приёмнике (30 сек) — при повторном FILE_END переотправляет из кеша
- DELIVERED дублируется через bleTask (pendingUploadStatus)

**Radio mutex (loraRadioMutex):**
- SemaphoreHandle_t защищает loraSend() и loraStartReceive()
- Предотвращает SPI конфликт между fileSendTask (Core 1) и loraTask (Core 0)
- PTT голос и текст работают во время файловой передачи (mutex serializes)

**State machine:**
- FILE_STATE_IDLE → UPLOADING → SENDING → IDLE
- FILE_STATE_RECEIVING (приём из LoRa)
- BUSY при попытке отправки/приёма в занятом состоянии
- Приём имеет приоритет над отправкой

**Настраиваемый таймаут:**
- `file_timeout` в NVS (30-180 сек, по умолчанию 60)
- Доступен через JSON settings (GET/SET)
- Android: 120 сек таймаут по умолчанию

**UI:**
- Прогресс: ⏳ (песочные часы, жёлтый) при передаче
- Доставлено: ✓ (зелёный)
- Ошибка: ✗ (красный)
- Голосовые файлы (0x04, 0x05) скрыты из вкладки "Файлы"

**Тестирование:**
- 21 КБ фото: 179/179 чанков, 0 потерь, ACK подтверждён (RSSI -19)
- 23 КБ фото: 194/194 чанков, 0 потерь, ACK подтверждён (RSSI -31)
- Тест на минимальной мощности (1 dBm): доставлено успешно
- NACK при потере: 2 чанка пропущено → NACK отправлен (проверено)

**Файлы:**
- Firmware: main.cpp (fileSendTask, state machine, BLE handlers, ACK cache), lora_radio.h/cpp (mutex, loraSendWake), ble_service.h (0x30-0x32)
- Android: BleManager.kt (CMD_FILE_UPLOAD_*), MeshTRXService.kt (sendFile v2, UPLOAD_STATUS handler), FilesFragment.kt (⏳ иконка)
- Документация: docs/TASK_FILE_TRANSFER_V2.md

### 32. Голосовые сообщения (Voice Messages) — В РАБОТЕ

**Концепция**: кнопка 🎤 в чате → запись до 10 сек → Codec2 → отправка через файловый протокол → воспроизведение в чате.

**Размер**: 10 сек = 4 КБ (Codec2 3200), передача ~3.4 сек по LoRa.

**Решения**:
- Транспорт: FILE_TYPE_VOICE (0x04) через существующий файловый протокол
- UI: голосовые видны только в чате (не на вкладке "Файлы")
- Хранение: 30 дней (как файлы), .c2 файл на диске
- Запись: удержание кнопки (как Telegram)

### Запланированное

**Интеграционные тесты между двумя девайсами (Python скрипт):**
- Два ESP32 подключены по USB (/dev/ttyUSB0 + /dev/ttyUSB1)
- Python скрипт загружает тестовую прошивку на оба, управляет через Serial
- Тесты: LoRa TX→RX (отправить пакет, проверить приём), файловая передача end-to-end,
  голосовой PTT (отправить аудио пакеты, проверить приём), вызовы (CALL_ALL, CALL_PRIVATE),
  beacon visibility, NACK retry при потерях
- Скрипт читает Serial обоих устройств, парсит логи, проверяет результат
- Запуск: `python test/test_integration.py`

**Миграция на pioarduino (Path B — Light Sleep):**
- Переход на Arduino 3.x + ESP-IDF 5.x для auto light sleep с BLE
- Тесты (48 шт) проверят совместимость после миграции
- См. `docs/PLAN_LIGHT_SLEEP.md`

### 33. Тесты firmware — ГОТОВО ✓

**Дата**: 2026-04-07

**Native тесты (20 шт, pio test -e native):**
- CRC16-CCITT: пустой буфер, single zero, known vector "123456789"→0x29B1, детерминизм, различие, большой буфер
- Bitmap операции: clear, set/get, various positions, no duplicates, find_missing (none/some/all/max_limit), count_set
- Размеры структур пакетов: Audio(39), Beacon(36), FileHeader(35), FileAck(107), FileEnd(5)
- utils.h — вынесены чистые функции (CRC16, bitmap) для тестируемости

**Embedded тесты (28 шт, pio test -e test_embedded):**
- CPU: частота 160 МГц (тест прошёл, но BLE Advertising крашится — подтверждение блокера)
- GPIO: LED(35), кнопка(0), Vext(36)
- OLED: init, sleep/wake, show message
- Батарея: напряжение 3.0-4.5V, процент 0-255
- Heap: >100 КБ свободно, malloc 200 КБ + write pattern
- WiFi: безопасное отключение WIFI_OFF
- LoRa (10 тестов): init, mutex, set channel (valid/invalid), frequency mapping, TX power, start receive, send small packet, power mode continuous/sleep, send wake
- BLE: init, PIN <10000, not connected
- Beacon: init, callsign, send

**Файлы:** firmware/test/test_native/test_main.cpp (199 строк), firmware/test/test_embedded/test_hardware.cpp (289 строк)
**platformio.ini:** `[env:native]` test_build_src=false, `[env:test_embedded]` test_build_src=true + PIO_UNIT_TESTING

### 34. Миграция на pioarduino + NimBLE 2.x + Power Optimization — В РАБОТЕ

**Дата**: 2026-04-08

**Контекст**: полевой тест показал 12 часов на 1200 мА·ч ≈ 100 мА среднее (ожидалось 20-28 мА).
Подключили Serial — обнаружили `[Power] esp_pm_configure failed: 0x106` (NOT_SUPPORTED).

**Причина**: `custom_sdkconfig` был в `[common]`, но НЕ наследовался в `[env:heltec_wifi_lora_32_V3]`.
`CONFIG_PM_ENABLE` отсутствует в предсобранных Arduino 3.x libs → esp_pm_configure() — заглушка.

**Попытка исправить custom_sdkconfig**: добавлен `custom_sdkconfig = ${common.custom_sdkconfig}` в env V3.
Результат: pioarduino 55.03.37 не компилирует IDF 5.5.2 из исходников:
- `esp_adapter.c`: `os_get_time`, `os_get_random` undeclared (include path баг)
- `eloop.c`: `os_reltime`, `os_semphr_give` undeclared (та же проблема)
- `esp_insights`/`esp_rainmaker`: missing `.S` files from `target_add_binary_data`
- **Вердикт**: custom_sdkconfig с pioarduino 55.03.37 + IDF 5.5.2 не работает. Light sleep отложен.
- Патч `esp_adapter.c` (forward declarations) решает одну ошибку, но за ней десятки других.

**Что сделано (софтверные оптимизации, без light sleep):**

**platformio.ini:**
- `platform` → pioarduino (Arduino 3.x + ESP-IDF 5.x)
- `NimBLE-Arduino` 1.4.x → **2.5.0** (новый API)
- `custom_sdkconfig` подготовлен в `[common]`, но закомментирован в env V3 (см. выше)
- Добавлен `gen_cert_stubs.py` — workaround для pioarduino `target_add_binary_data` (пока не нужен)

**ble_service.cpp — NimBLE 2.x API миграция:**
- `ble_gap_conn_desc*` → `NimBLEConnInfo&`
- `std::string` → `NimBLEAttValue` для getValue()
- `ESP_PWR_LVL_P6` → `NimBLEDevice::setPower(6)`
- `setScanResponse(false)` → `enableScanResponse(false)`
- `setMinPreferred/setMaxPreferred` → `setPreferredParams(160, 320)`
- Добавлен `pServer->advertiseOnDisconnect(true)` (NimBLE 2.x не рестартует advertising автоматически)
- Убран ручной `NimBLEDevice::startAdvertising()` из onDisconnect

**main.cpp — Power Management + Duty Cycle + Wake Packet:**
- `setupPowerManagement()` вызывает `esp_pm_configure()` — пока возвращает 0x106 (NOT_SUPPORTED без custom_sdkconfig)
- Добавлен `esp_sleep_enable_bt_wakeup()`, GPIO wakeup: DIO1(14) + кнопка USER(0)

**main.cpp — LoRa duty cycle (АКТИВИРОВАНО):**
- После 10 сек idle при BLE connected → `LORA_POWER_DUTY_CYCLE_RX` (аппаратный SX1262 duty cycle)
- При получении любого LoRa пакета → мгновенный возврат в `LORA_POWER_CONTINUOUS_RX`
- При PTT/file transfer → continuous RX (duty cycle не включается)
- Без BLE → `LORA_POWER_SLEEP` (standby, только beacon TX)

**main.cpp — PTT wake-пакет:**
- При `BLE_CMD_PTT_START` отправляется аудио пакет с `PKT_FLAG_PTT_START` + **длинная преамбула (32)**
- Будит приёмники из duty cycle перед началом голосовой передачи
- Приёмник получает wake-пакет → переключается в continuous RX → ловит все последующие аудио пакеты с короткой преамбулой

**main.cpp — Таймауты оптимизированы:**
- loraTask idle: 50мс → **500мс** (CPU просыпается в 10× реже, DIO1 ISR по-прежнему будит мгновенно)
- BLE STATUS_UPDATE: 2 сек → **10 сек** (реже будит BLE radio)
- Arduino loop(): 200мс → **500мс** (кнопка/OLED)

**Тестирование 2026-04-08:**
- Serial лог подтверждает: BLE connected → CONTINUOUS_RX → idle 10 сек → DUTY_CYCLE_RX ✓
- Beacon от второго устройства: получен в DUTY_CYCLE (long preamble) → переход в CONTINUOUS_RX ✓
- Без BLE → STANDBY ✓
- Отправка файла (24.6 КБ, 206 чанков): ACCEPTED → SENDING → ACK received → **DELIVERED** за 42 сек ✓
- PTT и сообщения: нужен полевой тест на расстоянии

**Ожидаемое потребление (без light sleep, с duty cycle):**
| Состояние | Оценка | Время на 1200 мА·ч |
|-----------|--------|---------------------|
| BLE connected, idle (duty cycle) | ~40-60 мА | ~20-30 ч |
| BLE disconnected (standby) | ~6-10 мА | ~120-200 ч |
| С light sleep (будущее) | ~10-18 мА | ~67-120 ч |

**Блокер для light sleep:**
pioarduino 55.03.37 не поддерживает `custom_sdkconfig` корректно (IDF 5.5.2 WiFi/WPA supplicant не компилируется из исходников). Варианты:
1. Ждать обновлённую версию pioarduino с исправленным IDF
2. Собирать IDF libs отдельно через `idf.py` и подменять
3. Попробовать другую версию pioarduino (51.x / 53.x)

### 35. Баг: ACK не доходит при отправке с MeshTRX-6770 — НЕ РЕШЕНО

**Дата**: 2026-04-08

**Симптом**: при отправке файла с 6770→7E88, файл доходит полностью (0 потерь), но ACK от приёмника (7E88) не принимается отправителем (6770). Таймаут 30 сек → FAILED. В обратном направлении (7E88→6770) ACK приходит и файл помечается DELIVERED.

**Подтверждено Serial логами обоих устройств:**

*На приёмнике (7E88) — всё работает:*
```
[File] RX start: i.jpeg (18275 bytes, 153 chunks)
[LoRa RX] type=0xC1 ... (153 чанков, 0 потерь)
[File] RX all chunks: i.jpeg (153/153) — waiting for FILE_END
[File] RX complete via FILE_END: i.jpeg (18275 bytes)
[File] Sending ACK (long preamble)
[File] Sent to phone OK
```

*На отправителе (6770) — ACK не приходит:*
```
[FileSend] Starting: i.jpeg (18275 bytes, 153 chunks)
[LoRa] Power: CONTINUOUS_RX
[FileSend] FILE_END sent, waiting for ACK...
[LoRa] RX restarted by loraTask    ← 15 перезапусков RX за 30 сек
...
[FileSend] ACK timeout
[FileSend] FAILED (61306 ms, 0 NACK rounds)
```

**Что проверено (всё не помогло):**
1. ~~`loraStartReceive()` из fileSendTask (Core 1)~~ — заменён на флаг `loraNeedRxRestart`, loraTask (Core 0) перезапускает RX
2. ~~Периодический перезапуск RX каждые 2 сек в цикле ожидания ACK~~ — 15 перезапусков, ни один пакет не получен
3. ~~Duty cycle мешает приёму~~ — CONTINUOUS_RX подтверждён логом
4. ~~Радио не в RX~~ — `[LoRa] RX restarted by loraTask` подтверждает startReceive()

**Характеристики проблемы:**
- Направление 6770→7E88 (данные): работает идеально, 0 потерь, RSSI -47...-55
- Направление 7E88→6770 (данные): работает идеально, 0 потерь
- Направление 7E88→6770 (ACK после файла 7E88→6770): **работает** ✓
- Направление 7E88→6770 (ACK после файла 6770→7E88): **НЕ работает** ✗
- При этом beacons от 7E88 на 6770 приходят нормально (вне окна file transfer)
- RSSI отличный (-28...-59 dBm), TX power 1 dBm, расстояние ~1м

**Гипотезы для следующей сессии:**
1. **Устройства на РАЗНОЙ прошивке** — 7E88 прошит ранее (до loraNeedRxRestart), 6770 — позже. Нужно прошить оба одинаково и протестировать.
2. **Cross-core SPI race**: fileSendTask (Core 1) вызывает `loraSend()` через SPI, затем loraTask (Core 0) вызывает `loraStartReceive()` через тот же SPI. Mutex защищает, но возможны тонкие проблемы с DMA/кешем.
3. **RadioLib state**: после `loraSend()` из Core 1 RadioLib/SX1262 может оставаться в неконсистентном состоянии, и `radio.startReceive()` из Core 0 не работает корректно.
4. **DIO1 ISR не срабатывает**: ISR привязана к DIO1 через RadioLib. После `loraSend()` DIO1 переключается с onTxDone на onRxDone. Может быть edge case с пропуском прерывания.
5. **Аппаратная проблема 6770**: возможна разница в кристалле/PA/настройке приёмника именно 6770.

**Текущее состояние кода (незакоммичено):**
- `loraNeedRxRestart` — volatile bool флаг, fileSendTask ставит, loraTask обрабатывает
- fileSendTask НЕ вызывает `loraStartReceive()` напрямую (всё через флаг)
- Периодический перезапуск RX каждые 2 сек в ACK wait loop
- `loraSetPowerMode(CONTINUOUS_RX)` вызывается в начале fileSendTask

**План для следующей сессии:**
1. Прошить ОБА устройства одинаковой прошивкой
2. Тест в обоих направлениях: 7E88→6770 и 6770→7E88
3. Если 6770→7E88 всё ещё fail — добавить лог в ACK wait: проверить `radio.getModemStatus()` или аналог для подтверждения что SX1262 реально в RX
4. Попробовать вариант: fileSendTask ставит FILE_END в очередь, loraTask отправляет его (весь SPI доступ с Core 0)
5. Попробовать вариант: после loraSend() в fileSendTask вызвать `radio.standby()` + задержка + затем флаг на loraTask

---

### 36. NimBLE 2.x миграция + power management — ГОТОВО ✓

**Дата**: 2026-04-14

**Что сделано:**
- **NimBLE-Arduino 2.5.0** — полная миграция API в `ble_service.cpp`:
  - `ble_gap_conn_desc*` → `NimBLEConnInfo&`
  - `setPower(ESP_PWR_LVL_P6)` → `setPower(6)`
  - `advertiseOnDisconnect(true)` — авто-рестарт advertising
  - `enableScanResponse()` / `setPreferredParams()` — новый API
- **pioarduino** (platform-espressif32 stable) — замена стандартной espressif32
- **custom_sdkconfig** для V4/test_embedded:
  - `CONFIG_PM_ENABLE=y` + `CONFIG_FREERTOS_USE_TICKLESS_IDLE=y`
  - `CONFIG_BT_CTRL_MODEM_SLEEP=y` + modem sleep mode 1
  - `CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP=y`
  - `CONFIG_ESP_PHY_MAC_BB_PD=y` — power down MAC/BB during sleep
- **Auto light sleep** в `main.cpp`: `esp_pm_configure()` (240/80 МГц)
- **BT wakeup**: `esp_sleep_enable_bt_wakeup()`
- **Duty cycle RX**: idle 10 сек → DUTY_CYCLE_RX, активность → CONTINUOUS_RX
- V3 env: `custom_sdkconfig` закомментирован (pioarduino не компилирует IDF из исходников)

**Результат тестирования:**
- Прошивка залита и протестирована на устройствах
- Аккумулятор 1200 мАч: **10–12 часов** автономной работы
- BLE, LoRa, OLED — всё работает стабильно

---

### Анализ энергопотребления — статус проблем (2026-04-08)

**Проблема 1: LoRa duty cycle НЕ активирован — РЕШЕНО ✓**
- Был комментарий `// Duty cycle отключён`. Теперь: idle 10 сек → DUTY_CYCLE_RX, активность → CONTINUOUS_RX.
- PTT wake-пакет с длинной преамбулой будит приёмники перед голосом.

**Проблема 2: loraTask будит Core 0 каждые 50мс — РЕШЕНО ✓**
- Таймаут idle: 50мс → 500мс. DIO1 ISR по-прежнему будит мгновенно при пакете.

**Проблема 3: Arduino loop() каждые 200мс — РЕШЕНО ✓**
- Увеличен до 500мс.

**Проблема 4: BLE STATUS_UPDATE каждые 2 сек — РЕШЕНО ✓**
- Увеличен до 10 сек.

**Проблема 5: Serial включён (debug build) — НЕ РЕШЕНО**
- `NDEBUG` не определён. UART ~5 мА. Не критично пока идёт отладка.

**Проблема 6: esp_pm_configure() не работает — РЕШЕНО ✓ (V4), НЕ РЕШЕНО (V3)**
- V4: pioarduino + `custom_sdkconfig` с CONFIG_PM_ENABLE=y — light sleep работает.
- V3: `custom_sdkconfig` закомментирован (pioarduino 55.03.37 не компилирует IDF из исходников).

**Проблема 7: Serial.print без LOG_D — НЕ РЕШЕНО**
- lora_radio.cpp, beacon.cpp используют `Serial.print()` напрямую. Не критично пока NDEBUG не включён.

**Прочее:**
- Pre-emphasis/de-emphasis фильтр (дополнительное улучшение разборчивости)
- AGC (нормализация уровня микрофона)
- Скрипты автоматизации (install_deps, flash, build)
- Полный тест ретранслятора (WiFi монитор, смена канала через веб)
- Шифрование AES-128 для голоса и сообщений
- Групповые вызовы с аудио-микшированием

---

### 37. Восстановление связи + тестовый стенд + закрытие бага №35 — ГОТОВО ✓

**Дата**: 2026-08-18

**Исходная проблема**: после коммита `52d42e4` устройство циклически перезагружалось
(`boot → advertising 3 с → deep sleep 10 с → rst:0x5 DSLEEP`, период ~15 с), телефон терял
BLE и не мог переподключиться. Замерено вживую: 2 полных ребута за 25 с лога.

**Firmware — стабильность связи:**
- Удалён цикл deep sleep целиком; возвращены `esp_pm_configure(240/80)` + BT/GPIO wakeup
  (на V3 `esp_pm_configure` → 0x106, light sleep по-прежнему недоступен — см. этап 4 плана)
- `EVT BOOT reason=… count=…` — причина reset и счётчик перезагрузок в NVS
- Advertising 100-200 мс + scan response; `setPreferredParams(1600,3200)` просил соединение
  1000-2000 мс (в коде это ошибочно называлось интервалом рекламы) → 30-50 мс;
  `updateConnParams` slave latency 2 → 0
- OLED убран из BLE-колбэка (там были `delay(100)` и реинициализация I2C в контексте NimBLE)
- `loop()` 500 мс независимо от BLE; `bleTask` не спит 30 с без телефона

**Firmware — потокобезопасность радио:**
- Весь доступ к SX1262 под `loraRadioMutex`: разделение locked/unlocked + RAII `MutexGuard`
- `loraSendWake` атомарен (преамбула → TX → возврат в приём) и возвращает приём безусловно
- `loraAppBusy` — beacon больше не влезает в активную передачу
- Утечка: принятый файл без телефона навсегда оставался в heap — освобождается

**Баг №35 (ACK не доходил) — закрыт.** Причина не в «разнице устройств»: beacon уходил
посреди приёма файла, после чего радио не возвращалось в приём (`loraSendWake` делал это
только при подключённом телефоне). Из 43 чанков доходило 26, `FILE_END` терялся, приёмник
зависал в `FILE_STATE_RECEIVING` и отвечал `busy`. После фикса файлы 1/5/20 КБ проходят
в обе стороны с симметричным временем и 0 NACK-раундов.

**Тестовый стенд (новое):**
- `firmware/src/test_console.*` — консоль в dev-сборке (`-DTEST_CONSOLE`), env `heltec_v3_dev` /
  `heltec_v4_dev`; команды PING/INFO/TESTMODE/CH/PWR/TX TEXT|AUDIO|FILE|BEACON|CALL/RX STATS/LORA/BLE
- Ответы машинного формата `EVT <NAME> k=v` — парсятся стендом
- `firmware/test/harness/` — `device.py` (драйвер UART + парсер событий), `run_tests.py`
  (31 проверка), `README.md`; `pyserial` в `vendor/` (системный pip закрыт PEP 668)
- Тесты гоняются на минимальной мощности −9 дБм: платы лежат рядом, при 14 дБм канал
  ничего не проверяет (кламп `power < 1` снят, введён `MIN_RADIO_DBM`)

**Результат: 31/31 проверка за 172 с.** Голос 100/100 пакетов без потерь; текст, ACK, beacon,
вызовы ALL/PRIV/SOS, изоляция каналов — работают; 0 ребутов, память стабильна.

**Выявленные ограничения:**
- Реальный потолок файла ≈65 КБ (heap ~97 КБ, требуется size+30 КБ), а README обещает 100 КБ
- Скорость файлов 585 Б/с — половина времени уходит на паузы 50 мс между чанками
- Энергосбережение сейчас отсутствует (связь важнее); light sleep на V3 требует CONFIG_PM_ENABLE
- Не проверены: BLE-путь эмулятором телефона, Android-реконнект (лимит 10 попыток не снят)

**План работ**: `docs/TASK_STABILITY_AND_TESTBENCH.md`

---

## Git коммиты

- `2b16391` v1.0 — firmware + Android app, full working state
- `4131919` v1.1 — PIN auth, auto-connect, spec v2
- `6d4e1bc` v2.0 — Multi-tab UI, Foreground Service, listen modes
- `fca05f9` v2.1 — Call picker, recent calls, peer persistence
- `04b16fa` v2.2 — Dark tactical theme, PttButtonView with waves
- `221d135` v2.3 — Callsign from settings, header layout
- `ae0ab2f` v2.4 — Clean voice layout, remove duplicate row
- `ee0a7f0` v2.4+ — Spec wireframe update
- `4bb7b59` v2.5 — Incoming call system (overlay, ringer, vibration)
- `2cd17e6` v2.6 — Async Roger Beep
- `2f87c6d` v2.7 — Stack overflow fix (static pcmBuf + 16K lora stack)
- `edcb829` v2.7+ — PROJECT_LOG update
- `ad4dd9a` v2.8 — Shared header, disabled controls without BLE
- `ef4acfb` v2.8+ — PROJECT_LOG update
- `0858d65` v2.9 — File transfer: photo/file send, ImageProcessor
- `91e6730` v3.0 — File history, share, retry, delete, persistence
- `851eae9` v3.1 — File transfer fixes, list display, retry/share
- `4ec47c3` v3.2 — File storage on disk, addressed transfers, messenger UI overhaul
- `43c7427` v3.2+ — PROJECT_LOG update
- `c5937fd` v3.3 — Map/Radar, GPS, i18n (ru/en), 5-tab navigation
- `f7c65f7` v3.4 — File transfer fix: CHUNK_SIZE 120, BLE protocol, auto-complete
- `ea7f002` v3.5 — File transfer stability: bleTask, bitmap, filename, 100ms
- `34d08d9` v3.6 — Speaker, local roger beep, RX volume, squelch, no SOS
- `da96e08` v3.8 — UI, repeater, battery, button
- `8fbca94` v4.0 — Public website meshtrx.com + meshtrx.ru, i18n RU/EN
- `e34d2f2` v4.1 — Power optimization, BLE auto-reconnect, GPS beacon fix
- `xxxxxxx` v4.2 — Radar improvements, GPS in calls, splash screen, app icon
- `166b3ef` v4.3 — Voice reliability, addressed PTT, voice messages, UI overhaul
- `bf7538b` v4.3.0 — Version bump, deploy to meshtrx.com
- `b4cb82d` v4.3.1 — Addressed calls fix, callSign sync, UI improvements
- `4745dfd` Docs: power optimization research + Heltec V4 specs
- `faffaea` Firmware: power optimization — event-driven LoRa, BLE tuning, OLED Vext off
- `6542052` Power optimization: LoRa sleep without BLE, PA management V4, preamble 32
- `7597f57` Fix: signed release APK for website download
- `7414a9e` File transfer: FILE_END support, preamble fix, chunk delay tuning
- `7273685` Task: file transfer v2 protocol design
- `3fd25ce` File Transfer v2: ESP32 RAM buffering, autonomous LoRa TX with NACK retry
- `70ee714` File Transfer v2: ACK delivery fixed, radio mutex, full working pipeline
- `10c756a` Docs: PROJECT_LOG and SPEC updated for File Transfer v2, power optimization
- `da04edd` Tests: embedded hardware tests + clean platformio.ini
- `2dcba19` Tests: native unit tests + utils.h refactoring
- `8ff7f18` Docs: Light Sleep implementation plan — Path A (manual) + Path B (pioarduino)
- `bb18e06` Revert pioarduino: CONFIG_PM_ENABLE crashes BLE controller
- `2a1b727` Revert CPU 160MHz: BLE controller crashes on non-240MHz
- `8b4f5f3` Task: NimBLE migration to IDF 5.x — unlock light sleep and CPU scaling
- (незакоммичено) NimBLE 2.x API migration, pioarduino + custom_sdkconfig, auto light sleep, duty cycle RX

---

### 38. Устройство без телефона снова слышит эфир + энергетика с цифрами — ГОТОВО ✓

**Дата**: 2026-08-19

**Главная находка.** В `loraTask` устройство без подключённого телефона уводило радио
в полный standby (`LORA_POWER_SLEEP`). Замер на стенде: **0 из 4** адресованных сообщений
и **0 из 5** маяков. То есть узел, у которого телефон разрядился, ушёл из зоны или просто
закрыл приложение, полностью выпадал из сети до следующего BLE-подключения — ровно то,
что выглядело как «связь пропадает и не восстанавливается». Заменено на duty cycle RX
(слушает периодически, просыпается на длинную преамбулу): **4/4** после правки.
Цена — около 1 мА против standby при 45 мА, которые потребляет контроллер.

**Сопутствующие дефекты, найденные по дороге:**
- Смена режима радио стояла *перед* `readData()`, а внутри неё `startReceive()` сбрасывает
  буфер чипа — пакет, ради которого устройство проснулось, стирался. Теперь читаем, потом
  переключаем.
- Текст и первый кадр голоса шли короткой преамбулой, поэтому спящий приёмник их не слышал
  (маяки и вызовы будили правильно). Теперь текст всегда, а голос после паузы — `loraSendWake`.
- Перерисовка OLED на каждый принятый аудиопакет не давала задаче приёма вернуться в эфир:
  было 27/30 при интервале 80 мс, стало **30/30**, потерь на BLE ноль.
- `bleNotifyFail` считал и попытки при отсутствии телефона — 30 % «неудач» на пустом месте.
  Разделено на `notify_noconn` / `notify_retry` / `notify_fail`, добавлен один повтор через
  20 мс. На прогоне BLE: 195 уведомлений, 0 неудач, 20/20 циклов переподключения.
- `EVT INFO` печатал адрес побайтово, а консоль принимает его как 16-битное значение —
  байты переставлялись, и тест, подставивший свой же `id`, слал пакет в никуда. Формат
  приведён к единому `%04X`; тесты больше не хранят адреса константами.

**Android:** снят лимит в 10 попыток переподключения — сервис замолкал навсегда и рация
оставалась мёртвой, даже когда устройство снова оказывалось рядом. Теперь попытки
бесконечны с нарастающей паузой до минуты, и они идут даже если соединение в этой сессии
ещё ни разу не состоялось (при сохранённом авторизованном устройстве).

**Энергетика — впервые с цифрами.** Прошивка считает время в режимах радио
(`RADIO TIME`), `power.py` переводит это в средний ток и ресурс батареи:

| Сценарий | Радио | Всего | Ресурс от 2000 мАч |
|----------|-------|-------|--------------------|
| покой, телефона нет | 0,8 мА | 45,8 мА | 43,7 ч |
| постоянный приём | 5,3 мА | 50,3 мА | 39,8 ч |
| передача голоса 12 пак/с | 12,4 мА | 74,4 мА | 26,9 ч |
| приём голоса | 5,3 мА | 67,3 мА | 29,7 ч |

Вывод: режимы LoRa на батарею почти не влияют, всё решает контроллер. Наблюдавшиеся
10-12 часов от 2000 мАч — это около 170 мА, вчетверо больше расчётных: искать нужно
постоянно включённый экран, питание платы и отсутствие light sleep, а не настройки радио.

**Новое в стенде:** `RADIO TIME`, `DUTY ON|OFF`, `EVT FILE_RX_START` / `FILE_END_RX` /
`TEXT_ACK_RX`, скрипты `power.py` (энергия) и `soak.py` (многочасовой прогон с CSV),
блок `idle` в `run_tests.py` — регрессия против главного дефекта этого дня.

**Результат: 54/55 проверок за 340 с** (единственный отказ — коллизия двух маяков в эфире,
при отдельном прогоне блок проходит), блок `idle` — 3/3 сообщений после простоя.
BLE: 14/14 проверок, 20/20 циклов подключения, среднее 2,6 с, 0 потерянных уведомлений.
Фаззинг 300 пакетов — 0 падений. Прогон 30 минут смешанного трафика — 0 перезагрузок,
куча не сдвинулась ни на байт.

**Дополнительно за этот день:** одиночный чужой `FILE_START` занимал приёмник на минуту —
пустая сессия теперь закрывается за 15 с; проверены ответы на вызов (`accept`/`reject`/
`cancel`); в Android найдено второе место, где связь не восстанавливалась — неудачный
поиск сервисов GATT завершался молчаливым `return`, оставляя открытое соединение без
характеристик и без события разрыва.

**Подтверждено про энергию:** `esp_pm_configure` возвращает `0x106` — light sleep
недоступен в текущих библиотеках, контроллер постоянно на 240 МГц. Это главный расход
(45 мА против 2,5 мА), и это следующая задача; в истории проекта два отката подряд
из-за падений BLE, поэтому включать вслепую нельзя. Для честного замера добавлен
`battery.py` — устройство на аккумуляторе без USB, ноутбук пишет напряжение по BLE.

### 39. Смешанная пара V3 + V4 на стенде — ГОТОВО ✓

**Дата**: 2026-08-19

Стенд переведён на пару разных плат: V3 через мост (`ttyUSB`), V4 нативным USB
(`ttyACM`). Порты определяются сами.

Три дефекта, которые видны только на такой паре:
- Команда `CH` меняла частоту радио, но не `currentChannel` — устройства слышали друг
  друга и отбрасывали пакеты как «чужой канал». На двух одинаковых платах не всплывало.
- Телеметрия V4 терялась в USB CDC: радио принимало 20 пакетов из 20, а до стенда
  доходило 13-18 событий, и часть строк приходила обрезанной. Событие теперь пишется
  одной операцией, буфер CDC 8 КБ, таймаут записи 20 мс — стало 20/20.
- Уровень сигнала перестал быть константой: рядом лежащие платы перегружают входной
  тракт V4, разнесённые дают −85 дБм. Перед прогоном стенд калибрует мощность замером.

Усилитель V4 проверен новой командой `FEM`: комбинация 1/1/0 даёт −58 дБм против
−68…−69 у остальных, то есть LNA включён верно, дефекта тракта нет.

**Результат: 55/56 проверок за 385 с**; файловый блок отдельно — 21/21, включая 20 КБ
в обе стороны с симметричным временем (35,1 и 35,2 с).

### 40. v4.4.0 — деплой на meshtrx.com для полевого тестирования — ГОТОВО ✓

**Дата**: 2026-08-19

Прошивки V3/V4 и APK пересобраны и выложены на сайт, версия поднята до 4.4.0
(`versionCode` 45). Релизная сборка (без тестовой консоли) проверена на железе перед
выкладкой: V3 без подключённого телефона принимает адресные сообщения и отвечает ACK —
3 из 3, BLE-подключение 3,0 с в среднем за пять циклов. APK подписан прежним ключом,
ставится поверх установленного.

**Сайт**: `web/out/` собран статикой и выгружен по rsync на VPS (195.133.1.248,
`/var/www/meshtrx`), перед выгрузкой снят архив прежней версии в `/root/`. Проверено:
оба домена отвечают 200, страница загрузки показывает 4.4.0, хеши выложенных прошивок
совпадают с локально собранными.

**Changelog** переписан на язык последствий, а не внутренностей: пользователю важно
«устройство без телефона снова слышит эфир», а не «loraSetPowerMode вместо standby».

**Новое на странице загрузки** — блок с просьбой проверить ровно те три вещи, на которых
связь и ломалась: держится ли BLE и восстанавливается ли сам, доходят ли сообщения и
вызовы до устройства без телефона, и как ведут себя голос и дальность в реальных
условиях. Ссылки на Telegram и GitHub Issues.

### 41. Настольный клиент для Windows / Linux / macOS — ГОТОВО ✓

**Дата**: 2026-08-19

Python + PySide6 + bleak + sounddevice, Codec2 подключается через ctypes к системной
библиотеке. Выбор платформы объяснялся тем, что переписывать протокол ради красивого
нативного приложения нет смысла: он один и тот же на трёх языках, а bleak даёт BLE
одинаково на всех трёх системах.

Перенесено всё, что есть в телефоне: голос по удержанию кнопки или пробела, сообщения
широковещательные и адресные, файлы, вызовы, радар, карта по маякам, настройки и
ретранслятор. Сверх телефона — журнал диагностики: видно, что именно приходит по BLE.

**Проверка на живой паре** (клиент на одном устройстве, тестовое — на втором): 28
проверок, два дефекта найдены и закрыты. Один из них — рваный приём голоса: клиент
проигрывал кадры по мере поступления, а эфир приходит рывками; вылечено буфером.

### 42. v4.4.1–4.4.2 — рация не пропадает из списка и находит соседей за секунду — ГОТОВО ✓

**Дата**: 2026-08-20 … 2026-08-21

Устройство переставало объявлять о себе после разрыва соединения и исчезало из списка
Bluetooth до перезагрузки. Добавлено `bleEnsureAdvertising()` — периодическая проверка,
что устройство слышно в эфире, и освобождение брошенного соединения через 45 секунд
тишины: телефон, ушедший в карман с выключенным Bluetooth, больше не держит место.

Соседние узлы раньше появлялись в списке через минуты — по расписанию маяков. Введён
флаг `BEACON_FLAG_REQUEST` (0x10, потому что 0x08 занят признаком ретранслятора):
новичок просит соседей отозваться, и список собирается за секунду.

### 43. Прошивка для Heltec V4 rev 4.3 — ГОТОВО ✓

**Дата**: 2026-08-26

У ревизии 4.3 другой усилитель: KCT8103L вместо GC1109, и управляется он другим
выводом (CTX = GPIO5, низкий уровень включает малошумящий усилитель, вместо CPS =
GPIO46). Прошивки разведены по окружениям `heltec_wifi_lora_32_V4` и
`heltec_wifi_lora_32_V43`; версия платы попала в имя файла на сайте, чтобы человек
не гадал, что скачал.

Заодно пересчитан делитель измерения батареи для V4: показывало 4,35 В вместо 4,20 В,
коэффициент 5,36 против 5,55 у V3.

### 44. v4.4.3 — найдена настоящая причина «телефон не видит ноду» — ГОТОВО ✓

**Дата**: 2026-08-26

Жалоба тестировщиков звучала как «не видит устройства», и несколько версий подряд
чинилось не то. Настоящая причина: в основном пакете объявления всего 31 байт, три
уходят на флаги, восемнадцать — на 128-битный идентификатор сервиса, и на имя из
двенадцати символов места не остаётся. Устройство было в эфире всё это время, но
безымянным, а приложение искало по имени.

Имя переехало в основной пакет, идентификатор сервиса — в scan response. Плюс
устранена вторая причина той же жалобы: приложение с сайта бралось из кеша браузера,
поэтому в имена файлов добавлена версия, а на nginx выставлен запрет кеширования.

### 45. Речь ограничена десятью секундами во всех режимах — ГОТОВО ✓

**Дата**: 2026-08-28

Канал полудуплексный: пока кто-то говорит, остальных не слышно вовсе. Предел в 10
секунд действует в удержании кнопки, в голосовой активации, в голосовом сообщении и
в адресном голосе, и продублирован в прошивке (`PTT_MAX_SECONDS`) — зависший телефон
больше не занимает эфир, устройство само прекращает передачу и пишет на экране
`LIMIT 10s`.

На кнопке передачи идёт обратный отсчёт, на нуле передача прекращается даже при
удерживаемой кнопке; чтобы продолжить, нужно отпустить и нажать заново. Тот же отсчёт
показывается в режиме голосовой активации. Реализовано одинаково в телефоне и в
настольном клиенте.

**Режим SOS убран** из приложения и прошивки: отдельная тревожная кнопка рядом с
кнопкой передачи нажималась случайно, а в сети, где все слышат друг друга, отдельный
канал тревоги не нужен. Код типа вызова оставлен в таблице протокола ради устройств
со старой прошивкой.

### 46. Ретранслятор проверен на стенде — ГОТОВО ✓

**Дата**: 2026-08-28

Текст проходит через ретранслятор 5 из 5, маяки 2 из 2, голос — примерно наполовину
(около 10 пакетов из 20): ретранслятор повторяет пакет сразу и попадает в хвост чужой
передачи. Это известное ограничение, лечится сдвигом повтора — записано в
[ROADMAP](docs/ROADMAP.md).

Блок проверок ретранслятора перенесён в конец прогона: перезагрузка платы сбрасывала
тестовый режим и роняла всё, что шло после (было 48 из 62). Сейчас — 62 из 62.

### 47. Мост в Telegram и MCP-сервер — ГОТОВО ✓

**Дата**: 2026-08-28

Обратная связь от тестировщиков собирается в группе Telegram, и читать её вручную
дорого. Заведён бот `@meshtrx_bot`: демон забирает сообщения группы в SQLite, MCP-сервер
даёт агенту читать переписку и отвечать в неё, `/ask` ищет ответ по документации
проекта.

С Telegram разговаривает **только демон**: длинный опрос отдаёт обновление ровно одному
клиенту, и два процесса начали бы отбирать сообщения друг у друга. Всё остальное
общается через базу — побочная польза в том, что отправка переживает перезапуск.

Токен лежит вне репозитория, в `~/.config/meshtrx/telegram.env` с правами 600.
Тон сообщений вынесен в `tools/tgbot/persona.md`: пишем как участник команды, без
подписей и канцелярита, но на прямой вопрос, кто отвечает, отвечаем честно.

### 48. v4.4.4 — предел речи и обратный отсчёт в продакшене — ГОТОВО ✓

**Дата**: 2026-08-28

Выпущено то, что накопилось после 4.4.3: предел речи в десять секунд (в приложении
и продублированный в прошивке), обратный отсчёт на кнопке передачи и в режиме
голосовой активации, снятый режим SOS, версия на экране настроек из сборки вместо
зашитой строки 4.3.1.

**Собрано**: три прошивки (V3, V4 rev 4.2, V4 rev 4.3) и APK `versionCode` 49.
Подпись сверена с выложенными 4.4.2 и 4.4.3 — отпечаток тот же,
`b7c5983e…`, обновление ставится поверх установленного.

**Стенд прогнать не удалось**: обе платы отключились от USB, портов в системе не
осталось. Проверка при этом не потеряна — код прошивки не менялся ни строкой с
коммита `9b79ec7`, на котором стенд дал 62 из 62; всё, что легло между ним и
релизом, касается моста в Telegram и документации. Бинарники собраны из
проверенного кода, но живого прогона именно этой сборки нет — стоит повторить,
когда платы вернутся на стол.

**Сайт**: `web/out/` выгружен на VPS, перед выгрузкой снят архив прежней версии
(`/root/meshtrx-backup-4.4.3-20260828-2046.tar.gz`, 31 МБ). Проверено: оба домена
отвечают 200, страница загрузки показывает 4.4.4, хеши четырёх выложенных файлов
совпадают с локально собранными. Прошлые версии на сервере оставлены — ссылки на
них уже разошлись по переписке.

**Changelog** описан последствиями: «одна передача длится не больше десяти секунд,
потому что пока говорит один, остальных не слышно вовсе». Заодно из записи 4.4.1
убрана калька «реклама Bluetooth» — по-русски устройство объявляет о себе в эфире.

### 49. v4.4.5 — настройки звука перестали пропадать — ГОТОВО ✓

**31.08.2026.** Релиз целиком вырос из одного вопроса в группе: «как настроить
модуляцию голоса и звук окончания передачи?».

**Модуляции нет и не будет**: голос идёт кодеком Codec2 на постоянных 3200 бит/с,
тембр там регулировать нечем. Так и ответили, добавив абзац в руководство — вопрос
повторится.

**Звук окончания передачи** был зашит намертво (`AudioEngine.playRogerBeep`). Кому-то
он нужен, чтобы отличить «договорил» от «связь оборвалась», кому-то мешает в
помещении — сделан переключатель в разделе «Аудио».

**Попутно вскрылись две вещи в том же месте.** Ни одна настройка звука не
переживала перезапуск: громкость приёма, PTT RMS, порог и задержка VOX жили только
в памяти, и подобранные под свой голос значения молча возвращались к заводским —
со стороны это выглядело как «VOX опять срабатывает не вовремя». А ползунки VOX
вообще никогда не заполнялись текущими значениями и всегда показывали 800 из
разметки, так что настройку правили вслепую. И третье: «Новое устройство (забыть
текущее)» делало `prefs.edit().clear()` — вместе с адресом ноды пропадали переписка,
история файлов, список абонентов и позывной, хотя человек в меню выбирал забыть
рацию, а не сбросить приложение.

**Прошивка не менялась ни строкой** — на сайте у неё осталась версия 4.4.4 и своя
дата (`VERSION.firmwareDate`), чтобы люди не перепрошивали рации впустую. В
changelog это сказано прямым текстом.

**Собрано и выложено**: APK `versionCode` 50, подпись `b7c5983e…` — та же, что у
4.4.2–4.4.4, обновление встаёт поверх. Хеш выложенного файла сверен со скачанным с
сайта, `meshtrx-latest.apk` указывает на него же, оба домена отвечают 200, старые
версии на месте. Перед выгрузкой снят архив
(`/root/meshtrx-backup-4.4.4-20260831-1800.tar.gz`, 42 МБ).

**Стенд прогнан в тот же день, после релиза**: 61 проверка из 62. Единственный
провал — «B отвечает на PING» в первом же блоке: стенд запустили сразу после
прошивки, и PING ушёл, пока плата ещё грузилась; повторный прогон на прогретых
платах дал отклик. Голос через ретранслятор снова показал ровно половину:
переслано 10 пакетов из 20 — это известное ограничение полудуплекса, не регрессия.

**Почему прогон не шёл 28.08 — диагноз тогда был неверный.** Дело не в
отвалившемся USB (порты действительно исчезали, но вернулись). Плата V4 rev 4.3
оставалась в **режиме ретранслятора** с прежних опытов: флаг лежит в NVS и
переживает перезагрузки, а в этом режиме прошивка не создаёт задачу LoRa
приложения — `INFO` показывает `lora=0`, `RX STATS` пуст, по радио тишина при живой
консоли. Со стороны стенда это выглядит как «плата есть, связи нет». Лечится
командой `REPEATER OFF` (перезагружает в обычный режим). За всё время в этой роли
плата переслала ноль пакетов — рядом никто не передавал.

### 50. v4.4.6 — устройство выключается кнопкой — ГОТОВО ✓

**31.08.2026.** Вопрос из группы: «как выключить ноду? она включена постоянно».
Ответ был неутешительный — выключателя на плате нет, выключения в прошивке не
было, оставалось отсоединять USB или аккумулятор.

**Как сделано.** Три секунды удержания кнопки в обычном режиме (восемь в режиме
ретранслятора — там удержания до трёх секунд заняты сбросом статистики и выходом
из режима), крупная надпись `SLEEP` на экране, дальше глубокий сон: BLE
остановлен, радио уведено настоящим `radio.sleep()`, усилитель обесточен, экран
погашен вместе с Vext. Пока кнопка нажата, идёт обратный отсчёт — без него человек
держит вслепую и отпускает раньше. Пробуждение той же кнопкой; выход из deep sleep
на ESP32 — полный старт, то есть ровно то, чего ждут от «включить».

**Механизм пробуждения выбран по факту.** `esp_deep_sleep_enable_gpio_wakeup` для
ESP32-S3 недоступна (`SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP` не объявлен) — компилятор
это и сказал. Работает `ext0`, который на S3 поддерживается и принимает любой
RTC-вывод, каким GPIO0 является. Код возврата проверяется: уснуть без будильника —
это кирпич до отключения питания, поэтому при ошибке устройство не засыпает, а
показывает её на экране и перезапускается.

**Диагностика.** В строку `EVT BOOT` добавлена причина пробуждения. Повод был
личный: усыпив плату, я открыл порт заново, увидел загрузку и принял её за
пробуждение — а это был сброс по линиям DTR/RTS, `reason=POWERON`. Отличить одно
от другого без этой строки нельзя. Правильная проверка делается в одном непрерывном
сеансе и даёт `reason=DEEPSLEEP wake=BUTTON`.

**Проверено.** Вход в сон и пробуждение — на живой плате V3. Стенд: **62 из 62**
за 405 с, впервые полностью зелёный с 28.08. Голос через ретранслятор по-прежнему
10 пакетов из 20 — ограничение полудуплекса, не регрессия.

**Выложено.** Три прошивки, sha256 сверены со скачанными с сайта. Приложение
осталось 4.4.5 и на сайте показано отдельной версией со своей датой:
переустанавливать его не нужно, и в changelog это сказано прямым текстом.

### 51. Плата V4 rev 4.3 передавала мимо усилителя — ИСПРАВЛЕНО ✓

**02.09.2026.** Из группы пришла жалоба: две Heltec V4 rev 4.3 «не видят друг
друга, на десяти метрах связи нет». Разбор нашёл причину в нашей прошивке.

**Что было.** У KCT8103L (ревизия 4.3) управляющая линия CTX на GPIO5 — это
переключатель приём/передача, `HIGH` = передача. В коде она называлась
`PA_FEM_RX_MODE`, комментарий описывал её как «режим приёма», и поднималась она
никогда: блок переключения на время передачи стоял под `#ifdef PA_FEM_TX_MODE`,
который определён только для ревизии 4.2 с её GPIO46. То есть на 4.3 передача
уходила в антенну по приёмному тракту, мимо усилителя. Вдобавок прошивка
занижала мощность самого чипа, рассчитывая на усиление: `TX power → 14 dBm
effective (radio 8, PA +6)`.

**Замер.** Пара плат, одно расстояние, одна мощность, между прошивками платы не
трогали: было **−78 дБм**, стало **−20 дБм**. Пятьдесят восемь децибел — то есть
в эфир практически ничего не уходило. Повторный замер дал те же −20.

**Почему не поймали раньше.** На стенде пара V3 ↔ V4.3 лежит в полуметре, и
связь есть при любой мощности. Асимметрию в логах (V3 приходила на −56, V4.3 на
−72) видели и принимали за норму. Спецификация `docs/HELTEC_V4_SPECS.md` при этом
всё описывала верно: `LORA_PA_CTX`, `HIGH = TX`. Ошибка родилась при переносе
спецификации в код — и жила в комментарии, который выглядел объяснением.

**Что сделано.** Линия управления одна на обе ревизии (`PA_FEM_CTX`, GPIO46 у 4.2
и GPIO5 у 4.3): `HIGH` на время передачи, `LOW` в остальное время — что совпадает
и с приёмом через малошумящий усилитель, и с безопасным состоянием
strapping-пина GPIO46.

**Побочный эффект для стенда.** Сигнал стал настолько сильным, что калибровка не
попадает в рабочее окно даже на минимальных −9 дБм (−23 дБм при полуметре).
Стенд научен отличать это от отказа связи: слишком сильный сигнал засчитывается с
пометкой «платы стоят близко».

**Проверено.** Полный прогон 61 из 62; единственная непройденная проверка —
счётчик маяков ретранслятора, при отдельном прогоне блока проходит (2 из 2).

### 52. v4.4.7 — исправление передачи V4 rev 4.3 в продакшене — ГОТОВО ✓

**02.09.2026.** Выпуск ради одного исправления из записи 51: на ревизии 4.3
передача шла мимо усилителя, замер до и после — −78 дБм против −20 дБм.

Прошивка 4.4.7, приложение остаётся 4.4.5. Всем, у кого V4 rev 4.3, обновляться
обязательно: до этой версии рация отдавала в эфир доли процента мощности, и любые
их наблюдения о дальности до обновления смысла не имеют. V3 и V4 rev 4.2 правка
не касается — у них управление усилителем работало верно.

Собраны и выложены три образа, sha256 сверены со скачанными с сайта.

### 53. Приложение 4.4.6 — галочка доставки и телефон в стенде — ГОТОВО ✓

**06.09.2026.** Жалоба из группы после уличных тестов: «Не пишет в личном чате
доставку. Вернее, не определяет».

**Причина.** Подтверждение приходит с номером, под которым сообщение ушло в
эфир; само сообщение этот номер не хранило. Искали так: остаток от времени
отправки по модулю 256 сравнивали с номером — величины не связаны ничем, и
совпадение выпадало примерно раз на 256. Второе последствие оказалось хуже
первого: не дождавшись «своего» подтверждения, приложение повторяло сообщение
три раза, занимая общий полудуплексный канал вчетверо дольше нужного. Мешало это
всем в сети, а не только отправителю.

**Стенд научился видеть экран телефона.** Радио работало безупречно, стенд его и
проверял — а ломалось всё уже в приложении, в слепой зоне. Блок `[14]`:
телефон по adb подключается к плате B, шлёт адресное сообщение плате A и
проверяет галочку, приход подтверждения и отсутствие повторов. Три грабли,
найденные живьём: элементы искать по идентификаторам, а не координатам (диалог
PIN уезжает вверх с клавиатурой); на подключение есть 45 секунд, потом рация рвёт
связь с молчащим клиентом; PIN спрашивать у платы новой командой `PIN` и
сопоставлять по предпоследнему октету MAC — последний у BLE-адреса на единицу
больше, чем в имени.

**Замеры до и после:** было `00:15 → BSV ✗`, три повтора в журнале, адресат принял
три пакета; стало `00:20 → BSV ✓`, повторов нет, принято один пакет.

**Прогон:** 67 из 68 (проверок стало 68). Непройденная — таймаут события о
завершении файла при целом паттерне; при отдельном прогоне блока 21 из 21.

Приложение 4.4.6, прошивка остаётся 4.4.7. Подпись прежняя, `b7c5983e…`.

### 54. Приложение 4.4.7 — хвосты в тексте и галочка на голосовых — ГОТОВО ✓

**06.09.2026.** Оба дефекта нашли пользователи, и оба — в приложении, а не в
радио.

**Лишние символы в конце принятых сообщений.** Скриншоты из группы: на одной
рации хвост `mt`, на другой `с` и кракозябра; в тех же сообщениях отправитель
`TX-??` и уровень сигнала `0dBm`. Три симптома, причина одна. Прошивка шлёт в
телефон `[команда, RSSI, текст, 0, два байта адреса отправителя]`, а приложение
искало конец текста как первый нулевой байт — **с начала пакета**. При нулевом
RSSI найденным нулём оказывался он сам: граница уезжала в конец, к тексту
приклеивался адрес, а сам адрес читался за пределами пакета. Хвосты — буквально
адреса: `mt` = `6D 74` от MeshTRX-6D74, `с`+кракозябра = `63 D0` от MeshTRX-63D0.
Теперь конец ищем с третьего байта.

**Галочка на голосовых.** Текст ждёт подтверждения от рации и получает отметку, а
голосовое уходит файлом — и исход этой передачи до чата не доходил: сообщение
навсегда оставалось «отправляется». Теперь итог передачи переносится на
сообщение. Подтверждено на стенде: `11:45 → TX-C4C8 ✓`.

**Попутно снята ложная тревога.** Отправка файлов с телефона казалась сломанной —
приёмник вытягивал по одному чанку через NACK-раунды. После перезагрузки
платы-отправителя 13 передач подряд прошли за 2,66 с каждая, утечки памяти нет
(−100 байт за восемь передач). Виновато было состояние платы, сутки прожившей под
стендом, а не код: гипотезы про Bluetooth, мощность и перегрузку опровергнуты
замерами.

Приложение 4.4.7, `versionCode` 52, подпись прежняя. Прошивка остаётся 4.4.7.

## Спецификация

- `MESHTRX_SPEC.md` v2.0 — полная техническая спецификация
- `MESHTRX_DESIGN_SPEC.md` — дизайн UI/UX (тёмная тактическая тема)
- `MESHTRX_DESIGN_SPEC.md` — дизайн UI/UX (тёмная тактическая тема)
