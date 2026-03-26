# MeshTRX iOS — План реализации

> Порт Android-приложения MeshTRX на iOS (Swift/SwiftUI).
> Полный паритет функций с Android версией 4.2.0.
> Bundle ID: `com.meshtrx.app`

---

## Технологический стек

| Компонент | Технология |
|-----------|------------|
| UI | SwiftUI (iOS 16+) |
| BLE | CoreBluetooth |
| Аудио | AVFoundation + AudioToolbox (Audio Unit) |
| Codec2 | C-библиотека через Swift bridging header |
| Карта | MapKit |
| Геолокация | CoreLocation |
| Компас | CoreMotion / CoreLocation heading |
| Хранение | SwiftData / UserDefaults |
| Архитектура | MVVM + Combine / async-await |
| Минимальная iOS | 16.0 |
| Язык | Swift 5.9+ |

---

## Этап 0 — Инициализация проекта (1-2 дня)

- [x] Создать Xcode проект (SwiftUI App)
- [x] Настроить структуру папок:
  ```
  ios/MeshTRX/
  ├── MeshTRX.xcodeproj
  ├── MeshTRX/
  │   ├── App/
  │   │   └── MeshTRXApp.swift
  │   ├── Views/
  │   │   ├── SplashView.swift
  │   │   ├── MainTabView.swift
  │   │   ├── Voice/
  │   │   ├── Messages/
  │   │   ├── Files/
  │   │   ├── Map/
  │   │   └── Settings/
  │   ├── ViewModels/
  │   ├── Models/
  │   ├── Services/
  │   │   ├── BLEManager.swift
  │   │   ├── AudioEngine.swift
  │   │   ├── VoxEngine.swift
  │   │   └── LocationManager.swift
  │   ├── Codec2/
  │   │   ├── codec2/ (C sources)
  │   │   ├── Codec2Wrapper.swift
  │   │   └── MeshTRX-Bridging-Header.h
  │   ├── Utils/
  │   └── Resources/
  │       ├── Assets.xcassets
  │       └── Localizable.xcstrings
  ├── Info.plist
  └── MeshTRX.entitlements
  ```
- [x] Добавить Capabilities: Background Modes (BLE, Audio), локализация (ru, en)
- [x] Настроить Info.plist: NSBluetoothAlwaysUsageDescription, NSMicrophoneUsageDescription, NSLocationWhenInUseUsageDescription

---

## Этап 1 — Модели данных и состояние (1-2 дня)

Прямой порт Android моделей на Swift.

- [x] **Models.swift** — перечисления и структуры:
  ```swift
  enum BleState: String { case disconnected, scanning, connecting, connected }
  enum TxMode { case ptt, vox }
  enum ListenMode { case all, privateOnly }
  enum CallType { case all, private_, group, emergency }
  enum MessageStatus { case sending, sent, delivered, failed }
  enum FileStatus { case pending, transferring, done, failed }

  struct ChatMessage: Identifiable { ... }
  struct Peer: Identifiable { ... }
  struct FileTransfer: Identifiable { ... }
  struct IncomingCall { ... }
  struct RecentCall: Identifiable { ... }
  ```

- [x] **AppState.swift** — ObservableObject (аналог ServiceState):
  ```swift
  @MainActor
  class AppState: ObservableObject {
      @Published var bleState: BleState = .disconnected
      @Published var rssi: Int = 0
      @Published var snr: Int = 0
      @Published var channel: Int = 0
      @Published var isPttActive: Bool = false
      @Published var txMode: TxMode = .ptt
      @Published var messages: [ChatMessage] = []
      @Published var peers: [Peer] = []
      @Published var fileTransfers: [FileTransfer] = []
      @Published var myLat: Double? = nil
      @Published var myLon: Double? = nil
      // ...
  }
  ```

---

## Этап 2 — BLE-менеджер (3-4 дня)

Ключевой компонент — полный порт BLE-протокола.

- [x] **BLEManager.swift** — CoreBluetooth реализация:
  - CBCentralManager delegate (scan, connect, disconnect)
  - Фильтр по имени `MeshTRX-`
  - Nordic UART Service UUID: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
  - RX characteristic: `6E400002-...` (write)
  - TX characteristic: `6E400003-...` (notify)
  - Подписка на notifications
  - MTU negotiation

- [x] **Протокол команд** — парсинг и формирование пакетов:
  | Cmd | Константа | Описание |
  |-----|-----------|----------|
  | 0x01 | AUDIO_TX | Отправка аудио (64 байта Codec2) |
  | 0x02 | AUDIO_RX | Приём аудио |
  | 0x03 | PTT_START | Начало передачи |
  | 0x04 | PTT_END | Конец передачи |
  | 0x05 | SET_CHANNEL | Установка канала |
  | 0x06 | STATUS_UPDATE | Статус (канал, RSSI, SNR) |
  | 0x07 | SEND_MESSAGE | Отправка текста |
  | 0x08 | RECV_MESSAGE | Приём текста |
  | 0x09 | MESSAGE_ACK | Подтверждение |
  | 0x0A-0x0C | SETTINGS | Get/Set/Response JSON |
  | 0x0D | FILE_START | Начало файла |
  | 0x0E | FILE_CHUNK | Чанк файла (120 байт) |
  | 0x0F | FILE_RECV | Приём файла |
  | 0x10 | FILE_PROGRESS | Прогресс передачи |
  | 0x11 | SET_TX_MODE | PTT/VOX |
  | 0x14-0x16 | LOCATION | GPS данные, beacon |
  | 0x17 | PEER_SEEN | Обнаружение пира (28 байт) |
  | 0x18-0x20 | CALL_* | Вызовы и статусы |
  | 0x25-0x26 | PIN_* | Проверка PIN |
  | 0x27 | FILE_DATA | Данные файла |
  | 0x28 | SET_REPEATER | Конфиг ретранслятора |

- [x] Автопереподключение с бэкоффом (3с, 5с, 7с, 10с)
- [x] Сохранение последнего устройства (UserDefaults)
- [x] Background BLE (state restoration, CBCentralManagerOptionRestoreIdentifierKey)

---

## Этап 3 — Codec2 интеграция (2-3 дня)

- [x] Скопировать C-исходники Codec2 из `android/MeshTRX/app/src/main/jni/codec2/`
- [x] Создать bridging header:
  ```c
  // MeshTRX-Bridging-Header.h
  #include "codec2.h"
  ```
- [x] **Codec2Wrapper.swift** — Swift обёртка:
  ```swift
  class Codec2Wrapper {
      private var codec: OpaquePointer?

      init(mode: Int32 = CODEC2_MODE_3200) {
          codec = codec2_create(mode)
      }

      func encode(pcm: [Int16]) -> Data          // 160 samples → 8 bytes
      func decode(encoded: Data) -> [Int16]       // 8 bytes → 160 samples
      func encodePacket(pcm: [Int16]) -> Data     // 1280 samples → 64 bytes
      func decodePacket(encoded: Data) -> [Int16]  // 64 bytes → 1280 samples

      deinit { codec2_destroy(codec) }
  }
  ```
- [x] Добавить .c файлы в Xcode target, настроить compile flags (`-DCODEC2_MODE_EN_DEFAULT=0`)

---

## Этап 4 — Аудио движок (3-4 дня)

- [x] **AudioEngine.swift** — AVAudioEngine-based:
  - Запись: 8 кГц, mono, Int16
  - Воспроизведение: 8 кГц, mono
  - Audio Session категория: `.playAndRecord`, mode: `.voiceChat`
  - Маршрутизация: speaker / earpiece
  - Прерывания (звонки, другие приложения)

- [x] **Запись (TX):**
  - Install tap on input node
  - Конвертация sampleRate → 8 кГц (AVAudioConverter)
  - Буферизация 1280 сэмплов (8 фреймов × 160)
  - Codec2 encode → 64 байта → BLE send
  - RMS-мониторинг для VU-индикатора и VOX

- [x] **Воспроизведение (RX):**
  - Codec2 decode → PCM
  - Volume boost (0.5x – 3.0x)
  - Roger beep (1200 + 1600 Гц, 80 мс)
  - Очередь пакетов для плавного воспроизведения

- [x] **VoxEngine.swift** — state machine:
  ```
  IDLE → ATTACK → ACTIVE → HANGTIME → IDLE
  ```
  - Порог RMS (настраиваемый)
  - Attack time: 50 мс
  - Hangtime: 800 мс (настраиваемый)

- [x] Background audio (UIBackgroundModes: audio)

---

## Этап 5 — UI: Splash + Tab навигация (1 день)

- [x] **SplashView.swift:**
  - Логотип с fade-in анимацией
  - 2 секунды → переход к MainTabView

- [x] **MainTabView.swift:**
  - TabView с 5 табами: Voice, Messages, Files, Map, Settings
  - Иконки: mic.fill, message.fill, doc.fill, map.fill, gear
  - Badge на Messages (непрочитанные)
  - Запрос permissions при первом запуске

---

## Этап 6 — Voice экран (3-4 дня)

- [x] **VoiceView.swift:**
  - PTT-кнопка (большая, центральная)
  - Состояния: IDLE (серый), TX (красный), RX (зелёный), VOX_IDLE (синий), VOX_TX (красный)
  - Анимация колец при TX (на основе RMS уровня)
  - Переключатель PTT / VOX
  - Listen Mode: ALL / PRIVATE ONLY
  - Кнопки вызова: общий, приватный (выбор пира), экстренный
  - Список недавних вызовов

- [x] **PttButtonView.swift** — Canvas (SwiftUI Shape/Canvas):
  - Анимированные расходящиеся кольца
  - Пульсация центра
  - Long press gesture для PTT
  - Haptic feedback (UIImpactFeedbackGenerator)

- [x] **IncomingCallView.swift:**
  - Full-screen overlay (.fullScreenCover)
  - Цвета по типу: ALL=синий, PRIVATE=зелёный, GROUP=янтарный, EMERGENCY=красный
  - Accept / Reject кнопки
  - 30-сек автоотклонение
  - Вибрация (AudioServicesPlaySystemSound, паттерн по типу вызова)
  - Пульсирующий круг с иконкой

---

## Этап 7 — Messages экран (2-3 дня)

- [x] **MessagesView.swift:**
  - Поле ввода с ограничением 84 байта UTF-8
  - Счётчик оставшихся байт
  - Picker получателя: Все / конкретный пир
  - Фильтр по отправителю/получателю

- [x] **MessageBubbleView.swift:**
  - Исходящие: справа, синий
  - Входящие: слева, серый
  - Метаданные: время, RSSI, имя отправителя
  - Статус: отправляется → отправлено → доставлено / ошибка

- [x] Автоскролл к последнему сообщению
- [ ] Персистентность: SwiftData или JSON в Documents/

---

## Этап 8 — Files экран (3-4 дня)

- [x] **FilesView.swift:**
  - PhotosPicker для выбора изображений
  - DocumentPicker (fileImporter) для файлов
  - Превью перед отправкой
  - Picker получателя

- [x] **ImageProcessor.swift:**
  - Ресайз до 320×426 (aspect fit)
  - JPEG сжатие 70%
  - Коррекция EXIF ориентации (UIGraphicsContext)
  - Лимит 100 КБ

- [x] **Список передач (FileTransferRow):**
  - ProgressView для активных
  - Статусы: ожидание, передача, готово, ошибка

- [ ] Чанки 120 байт с задержкой 100 мс
- [ ] Хранение в Documents/transfers/

---

## Этап 9 — Map экран (3-4 дня)

- [x] **MapContentView** — MKMapView через UIViewRepresentable:
  - Маркеры пиров с callsign, дистанцией, RSSI
  - Линии от меня к пирам (MKPolyline)
  - showsUserLocation для своей позиции

- [x] **RadarView.swift** — Canvas (SwiftUI):
  - Концентрические кольца дистанции (4 кольца)
  - Вращение по компасу (heading)
  - Точки пиров с цветом по давности (<5 мин зелёный, <10 жёлтый, >10 красный)
  - 10 уровней масштаба (0.1 – 100 км)
  - Cardinal points (N/E/S/W)
  - Кнопки +/− для масштаба
  - Пульсирующий центр (своя позиция)

- [x] **Табы:** Map / Radar (кастомные кнопки)

- [x] **LocationManager.swift:**
  - CLLocationManager delegate
  - requestWhenInUseAuthorization
  - desiredAccuracy: kCLLocationAccuracyBest
  - distanceFilter: 5 м
  - Heading updates для радара

---

## Этап 10 — Settings экран (2 дня)

- [x] **SettingsView.swift** — Form/List:
  - Подключение: Connect / Disconnect / Forget Device
  - Callsign: TextField, отправка на устройство
  - Канал: Picker 0-22 (863.15 – 869.75 МГц, шаг 300 кГц)
  - TX Power: Slider (зависит от устройства)
  - Duty Cycle: Toggle (EU868)
  - Beacon interval: Picker
  - Peer timeout: Picker (15 мин – 24 ч)
  - RX Volume: Slider (50% – 300%)
  - PTT RMS порог: Slider (0 = выкл)
  - VOX: Threshold + Hangtime sliders
  - Ретранслятор: SSID, пароль, IP, вкл/выкл
  - О приложении: версия, сборка

- [ ] Сохранение в UserDefaults
- [ ] Синхронизация настроек с устройством через BLE (CMD 0x0A-0x0C)

---

## Этап 11 — Peer Discovery и управление (2 дня)

- [x] Парсинг PEER_SEEN пакетов (28 байт):
  - Device ID (4 байта)
  - Callsign (9 байт)
  - RSSI, SNR, TX Power
  - Battery %
  - GPS координаты (lat_e7, lon_e7)

- [x] Таймер очистки стухших пиров (каждые 60 сек)
- [x] Настраиваемый таймаут (15 мин – 24 ч)
- [ ] Персистентность списка пиров

---

## Этап 12 — Вызовы (2-3 дня)

- [x] Типы вызовов: ALL, PRIVATE, GROUP, EMERGENCY
- [x] Исходящий вызов → CMD 0x18-0x1B
- [x] Входящий вызов → CMD 0x1F → IncomingCallView
- [x] Accept / Reject / Cancel → CMD 0x1C-0x1E
- [ ] CallKit интеграция (опционально, для нативного UI звонков)
- [x] История вызовов (макс 20)
- [x] Вибрация по типу вызова:
  - EMERGENCY: 7 пульсов
  - PRIVATE: 3 пульса
  - GROUP: 2 пульса
  - ALL: 1 короткий

---

## Этап 13 — Фоновая работа (2-3 дня)

iOS имеет жёсткие ограничения на фоновую работу. Критические моменты:

- [ ] **BLE Background Mode:**
  - `UIBackgroundModes: bluetooth-central`
  - State preservation/restoration (CBCentralManagerOptionRestoreIdentifierKey)
  - Сканирование по UUID в фоне (не по имени!)
  - Ограниченный MTU в фоне

- [ ] **Audio Background Mode:**
  - `UIBackgroundModes: audio`
  - AVAudioSession active в фоне
  - Обработка прерываний (звонки, Siri)

- [ ] **Local Notifications:**
  - Входящие сообщения в фоне → UNUserNotificationCenter
  - Входящие вызовы → Critical alert или VoIP push (при наличии сервера)
  - Входящие файлы → notification с прогрессом

- [ ] **Bluetooth фоновый приём:**
  - Все BLE callback'и приходят в фоне
  - Буферизация данных при suspended state

---

## Этап 14 — Локализация (1 день)

- [ ] Localizable.xcstrings (Xcode 15 String Catalogs):
  - Русский (основной)
  - English
- [ ] Все строки через `String(localized:)`
- [ ] Форматы дат/чисел с учётом локали

---

## Этап 15 — Тестирование и отладка (3-5 дней)

- [ ] Unit-тесты: Codec2Wrapper, парсинг BLE-пакетов, модели
- [ ] UI-тесты: навигация, основные flow
- [ ] Тестирование с реальным Heltec V3:
  - Подключение / переподключение
  - PTT голос (качество, задержка)
  - VOX (срабатывание, отпускание)
  - Сообщения (отправка/приём, кириллица)
  - Файлы (фото, текст)
  - Карта и радар (GPS, компас)
  - Вызовы всех типов
  - Фоновая работа (BLE + аудио)
  - Множественные пиры
- [ ] Тестирование граничных случаев:
  - Потеря BLE-связи во время передачи
  - Большие файлы (100 КБ)
  - Быстрое переключение PTT
  - Одновременный приём от нескольких пиров

---

## Этап 16 — Полировка и релиз (2-3 дня)

- [ ] Иконка приложения (Assets.xcassets)
- [ ] Launch Screen
- [ ] Dark/Light тема (автоматическая)
- [ ] Haptic feedback на ключевых действиях
- [ ] Оптимизация энергопотребления
- [ ] TestFlight → бета-тестирование
- [ ] App Store Review Guidelines — проверка:
  - Bluetooth usage описание
  - Микрофон usage описание
  - Геолокация usage описание
  - Background modes обоснование

---

## Отличия от Android-реализации

| Аспект | Android | iOS |
|--------|---------|-----|
| BLE API | BluetoothGatt | CoreBluetooth |
| Codec2 | JNI (C → Java) | Bridging Header (C → Swift) |
| Аудио | AudioRecord/AudioTrack | AVAudioEngine |
| Карта | OSMDroid | MapKit |
| Фон | Foreground Service | Background Modes (ограниченно) |
| Уведомления о вызовах | Fullscreen Activity | Local Notification + fullScreenCover |
| Хранение | SharedPreferences | UserDefaults + SwiftData |
| Локализация | strings.xml | Localizable.xcstrings |
| Навигация | Fragments + BottomNav | TabView (SwiftUI) |
| Кастом Canvas | Android Canvas | SwiftUI Canvas / Shape |

---

## Оценка трудозатрат

| Этап | Дней |
|------|------|
| 0 — Инициализация | 1-2 |
| 1 — Модели | 1-2 |
| 2 — BLE | 3-4 |
| 3 — Codec2 | 2-3 |
| 4 — Аудио | 3-4 |
| 5 — Splash + Tab | 1 |
| 6 — Voice | 3-4 |
| 7 — Messages | 2-3 |
| 8 — Files | 3-4 |
| 9 — Map | 3-4 |
| 10 — Settings | 2 |
| 11 — Peers | 2 |
| 12 — Вызовы | 2-3 |
| 13 — Фоновая работа | 2-3 |
| 14 — Локализация | 1 |
| 15 — Тестирование | 3-5 |
| 16 — Релиз | 2-3 |
| **Итого** | **~35-50 дней** |

---

## Порядок реализации (рекомендуемый)

```
Этап 0 → 1 → 2 → 3 → 4 → 5 → 6 → 10 → 7 → 8 → 9 → 11 → 12 → 13 → 14 → 15 → 16
         │       │       │
         └───────┴───────┘
         Можно параллельно:
         BLE + Codec2 + Audio
```

Критический путь: **BLE → Codec2 → Audio → Voice UI** — это ядро приложения, без которого остальное не тестируется на реальном устройстве.
