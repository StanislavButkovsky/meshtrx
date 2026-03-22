# MeshTRX

**Decentralized voice mesh network over LoRa + BLE**

MeshTRX is an open-source communication system that provides PTT voice, text messaging, file transfer, and GPS tracking over LoRa radio — independent of cellular networks and internet.

```
[Phone A] <--BLE--> [Heltec V3 A] <--LoRa 868MHz--> [Heltec V3 B] <--BLE--> [Phone B]
```

## Features

- **Voice** — Codec2 3200 bps, PTT and VOX modes, roger beep, noise gate
- **Messaging** — Text chat up to 84 chars, broadcast or direct
- **Files** — Photo and file transfer up to 100 KB over LoRa
- **Map & Radar** — OpenStreetMap + tactical radar with zoom and contrast, peer positions via GPS
- **Splash screen** — Animated launch screen with version info
- **Calls** — All-call, private, group (up to 8 members)
- **23 Channels** — EU868 band (863–870 MHz), 300 kHz spacing
- **Repeater** — Store & forward mode with WiFi web monitoring
- **Localization** — English and Russian

## Hardware

- **Device**: [Heltec WiFi LoRa 32 V3](https://heltec.org/project/wifi-lora-32-v3/) (ESP32-S3 + SX1262)
- **Phone**: Android 5.0+ with BLE
- **Range**: 5+ km line of sight, extendable with repeaters

## Quick Start

### Flash firmware

```bash
cd firmware
pio run --target upload --upload-port /dev/ttyUSB0
```

### Build and install Android app

```bash
cd android/MeshTRX
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

### Connect

1. Open MeshTRX app → Settings → Connect
2. Press button on device (>1 sec) to show PIN
3. Enter PIN in app
4. Start talking!

## Project Structure

```
meshtrx/
├── firmware/              # ESP32 firmware (PlatformIO + Arduino)
│   ├── src/
│   │   ├── main.cpp       # FreeRTOS tasks, BLE commands, LoRa RX/TX
│   │   ├── lora_radio.*   # SX1262 driver, 23 channels
│   │   ├── ble_service.*  # NimBLE NUS, 40+ commands
│   │   ├── audio_codec.*  # Codec2 3200 wrapper
│   │   ├── oled_display.* # SSD1306 128x64
│   │   ├── packet.h       # All LoRa packet structures
│   │   ├── repeater.*     # Store & forward repeater
│   │   ├── wifi_monitor.* # HTTP status page for repeater
│   │   ├── battery.*      # ADC voltage reading
│   │   ├── beacon.*       # Periodic position beacon
│   │   ├── vox.*          # Voice-activated TX
│   │   ├── roger_beep.*   # End-of-transmission tone
│   │   └── call_manager.* # Call system (all/private/group)
│   └── lib/codec2/        # Codec2 1.2.0 (modes 1200 + 3200)
├── android/MeshTRX/       # Android app (Kotlin)
│   └── app/src/main/
│       ├── java/.../
│       │   ├── SplashActivity.kt        # Animated launch screen
│       │   ├── MainActivity.kt
│       │   ├── MeshTRXService.kt        # Foreground service
│       │   ├── BleManager.kt            # BLE connection
│       │   ├── AudioEngine.kt           # Record/playback + Codec2
│       │   ├── LocationHelper.kt        # GPS location provider
│       │   ├── model/Models.kt          # Data classes
│       │   └── ui/                      # UI fragments & views
│       │       ├── VoiceFragment.kt     # PTT/VOX tab
│       │       ├── MessagesFragment.kt  # Chat tab
│       │       ├── FilesFragment.kt     # File transfer tab
│       │       ├── MapFragment.kt       # OpenStreetMap tab
│       │       ├── SettingsFragment.kt  # Settings tab
│       │       ├── RadarView.kt         # Tactical radar view
│       │       ├── PttButtonView.kt     # Custom PTT button
│       │       └── CallPickerSheet.kt   # Call target picker
│       ├── jni/                         # Codec2 JNI (C/C++)
│       └── res/                         # Layouts, strings (en/ru)
├── web/                  # Public website (Next.js)
│   └── src/
│       ├── app/          # Pages: /, /download, /flash, /docs, /about
│       ├── components/   # React components
│       └── lib/          # Constants, i18n, flash utils
├── docs/
│   └── USER_GUIDE.md      # Full user documentation
├── MESHTRX_SPEC.md         # Technical specification
├── PROJECT_LOG.md           # Development log
└── README.md                # This file
```

## Radio Parameters

| Parameter | Value |
|-----------|-------|
| Frequency | 863.15–869.75 MHz (EU868) |
| Channels | 23 (300 kHz spacing) |
| Modulation | LoRa SF7 / BW250 / CR 4/5 |
| TX Power | 1–22 dBm (default 14) |
| Audio Codec | Codec2 3200 bps |
| Audio Latency | ~160 ms |
| Packet Size | 71 bytes (voice) |

## Website

- **https://meshtrx.com** | **https://meshtrx.ru**
- Telegram: **https://t.me/MeshTRX**

## Documentation

- **[User Guide](docs/USER_GUIDE.md)** — full feature documentation, settings reference, usage scenarios
- **[Technical Spec](MESHTRX_SPEC.md)** — packet formats, BLE protocol, radio parameters
- **[Project Log](PROJECT_LOG.md)** — development history and changelog

## Dependencies

### Firmware (PlatformIO)
- [RadioLib](https://github.com/jgromes/RadioLib) 6.4+ — SX1262 LoRa driver
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) 1.4+ — BLE stack
- [U8g2](https://github.com/olikraus/u8g2) 2.35+ — OLED display
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) 7.0+ — JSON parsing
- [Codec2](https://github.com/drowe67/codec2) 1.2.0 — Voice codec (included)

### Android
- Target SDK 34, Min SDK 21
- [osmdroid](https://github.com/osmdroid/osmdroid) — OpenStreetMap
- Codec2 via JNI (native C library included)

## License

This project is licensed under the **Creative Commons Attribution-NonCommercial 4.0 International License (CC BY-NC 4.0)**.

You are free to:
- **Share** — copy and redistribute the material in any medium or format
- **Adapt** — remix, transform, and build upon the material

Under the following terms:
- **Attribution** — You must give appropriate credit and indicate if changes were made
- **NonCommercial** — You may not use the material for commercial purposes

Full license text: https://creativecommons.org/licenses/by-nc/4.0/

## Contributing

Contributions are welcome! Please open an issue or pull request.

## Author

**Stanislav Butkovsky** — [GitHub](https://github.com/StanislavButkovsky)
