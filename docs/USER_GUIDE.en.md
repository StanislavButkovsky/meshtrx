# MeshTRX — User Guide

> App 4.4.5 | Firmware 4.4.4 | Updated: 2026-08-31

---

## Contents

1. [Overview](#overview)
2. [Hardware](#hardware)
3. [First connection](#first-connection)
3.1. [Desktop client](#desktop-client-windows-linux-macos)
4. [Voice (PTT)](#voice-ptt)
5. [Listening modes](#listening-modes)
6. [Calls](#calls)
7. [Text messages](#text-messages)
8. [File transfer](#file-transfer)
9. [Map and radar](#map-and-radar)
10. [Settings](#settings)
11. [Repeater mode](#repeater-mode)
12. [The button on the device](#the-button-on-the-device)
13. [Indicators](#indicators)
14. [Specifications](#specifications)

---

## Overview

**MeshTRX** is a decentralised voice mesh network over LoRa and BLE. Two or more Heltec WiFi LoRa 32 devices talk to each other over LoRa at ranges of 5 km and beyond. Each device connects over Bluetooth LE either to an Android phone or to a computer running the desktop client.

```
[Phone A] <--BLE--> [Heltec A] <--LoRa 868 MHz--> [Heltec B] <--BLE--> [Computer B]
```

### What you can do

- Voice in PTT or VOX mode
- Text messages (up to 84 characters)
- Photos and files (up to 100 KB)
- Stations on a map and on a tactical radar
- Broadcast, private and group calls
- Repeater mode with WiFi monitoring
- 23 channels in the 863–870 MHz band
- Use it from a phone (Android) or a computer (Windows, Linux, macOS)

---

## Hardware

### Device: Heltec WiFi LoRa 32 (V3 or V4)

| Parameter | Value |
|-----------|-------|
| MCU | ESP32-S3 (WiFi + BLE 5.0) |
| LoRa | Semtech SX1262 |
| Display | OLED 128x64 (I2C) |
| Band | 863–870 MHz (EU868) |
| Power | USB-C or LiPo battery |
| TX power | 1–22 dBm (configurable) |

Both board revisions are supported. Their firmware is **different** — take the file whose name matches yours:

| Board | What is inside | Firmware file |
|-------|----------------|---------------|
| V3 | SX1262, no external amplifier | `firmware-v3-<version>.bin` |
| V4 rev 4.2 | GC1109 amplifier | `firmware-v4-<version>.bin` |
| V4 rev 4.3 | KCT8103L amplifier | `firmware-v4.3-<version>.bin` |

The revision is printed on the board itself in small type next to the antenna connector. Firmware for the wrong revision will boot, but the amplifier will be driven from the wrong pin: the device will hear poorly or will not transmit at all.

### What you need

- 2 or more Heltec WiFi LoRa 32 devices (V3 or V4)
- for each device — an Android phone (5.0+) **or** a computer with the desktop client
- a USB-C cable for flashing
- an 868 MHz antenna, screwed on before power is applied: transmitting without an antenna destroys the output stage

---

## First connection

### 1. Flashing the device

The simplest way is to flash straight from the browser: open the [flashing page](/flash/) in Chrome or Edge, connect the device over USB and pick your board revision. Ready-made files are on the [download page](/download/) if you prefer to flash with your own tools (see the revision table above). From source:

```bash
cd firmware
pio run -e heltec_wifi_lora_32_V3  --target upload --upload-port /dev/ttyUSB0   # V3
pio run -e heltec_wifi_lora_32_V4  --target upload --upload-port /dev/ttyUSB0   # V4 rev 4.2
pio run -e heltec_wifi_lora_32_V43 --target upload --upload-port /dev/ttyUSB0   # V4 rev 4.3
```

After updating the firmware, **update the app as well**: the two agree on a shared protocol, and the pair "new firmware + old app" may fail to find each other over Bluetooth.

### 2. Installing the app

Download the APK from the [download page](/download/) and allow installation from unknown sources — Android will ask about this itself. Every release is signed with the same key, so an update installs over the previous one without uninstalling.

From source:

```bash
cd android/MeshTRX
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

### 3. Connecting over BLE

1. Open the MeshTRX app
2. Go to the **Settings** tab
3. Press **Connect** — scanning starts
4. The device appears as **MeshTRX-XXXX**
5. Press the button on the device (>1 s) — a **PIN** appears
6. Enter the PIN in the app
7. The status changes to **● Connected** (green)

**If the phone does not see the device in the Bluetooth list.** Check that both the phone and the device are on recent versions: before 4.4.3 the device went on air without a name, so the phone did not show it in the list. Update both sides — updating one will not help. If that does not help, power-cycle the device: it releases an abandoned connection on its own, but not instantly.

---

## Desktop client (Windows / Linux / macOS)

The computer gives you everything the phone does: voice, messages, calls, files, radar, map, settings and the repeater. Plus a diagnostics log that the phone does not have.

```bash
python3 -m venv .venv
.venv/bin/pip install -r desktop/requirements.txt
.venv/bin/python desktop/run.py
```

The client needs the system Codec2 library (`libcodec2` on Linux and macOS, `codec2.dll` next to the program on Windows). If it is missing, the client says so at startup instead of silently staying mute.

To transmit, hold the button with the mouse or press the space bar — with the same ten-second limit and the same countdown as on the phone. Details and a walk-through of the program are in [desktop/README.md](../desktop/README.md).

---

## Voice (PTT)

### Codec

**Codec2 at 3200 bps** — intelligible speech at minimal use of the radio channel.

| Parameter | Value |
|-----------|-------|
| Bitrate | 3200 bps |
| Frame | 20 ms / 8 bytes |
| Packet | 8 frames = 64 bytes = 160 ms |
| Latency | ~160 ms |
| LoRa airtime | ~20 ms |

### How long you may talk: the ten-second limit

A single transmission takes the whole channel. LoRa is half-duplex: while somebody is talking, the others can neither answer nor call for help — they simply are not heard. So speech is limited to **ten seconds** in every mode:

| Where | What happens at the tenth second |
|-------|----------------------------------|
| Holding the button (PTT) | transmission stops by itself, the button shows a countdown; to continue, release it and press again |
| Voice activation (VOX) | the same, with the same countdown; to continue, pause — transmission restarts with the new phrase |
| Voice message in chat | recording stops and the message is sent as it is |
| Addressed voice | the same: up to 10 seconds are recorded and sent as a file |

The limit is duplicated inside the device itself, not just in the app: if the phone freezes or the button sticks, the radio still falls silent at the tenth second and frees the channel. `LIMIT 10s` appears on the device screen.

To talk longer, simply press again. The pause between transmissions is useful in itself: it gives the other side time to answer and lets the network carry other people's messages and calls.

### PTT mode (push-to-talk)

1. Make sure the **PTT/VOX** switch is set to **PTT**
2. Press and **hold** the large round button
3. Speak — the status shows **● transmitting… N s left**
4. Release the button and transmission stops
5. The other side hears a short two-tone end-of-transmission signal (it can be turned off in Settings, the "Audio" section)

If you do not release it, transmission ends by itself at the tenth second.

### VOX mode (voice activation)

1. Set the **PTT/VOX** switch to **VOX**
2. Transmission starts automatically when speech is detected
3. States: `...` (attack) → `>>> TX <<<` (transmitting) → `TX (pause)` (hangtime)

**VOX settings** (Settings tab):
- **VOX threshold** (0–5000) — sensitivity; the lower the value, the more sensitive
- **VOX delay** (200–2000 ms) — the pause before transmission ends

### Loudspeaker

The speaker button (top right of the PTT button):
- **Green** — speaker on (default)
- **Grey** — speaker off (sound through the earpiece)

### Noise gate (PTT RMS)

The **PTT RMS** slider in the settings (0–1000):
- **0** — off (everything is transmitted, default)
- **50–300** — light filtering of background noise
- **300+** — aggressive filtering (loud speech only)

### Receive volume

The **Receive volume** slider in the settings (50%–300%, 200% by default).

---

## Listening modes

Two buttons at the top of the PTT screen:

| Mode | Description |
|------|-------------|
| **All** | You hear every transmission on the channel |
| **Mine** | You hear only calls addressed to you |

The active mode is highlighted in green.

---

## Calls

### Call types

| Type | Button | Description |
|------|--------|-------------|
| **Broadcast** | BROADCAST (blue) | A call to everyone on the channel |
| **Private** | CALL (green) | A call to one particular station |
| **Group** | via the picker | A call to a group (up to 8 participants) |

### How to call

1. **Broadcast call**: press **BROADCAST** — everyone on the channel is notified
2. **Private call**: press **CALL** → pick a station from the list → the call is sent
3. Incoming call: an overlay appears with **ACCEPT** / **DECLINE**

### Recent calls

At the bottom of the PTT screen there is a scrollable list of recent calls:
- Shows unique entries (duplicates replace each other)
- Direction: → outgoing, ← incoming
- Colour by type: blue (broadcast), green (private), yellow (group)
- Tap to call again

---

## Text messages

### Sending

1. Go to the **Chat** tab
2. Type your text (up to 84 characters)
3. Press the send button (the green arrow)
4. By default this is a broadcast to the channel

### Addressed messages

1. Press the **@** button next to the input field
2. Pick a recipient from the list
3. **To: [name]** appears above the input field
4. Only the addressee receives the message

### Filtering

Use the filter drop-down to show messages from one particular station or from everyone.

---

## File transfer

### Sending a photo

1. Go to the **Files** tab
2. Press **Photo** — the gallery opens
3. Pick a photo (it is compressed to 100 KB automatically)
4. Confirm sending
5. Progress is shown in the list

### Sending files

1. Press **File** — the file manager opens
2. Pick a file (100 KB max)
3. The transfer starts automatically

### Transfer parameters

| Parameter | Value |
|-----------|-------|
| Max size | 100 KB |
| Chunk size | 120 bytes |
| Interval | 100 ms |
| Throughput | ~1.2 KB/s |
| An 11 KB photo | ~10 seconds |

### What you can do with a file

Tap a file in the list:
- **Share** — send it to another app
- **Retry** — send it again
- **Delete** — remove it from the history

---

## Map and radar

### Map (OpenStreetMap)

- Shows your position and every station found
- Markers with call signs and signal level
- Dashed lines to the stations (colour by age)
- Buttons:
  - **Me** — centre on your position
  - **All** — show every station

### Radar (tactical)

- Black background with green range rings
- Compass orientation (true bearing)
- Stations are drawn relative to your position
- Zoom buttons +/− (100 m to 100 km)
- A contrast button (normal / bright for outdoors)
- Auto-zoom to the furthest station (5 km by default)
- Brightness by RSSI
- Call sign on the dot

### GPS

Android LocationManager is used (works without Google services). Updates every 15 seconds. GPS status is shown on the map screen.

---

## Settings

### Connection
- **Connect / Disconnect** — control the BLE connection
- **New device** — forget the current one and find another

### Call sign
- Up to 8 characters, shown in the header and in the beacon

### Radio
- **Channel** (0–22) — the working frequency (863.15–869.75 MHz)
- **TX power** (1–22 dBm) — transmission range
- **Duty cycle EU868** — the 1% limit for EU compliance

### Audio
- **Receive volume** (50–300%)
- **PTT RMS** (0–1000) — noise gate
- **End-of-transmission tone** — the short signal that ends someone else's speech. It can be turned off if it gets in the way; the setting is remembered (since version 4.4.5)
- **VOX threshold** (0–5000)
- **VOX delay** (200–2000 ms)

The timbre of the voice itself cannot be adjusted: it goes on air through Codec2 at a constant 3200 bps, and there is no separate "modulation" control. Intelligibility is changed with the receive volume and the PTT RMS threshold: the higher the threshold, the more quiet sound is cut off before transmission.

Since version 4.4.5 every setting in this section is remembered and survives an app restart. In earlier versions they were reset to factory values on every close — if a VOX threshold you had tuned "went back to 800 by itself", this was why.

### Beacon
- Beacon interval (Never / 1–60 min / 1 hour)

### Station list
- Timeout for removing inactive stations (15 min to 24 hours)

### File history
- Retention (7 / 14 / 30 / 90 days / Unlimited)

### Repeater
- See [Repeater mode](#repeater-mode)

### Language
- Russian / English — switches instantly

### Apply and save
The button sends the radio settings to the device and stores them in NVS.

---

## Repeater mode

A device can work as a standalone repeater — it receives LoRa packets and forwards them, decrementing the TTL.

### What gets forwarded

| Packet type | Forwarded |
|-------------|-----------|
| Voice (0xA0) | Yes |
| Text (0xB0) | Yes |
| Files (0xC0, 0xC1, 0xC3) | Yes |
| Beacon (0xD0) | Yes |
| Calls (0xE0–0xE6) | Yes |
| File ACK (0xC2) | No |

### Turning it on

1. Go to **Settings** → the **Repeater** section
2. Optionally enter a **WiFi SSID** and password to join a network
3. Optionally enter a **static IP**
4. Press **ENABLE REPEATER**
5. Confirm in the dialog
6. The device reboots into repeater mode

### WiFi monitoring

Once enabled, the device brings up WiFi:
- **Without an SSID**: it creates the access point `MeshTRX-Repeater` (password: `meshtrx123`)
- **With an SSID**: it joins that network (falling back to the access point on failure)

The web interface is available at:
- AP mode: `http://192.168.4.1`
- STA mode: at the address you set or received

### Web interface

The page refreshes every 5 seconds and shows:
- **Uptime** — how long it has been running
- **Channel** — the current one, with a drop-down to change it
- **TX power**
- **Forwarded / Dropped** — packet counters
- **By type**: audio, text, file, beacon
- **RSSI range** — minimum and maximum signal level
- **IP address**

### Changing the channel

The channel can be changed:
- from the **web interface** (drop-down plus the Set button)
- from the **app** (connect over BLE and change it in the settings)

### Turning it off

- From the **app**: Settings → **Disable repeater** (BLE stays available in repeater mode)
- The device reboots into normal mode

### Deduplication

- A cache of 64 entries, a 30-second window
- The TTL is decremented on every retransmission
- Packets with TTL=0 are not forwarded
- A random delay of 10–50 ms avoids collisions

---

## The button on the device

### In normal mode

| Press | Action |
|-------|--------|
| Short (<1 s) | Turn the screen on for 30 s |
| Medium (>1 s) | Show the PIN and the device name for 10 s |
| Long (>3 s) | Turn the device off |

### In repeater mode

| Press | Action |
|-------|--------|
| Short | Turn the screen on |
| Medium (>1 s) | Reset the statistics |
| Long (>3 s) | Leave repeater mode |
| Very long (>8 s) | Turn the device off |

### Turning the device off and on

There is no power switch on the board, so the button plays that role. Hold it for three seconds: **SLEEP** appears on the screen in large type and the device goes to sleep — radio, amplifier and display lose power, consumption drops to microamps, and a battery lasts for months of standby.

The same button turns it back on; a short press is enough. Waking from this sleep is a full start, exactly as after applying power, so the link and the settings come back on their own. In the log it shows as `reason=DEEPSLEEP wake=BUTTON`.

In repeater mode the threshold is longer — eight seconds: there the shorter holds are already taken by resetting the statistics and leaving the mode. While you hold the button a countdown runs on the screen, so you are not holding it blind.

---

## Indicators

### OLED display

**Normal mode**: channel, frequency, RSSI, SNR, TX power, BLE status, battery voltage (two decimals), VOX status.

**Repeater mode**: `** REPEATER **`, channel, frequency, FWD/DRP counters, last RSSI/SNR, TTL.

The screen turns itself off after 30 seconds.

### LED (GPIO35)

| Pattern | Meaning |
|---------|---------|
| Solid | Transmitting (TX) |
| Blinking ~300 ms | BLE waiting for a connection |
| Short flash every 5 s | BLE connected |
| Fast blinking | File transfer |
| Short pulse | Packet received (RX) |

### App header

| Element | Position | Description |
|---------|----------|-------------|
| Call sign | Top left | Large white text |
| Device name | Bottom left | Small grey text |
| Status | Top right | ● Connected (green) / Disconnected (grey) |
| Channel + frequency | Bottom right | Green text, "CH 5 · 864.65 MHz" |

### Bottom navigation

Five tabs: **PTT**, **Chat**, **Files**, **Map**, **Settings**. The active tab is highlighted in green.

---

## Specifications

### LoRa radio

| Parameter | Value |
|-----------|-------|
| Band | 863.15–869.75 MHz |
| Channels | 23 (300 kHz apart) |
| Bandwidth | 250 kHz |
| Spreading factor | 7 |
| Coding rate | 4/5 |
| Sync word | 0x34 |
| Power | 1–22 dBm |
| Range | up to 5+ km (line of sight) |

### Audio codec

| Parameter | Value |
|-----------|-------|
| Codec | Codec2 3200 bps |
| Sample rate | 8000 Hz |
| Frame | 160 samples = 20 ms = 8 bytes |
| Packet | 8 frames = 64 bytes = 160 ms |
| LoRa packet | 71 bytes (7 header + 64 audio) |

### BLE protocol

| Parameter | Value |
|-----------|-------|
| Service | Nordic UART Service (NUS) |
| MTU | 128 bytes |
| Commands | 40+ (0x01–0x28) |
| Audio packet | 68 bytes (cmd + flags + 64 payload) |
| Authorisation | a 4-digit PIN (derived from the MAC) |

### LoRa packets

| Type | ID | Size |
|------|-----|------|
| Voice | 0xA0 | 71 bytes |
| Text | 0xB0 | up to 91 bytes |
| File (header) | 0xC0 | 36 bytes |
| File (chunk) | 0xC1 | up to 128 bytes |
| File (end) | 0xC3 | 6 bytes |
| Beacon | 0xD0 | 36 bytes |
| Calls | 0xE0–0xE6 | 8–47 bytes |

### Battery

| Parameter | Value |
|-----------|-------|
| ADC | GPIO1 through a divider |
| Control | GPIO37 enable |
| Calibration | multiplier 5.55 |
| Averaging | 8 samples |
| Range | 3.0 V (0%) – 4.2 V (100%) |
