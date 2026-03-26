# MeshTRX iOS — Лог разработки

---

## 2026-03-24 — Начало проекта

### Решения
- iOS-приложение будет портом Android версии 4.2.0 (полный паритет функций)
- Стек: Swift/SwiftUI, CoreBluetooth, AVAudioEngine, MapKit
- **Среда разработки: Mac Mini 2012 (16 ГБ RAM, SSD)** — нативный Mac
- macOS: **Monterey 12** через OpenCore Legacy Patcher
- Xcode: **14.2** (Swift 5.7, iOS 16 SDK, SwiftUI 4.0)
- Тестовое устройство: iPhone (есть) + Heltec V3 (есть)
- Бюджет: $0

### Создано
- `ios/IMPLEMENTATION_PLAN.md` — план реализации из 17 этапов (~35-50 дней)
- `ios/DEV_LOG.md` — этот лог

---

## 2026-03-26 — Этап 0: Инициализация проекта

### Xcode
- Xcode 14.2 установлен, Swift 5.7.2, xcode-select настроен
- Additional Tools for Xcode 14.2 установлены в /Applications/Utilities/

### Создано
- Xcode-проект: `ios/MeshTRX/MeshTRX.xcodeproj`
- Bundle ID: `com.meshtrx.app`, iOS 16.0+, Swift 5.7
- Структура папок: App, Views (Voice/Messages/Files/Map/Settings), Models, ViewModels, Services, Codec2, Utils, Resources

### Файлы
- `MeshTRXApp.swift` — точка входа, SwiftUI App
- `SplashView.swift` — splash экран с fade-in анимацией, 2 сек
- `MainTabView.swift` — 5 табов (Voice, Messages, Files, Map, Settings)
- `VoiceView.swift`, `MessagesView.swift`, `FilesView.swift`, `MapTabView.swift`, `SettingsView.swift` — заглушки
- `Models.swift` — порт Android моделей (BleState, TxMode, ChatMessage, Peer, FileTransfer, и т.д.)
- `AppState.swift` — ObservableObject, аналог ServiceState из Android
- `Codec2Wrapper.swift` — Swift-обёртка для C-библиотеки Codec2
- `MeshTRX-Bridging-Header.h` — bridging header для C→Swift
- `Info.plist` — Background Modes (BLE, Audio), usage descriptions (Bluetooth, Microphone, Location)
- `MeshTRX.entitlements` — entitlements
- Codec2 C-исходники скопированы из Android (51 файл)

### Сборка
- `xcodebuild build` — **SUCCESS** (4 warning'а в Codec2 C-коде — long→int, не критично)
- Исправлен `#include <codec2/version.h>` → `"codec2/version.h"` в codec2.h

### Следующий шаг
- ~~Этап 1: Модели данных~~ — сделано в рамках Этапа 0
- Этап 2: BLE-менеджер

---

## 2026-03-26 — Этапы 1-3: Модели, BLE, Codec2

### Этап 1 — Модели данных
- Уже реализован в рамках Этапа 0 (Models.swift + AppState.swift полностью портированы из Android)

### Этап 2 — BLE-менеджер
- `BLEProtocol.swift` — константы команд (BLECmd), билдеры пакетов (BLEPacket), UUID'ы Nordic UART Service, хелперы парсинга Data
- `BLEManager.swift` — полный порт CoreBluetooth:
  - CBCentralManager + CBPeripheral delegate
  - Сканирование по UUID сервиса, фильтр MeshTRX-
  - Подключение, MTU negotiation, service/characteristics discovery
  - Подписка на TX characteristic (notify)
  - Отправка через RX characteristic (write without response)
  - Автопереподключение с backoff (3с, 5с, 7с, 10с), до 10 попыток
  - Сохранение последнего устройства (UserDefaults)
  - Background BLE: CBCentralManagerOptionRestoreIdentifierKey + willRestoreState
  - Connect timeout (10 сек), scan timeout (10 сек)
  - Все convenience-методы: sendAudioData, sendPttStart/End, setChannel, sendMessage, sendSettings, sendPinCheck, sendLocationUpdate, sendRepeaterConfig, setTxMode

### Этап 3 — Codec2
- Уже реализован в рамках Этапа 0 (C-исходники + Codec2Wrapper.swift + bridging header)

### Сборка
- `xcodebuild build` — **SUCCESS**
- Автогенерация pbxproj через Python-скрипт (auto-discovery Swift/C/H файлов)

---

## 2026-03-26 — Этап 4: Аудио движок

### AudioEngine.swift
- AVAudioEngine-based запись и воспроизведение
- Audio Session: `.playAndRecord`, mode `.voiceChat`, defaultToSpeaker
- Запись: tap на inputNode → AVAudioConverter (device rate → 8кГц mono Int16) → буферизация 1280 сэмплов → Codec2 encode → callback
- RMS мониторинг: вычисляется на каждом буфере (для VOX и VU-meter)
- Squelch: порог RMS, ниже которого звук не кодируется
- Воспроизведение: AVAudioPlayerNode, Codec2 decode → volume boost → Float32 буфер → scheduleBuffer
- Volume boost: 0.5x – 3.0x (усиление приёма)
- Roger beep: 1200 + 1600 Гц, 80мс, fade in/out
- Маршрутизация: speaker / earpiece
- Обработка прерываний (звонки) и route changes
- PTT режим: startRecording/stopRecording (кодирует сразу)
- VOX режим: startVoxMonitoring (слушает RMS, кодирует когда sendAudio=true)
- @Volatile property wrapper для thread-safe sendAudio флага

### VoxEngine.swift
- State machine: IDLE → ATTACK → ACTIVE → HANGTIME → IDLE
- Порог RMS (настраиваемый, default 800)
- Attack time: 50мс (фильтр щелчков/помех)
- Hangtime: 800мс (настраиваемый)
- Callbacks: onTxActivated, onTxDeactivated, onStateChanged

### Сборка
- `xcodebuild build` — **SUCCESS** (15 Swift + 24 C файлов)

---

## 2026-03-26 — Этапы 5-6: UI Splash/Tab + Voice экран

### Этап 5 — Splash + Tab
- Уже реализован в рамках Этапа 0 (SplashView + MainTabView)

### Этап 6 — Voice экран

**Colors.swift** — полный порт цветовой палитры из Android (green, blue, red, amber, text, bg, nav)

**PttButtonView.swift** — кастомная PTT-кнопка на SwiftUI Canvas:
- 5 состояний: IDLE (зелёная), TX (красная + волны), RX (синяя), VOX_IDLE (зелёная "VOX"), VOX_TX (красная "VOX TX")
- Анимированные расходящиеся волны при TX (интенсивность зависит от RMS)
- TimelineView для 60fps анимации
- DragGesture для PTT (onChanged=down, onEnded=up)
- Текстовые лейблы через GraphicsContext.resolve + draw

**VoiceView.swift** — полный Voice экран:
- Статус-бар: точка подключения, статус, канал, RSSI
- PTT-кнопка (центральная, 180×180)
- VOX state label (авто-скрытие в PTT режиме)
- Панель управления: speaker toggle, PTT/VOX переключатель, TX/RX индикатор
- Listen mode: ALL / PRIVATE ONLY (два кнопки-сегмента)
- Кнопки вызова: Общий, Приватный, SOS
- Список недавних вызовов (до 5)
- Тёмная тема (AppColors.bgPrimary фон)

### Сборка
- `xcodebuild build` — **SUCCESS** (17 Swift + 24 C файлов)
- Исправлены: switch с отрицательными range (Swift 5.7), onChange синтаксис

---

## 2026-03-26 — Этапы 7 и 10: Messages + Settings

### Этап 7 — Messages
- **MessagesView.swift** — полный чат экран:
  - Фильтр-бар: выбор получателя (Все / конкретный пир), фильтр по контактам (Menu)
  - ScrollView + LazyVStack с автоскроллом к последнему сообщению
  - Input bar: TextField, счётчик оставшихся байт (84 UTF-8), кнопка отправки
  - Валидация: disabled при отключении, при превышении лимита
- **MessageBubbleView** (встроена в MessagesView):
  - Исходящие: справа, синий фон (AppColors.blueBg)
  - Входящие: слева, тёмный фон (AppColors.bgElevated), имя отправителя зелёным
  - Метаданные: время, получатель, RSSI
  - Иконки статуса: clock (sending), checkmark (sent), checkmark.circle.fill (delivered), exclamationmark.circle (failed)

### Этап 10 — Settings
- **SettingsView.swift** — Form-based настройки:
  - Подключение: статус с цветной точкой, кнопки Connect/Disconnect/Forget
  - Радио: позывной, канал (0-22, частоты), TX power slider (2-30 dBm, EU warning >14), duty cycle, beacon interval, peer timeout
  - Аудио: громкость приёма (50-300%), шумоподавление PTT
  - VOX: порог (100-5000), hangtime (200-3000мс)
  - Хранение: история файлов
  - Ретранслятор: SSID/пароль/IP, включить/выключить с confirmation alert
  - О приложении: версия, платформа
  - Кнопка "Применить" — формирует JSON и отправляет на устройство

### Сборка
- `xcodebuild build` — **SUCCESS** (17 Swift + 24 C файлов)

---

## 2026-03-26 — Этапы 8-9: Files + Map/Radar

### Этап 8 — Files
- **ImageProcessor.swift** — resize 320×426, JPEG 70%, EXIF fix, лимит 100 КБ
- **FilesView.swift** — полный Files экран:
  - PhotosPicker (SwiftUI) для фото, fileImporter для произвольных файлов
  - Превью перед отправкой (image + размер + разрешение)
  - Валидация размера (>100 КБ — warning)
  - Кнопки Отправить / Отмена
  - FileTransferRow: направление, имя, размер, тип, время, ProgressView / статус иконка

### Этап 9 — Map + Radar
- **LocationManager.swift** — CLLocationManager: GPS + heading, requestWhenInUseAuthorization
- **MapTabView.swift** — табы Карта / Радар:
  - MapContentView (UIViewRepresentable → MKMapView): маркеры пиров, polylines до пиров, showsUserLocation
  - GPS status overlay (координаты или "ожидание")
  - Синхронизация GPS → appState.myLat/myLon
- **RadarView.swift** — Canvas (SwiftUI):
  - 4 кольца дистанции с подписями
  - Вращение по компасу (heading)
  - Пиры: точки с цветом по давности, callsign + дистанция + RSSI
  - 10 уровней масштаба, кнопки +/−
  - Cardinal points N/S/E/W
  - Пульсирующий центр (своя позиция)

### Сборка
- `xcodebuild build` — **SUCCESS** (20 Swift + 24 C файлов)

---

## 2026-03-26 — Этапы 11-12 + Интеграция

### MeshTRXController.swift — центральный контроллер (аналог Android MeshTRXService)
- Связывает BLEManager ↔ AudioEngine ↔ VoxEngine ↔ AppState
- **handleBleData** — диспетчер всех входящих BLE-команд:
  - AUDIO_RX: decode + play (с поддержкой старой прошивки без senderId)
  - STATUS_UPDATE: канал, RSSI, SNR
  - RECV_MESSAGE: парсинг текста + senderId, добавление в messages, badge
  - PEER_SEEN: парсинг 28 байт (ID, callsign, coords, RSSI, SNR, TX, battery)
  - GET_LOCATION: ответ GPS координатами
  - SETTINGS_RESP: логирование JSON
  - PIN_RESULT: авторизация или отключение
  - FILE_PROGRESS/FILE_RECV/FILE_DATA: приём файлов по чанкам
  - INCOMING_CALL: создание IncomingCall → fullScreenCover
  - CALL_STATUS: завершение вызова
- **PTT**: pttDown/pttUp — BLE + AudioEngine
- **VOX**: setTxMode — переключение PTT↔VOX, запуск/остановка мониторинга
- **Calls**: callAll, callPrivate, callEmergency, acceptCall, rejectCall + история (макс 20)
- **Messages**: sendTextMessage — BLE + добавление в AppState
- **Peers**: парсинг PEER_SEEN + таймер очистки каждые 60 сек
- **Files**: handleFileRecv + handleFileData — буферизация входящих файлов
- **Settings**: setCallSign, applySettings

### Этап 12 — IncomingCallView.swift
- Полноэкранный overlay (fullScreenCover)
- Цвета по типу: ALL=синий, PRIVATE=зелёный, GROUP=янтарный, EMERGENCY=красный
- Пульсирующий круг с иконкой по типу
- RSSI, обратный отсчёт 30 сек, авто-reject
- Вибрация: паттерн по типу (SOS=7, PRIVATE=3, GROUP=2, ALL=1)
- Accept / Reject кнопки

### MeshTRXApp.swift — обновлён
- Инициализация MeshTRXController как @StateObject
- environmentObject для controller
- fullScreenCover для IncomingCallView привязан к appState.incomingCall

### Сборка
- `xcodebuild build` — **SUCCESS** (22 Swift + 24 C файлов)

---

## 2026-03-26 — Подключение UI к контроллеру

Все экраны подключены к MeshTRXController через `@EnvironmentObject`:

- **VoiceView**: PTT gesture → pttDown/pttUp, VOX toggle → setTxMode, speaker → routeToSpeaker/Earpiece, call buttons → callAll/callEmergency, redial → callPrivate/callAll
- **MessagesView**: send → sendTextMessage, unread badge сброс при открытии
- **SettingsView**: connect/disconnect/forget, apply settings → JSON → BLE, channel change, callsign, audio/VOX настройки → engine/voxEngine
- **FilesView**: send photo → sendFile(photo), send file → sendFile(binary), все через controller
- **MeshTRXController.sendFile**: FILE_START header + Timer-based chunk sending (100ms между чанками), прогресс в реальном времени

### Сборка
- `xcodebuild build` — **SUCCESS** (22 Swift + 24 C файлов)
- Исправлены Swift 5.7 concurrency ошибки: Timer вместо DispatchQueue.global для file transfer

### Итого на текущий момент
Приложение функционально полное — все экраны реализованы и подключены к BLE/Audio/State.

### Следующий шаг
- Тестирование на реальном iPhone + Heltec V3
- Этап 13: фоновая работа (BLE/Audio background уже в Info.plist, state restoration в BLEManager)
- Этап 14: локализация (ru/en)
- Этап 15-16: тестирование, полировка, релиз
