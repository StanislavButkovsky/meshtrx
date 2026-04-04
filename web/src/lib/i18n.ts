export type Locale = 'ru' | 'en';

export const translations = {
  // Header / Nav
  'nav.home': { ru: 'Главная', en: 'Home' },
  'nav.download': { ru: 'Скачать', en: 'Download' },
  'nav.flash': { ru: 'Прошивка', en: 'Flash' },
  'nav.docs': { ru: 'Документация', en: 'Docs' },
  'nav.about': { ru: 'О проекте', en: 'About' },

  // Hero
  'hero.badge': { ru: 'Проект в активной разработке', en: 'Project in active development' },
  'hero.tagline': { ru: 'Off-grid голосовая связь через LoRa mesh-сеть', en: 'Off-grid voice communication via LoRa mesh network' },
  'hero.desc': { ru: 'Голос, сообщения и файлы без интернета. Heltec V3 + Android + LoRa mesh.', en: 'Voice, messages and files without internet. Heltec V3 + Android + LoRa mesh.' },
  'hero.download': { ru: 'Скачать APK', en: 'Download APK' },
  'hero.flash': { ru: 'Прошить модуль', en: 'Flash Firmware' },

  // Stats
  'stats.range': { ru: 'Дальность связи', en: 'Communication range' },
  'stats.range.hint': { ru: 'С ретранслятором — ещё дальше', en: 'Even further with a repeater' },
  'stats.channels': { ru: 'LoRa каналов', en: 'LoRa channels' },
  'stats.latency': { ru: 'Задержка голоса', en: 'Voice latency' },
  'stats.bitrate': { ru: 'Codec2 битрейт', en: 'Codec2 bitrate' },
  'stats.channels.unit': { ru: 'кан.', en: 'ch.' },
  'stats.latency.unit': { ru: 'мс', en: 'ms' },
  'stats.range.unit': { ru: 'км', en: 'km' },

  // How it works
  'how.title': { ru: 'Как это работает', en: 'How it works' },
  'how.step1.title': { ru: 'Прошейте Heltec V3', en: 'Flash Heltec V3' },
  'how.step1.desc': { ru: 'Загрузите прошивку MeshTRX через USB прямо в браузере.', en: 'Upload MeshTRX firmware via USB directly in the browser.' },
  'how.step2.title': { ru: 'Установите APK', en: 'Install APK' },
  'how.step2.desc': { ru: 'Скачайте приложение MeshTRX на Android и подключитесь к модулю по BLE.', en: 'Download MeshTRX app on Android and connect to the module via BLE.' },
  'how.step3.title': { ru: 'Общайтесь', en: 'Communicate' },
  'how.step3.desc': { ru: 'Голос, сообщения и файлы — всё работает без интернета через LoRa mesh.', en: 'Voice, messages and files — everything works without internet via LoRa mesh.' },

  // Features
  'features.title': { ru: 'Возможности', en: 'Features' },
  'feature.voice.title': { ru: 'Голосовая связь', en: 'Voice communication' },
  'feature.voice.desc': { ru: 'PTT голос через LoRa с кодеком Codec2. Задержка ~160ms, слышно чётко.', en: 'PTT voice over LoRa with Codec2 codec. ~160ms latency, clear audio.' },
  'feature.msg.title': { ru: 'Сообщения', en: 'Messages' },
  'feature.msg.desc': { ru: 'Текстовые сообщения с гарантированной доставкой и подтверждением.', en: 'Text messages with guaranteed delivery and confirmation.' },
  'feature.file.title': { ru: 'Передача файлов', en: 'File transfer' },
  'feature.file.desc': { ru: 'Отправка файлов через mesh-сеть с разбиением на фрагменты.', en: 'Send files over mesh network with fragmentation.' },
  'feature.map.title': { ru: 'Карта и радар', en: 'Map & Radar' },
  'feature.map.desc': { ru: 'GPS-позиции всех участников на карте. Радар ближайших узлов.', en: 'GPS positions of all participants on map. Nearby nodes radar.' },
  'feature.call.title': { ru: 'Групповые вызовы', en: 'Group calls' },
  'feature.call.desc': { ru: 'Голосовые вызовы на весь канал или адресный вызов конкретного узла.', en: 'Voice calls to entire channel or direct call to specific node.' },
  'feature.relay.title': { ru: 'Ретранслятор', en: 'Repeater' },
  'feature.relay.desc': { ru: 'Любой узел может работать как ретранслятор, увеличивая покрытие сети.', en: 'Any node can work as a repeater, extending network coverage.' },

  // Hardware
  'hw.title': { ru: 'Оборудование', en: 'Hardware' },
  'hw.subtitle': { ru: 'Основной модуль MeshTRX', en: 'Main MeshTRX module' },
  'hw.chip': { ru: 'Чип', en: 'Chip' },
  'hw.lora': { ru: 'LoRa', en: 'LoRa' },
  'hw.power': { ru: 'Мощность', en: 'Power' },
  'hw.ble': { ru: 'BLE', en: 'BLE' },
  'hw.battery': { ru: 'Питание', en: 'Battery' },
  'hw.antenna': { ru: 'Антенна', en: 'Antenna' },

  // Roadmap
  'roadmap.badge': { ru: 'В разработке', en: 'In development' },
  'roadmap.title': { ru: 'Планы развития', en: 'Roadmap' },
  'roadmap.desc': { ru: 'Проект активно развивается. Вот что будет добавлено в ближайших версиях.', en: 'The project is actively evolving. Here\'s what will be added in upcoming versions.' },
  'roadmap.soon': { ru: 'Скоро', en: 'Soon' },
  'roadmap.planned': { ru: 'Планируется', en: 'Planned' },
  'roadmap.v4.title': { ru: 'Heltec LoRa 32 V4', en: 'Heltec LoRa 32 V4' },
  'roadmap.v4.desc': { ru: 'Поддержка нового модуля: ESP32-S3R2, SX1262 до 28 dBm, OLED 0.96", USB-C, разъём для GNSS-модуля и солнечной панели.', en: 'New module support: ESP32-S3R2, SX1262 up to 28 dBm, 0.96" OLED, USB-C, GNSS and solar panel connectors.' },
  'roadmap.encrypt.title': { ru: 'Шифрование', en: 'Encryption' },
  'roadmap.encrypt.desc': { ru: 'End-to-end шифрование голоса и сообщений для приватной связи.', en: 'End-to-end encryption for voice and messages for private communication.' },
  'roadmap.devices.title': { ru: 'Расширение устройств', en: 'More devices' },
  'roadmap.devices.desc': { ru: 'Поддержка других LoRa-плат: LilyGo T-Beam, RAK WisBlock, и других ESP32 + SX1262 модулей.', en: 'Support for other LoRa boards: LilyGo T-Beam, RAK WisBlock, and other ESP32 + SX1262 modules.' },
  'roadmap.desktop.title': { ru: 'Desktop-клиент', en: 'Desktop client' },
  'roadmap.desktop.desc': { ru: 'Приложение для Windows/Linux/macOS с подключением через USB напрямую.', en: 'Windows/Linux/macOS app with direct USB connection.' },
  'roadmap.mesh.title': { ru: 'Mesh-маршрутизация', en: 'Mesh routing' },
  'roadmap.mesh.desc': { ru: 'Улучшенные алгоритмы маршрутизации для многохоповых сетей с автовыбором оптимального пути.', en: 'Improved routing algorithms for multi-hop networks with automatic optimal path selection.' },

  // CTA
  'cta.title': { ru: 'Готовы попробовать?', en: 'Ready to try?' },
  'cta.desc': { ru: 'Скачайте приложение, прошейте модуль и начните общение без интернета.', en: 'Download the app, flash the module and start communicating without internet.' },
  'cta.apk': { ru: 'Скачать APK', en: 'Download APK' },
  'cta.flash': { ru: 'Прошить модуль', en: 'Flash module' },

  // Download page
  'dl.title': { ru: 'Скачать', en: 'Download' },
  'dl.android': { ru: 'MeshTRX для Android', en: 'MeshTRX for Android' },
  'dl.btn': { ru: 'Скачать APK', en: 'Download APK' },
  'dl.qr.title': { ru: 'QR-код для скачивания', en: 'QR code for download' },
  'dl.qr.hint': { ru: 'Отсканируйте камерой телефона', en: 'Scan with your phone camera' },
  'dl.install.title': { ru: 'Инструкция установки', en: 'Installation guide' },
  'dl.step1.title': { ru: 'Скачайте APK', en: 'Download APK' },
  'dl.step1.desc': { ru: 'Нажмите кнопку "Скачать APK" выше или отсканируйте QR-код.', en: 'Click "Download APK" above or scan the QR code.' },
  'dl.step2.title': { ru: 'Разрешите установку', en: 'Allow installation' },
  'dl.step2.desc': { ru: 'В настройках Android разрешите установку из неизвестных источников для браузера.', en: 'In Android settings, allow installation from unknown sources for the browser.' },
  'dl.step3.title': { ru: 'Установите приложение', en: 'Install the app' },
  'dl.step3.desc': { ru: 'Откройте скачанный файл и следуйте инструкции установки.', en: 'Open the downloaded file and follow the installation instructions.' },
  'dl.step4.title': { ru: 'Подключите модуль', en: 'Connect the module' },
  'dl.step4.desc': { ru: 'Включите Bluetooth, откройте MeshTRX и подключитесь к Heltec V3.', en: 'Enable Bluetooth, open MeshTRX and connect to Heltec V3.' },
  'dl.firmware': { ru: 'Прошивка LoRa-модуля', en: 'LoRa module firmware' },
  'dl.firmware_hint': { ru: 'V3 — стабильная. V4 с GC1109 PA — beta, требует тестирования.', en: 'V3 — stable. V4 with GC1109 PA — beta, needs testing.' },

  // Flash page
  'flash.title': { ru: 'Прошивка модуля', en: 'Flash Firmware' },
  'flash.desc': { ru: 'Прошейте Heltec V3 прямо из браузера через Web Serial API.', en: 'Flash Heltec V3 directly from the browser via Web Serial API.' },
  'flash.supported': { ru: 'Web Serial поддерживается', en: 'Web Serial supported' },
  'flash.unsupported': { ru: 'Web Serial не поддерживается', en: 'Web Serial not supported' },
  'flash.unsupported.desc': { ru: 'Для прошивки используйте Chrome или Edge на десктопе.', en: 'Use Chrome or Edge on desktop for flashing.' },
  'flash.step1': { ru: 'Подключить USB', en: 'Connect USB' },
  'flash.step2': { ru: 'Выбрать порт', en: 'Select port' },
  'flash.step3': { ru: 'Прошить', en: 'Flash' },
  'flash.select': { ru: 'Выбрать порт', en: 'Select port' },
  'flash.btn': { ru: 'Прошить', en: 'Flash' },
  'flash.btn.busy': { ru: 'Прошивка...', en: 'Flashing...' },
  'flash.idle': { ru: 'Подключите Heltec V3 по USB и нажмите "Выбрать порт"', en: 'Connect Heltec V3 via USB and click "Select port"' },
  'flash.port.ok': { ru: 'Порт выбран. Зажмите BOOT на модуле, нажмите RST, отпустите BOOT — затем нажмите "Прошить".', en: 'Port selected. Hold BOOT on module, press RST, release BOOT — then click "Flash".' },
  'flash.connecting': { ru: 'Подключение к устройству...', en: 'Connecting to device...' },
  'flash.erasing': { ru: 'Стирание flash...', en: 'Erasing flash...' },
  'flash.writing': { ru: 'Запись прошивки...', en: 'Writing firmware...' },
  'flash.verifying': { ru: 'Проверка...', en: 'Verifying...' },
  'flash.done': { ru: 'Прошивка завершена!', en: 'Flashing complete!' },
  'flash.stage.idle': { ru: 'Ожидание', en: 'Idle' },
  'flash.stage.connecting': { ru: 'Подключение', en: 'Connecting' },
  'flash.stage.erasing': { ru: 'Стирание', en: 'Erasing' },
  'flash.stage.flashing': { ru: 'Запись', en: 'Writing' },
  'flash.stage.verifying': { ru: 'Проверка', en: 'Verifying' },
  'flash.stage.done': { ru: 'Готово', en: 'Done' },
  'flash.stage.error': { ru: 'Ошибка', en: 'Error' },
  'flash.loading': { ru: 'Загрузка...', en: 'Loading...' },
  'flash.unavailable': { ru: 'Временно недоступно', en: 'Temporarily unavailable' },
  'flash.unavailable.desc': { ru: 'Веб-прошивка находится в доработке. Используйте PlatformIO для прошивки модуля.', en: 'Web flashing is being reworked. Use PlatformIO to flash the module.' },

  // About page
  'about.title': { ru: 'О проекте', en: 'About' },
  'about.desc1': { ru: 'MeshTRX — это система off-grid голосовой связи, построенная на LoRa mesh-сети. Позволяет общаться голосом, обмениваться текстовыми сообщениями и передавать файлы без интернета и сотовых сетей.', en: 'MeshTRX is an off-grid voice communication system built on a LoRa mesh network. It enables voice communication, text messaging and file transfer without internet or cellular networks.' },
  'about.desc2': { ru: 'Система работает на модулях Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262) и Android-приложении, подключённом к модулю по BLE. Голос кодируется с помощью Codec2 (3200 bps) и передаётся через LoRa на расстояние до 5+ км.', en: 'The system runs on Heltec WiFi LoRa 32 V3 modules (ESP32-S3 + SX1262) and an Android app connected via BLE. Voice is encoded with Codec2 (3200 bps) and transmitted over LoRa up to 5+ km.' },
  'about.usage': { ru: 'Применение', en: 'Use cases' },
  'about.usage1': { ru: 'Туристические походы и экспедиции без сотовой связи', en: 'Hiking and expeditions without cellular coverage' },
  'about.usage2': { ru: 'Координация на мероприятиях и фестивалях', en: 'Coordination at events and festivals' },
  'about.usage3': { ru: 'Аварийная связь при отключении инфраструктуры', en: 'Emergency communication when infrastructure is down' },
  'about.usage4': { ru: 'Охрана территорий и сельское хозяйство', en: 'Security and agriculture' },
  'about.usage5': { ru: 'Любительская радиосвязь и эксперименты с mesh-сетями', en: 'Amateur radio and mesh network experiments' },
  'about.license': { ru: 'Лицензия', en: 'License' },
  'about.license.desc': { ru: 'Проект распространяется под лицензией', en: 'The project is licensed under' },
  'about.license.terms': { ru: 'Вы можете свободно использовать, копировать и распространять материалы проекта с указанием авторства. Коммерческое использование без разрешения запрещено.', en: 'You are free to use, copy and distribute the project materials with attribution. Commercial use without permission is prohibited.' },
  'about.links': { ru: 'Ссылки', en: 'Links' },
  'about.site': { ru: 'Сайт', en: 'Website' },

  // Docs
  'docs.toc': { ru: 'Содержание', en: 'Table of contents' },

  // Footer
  'footer.tagline': { ru: 'Off-grid голосовая связь через LoRa mesh-сеть', en: 'Off-grid voice communication via LoRa mesh network' },
} as const;

export type TranslationKey = keyof typeof translations;
