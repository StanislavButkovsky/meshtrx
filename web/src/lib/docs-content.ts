export const USER_GUIDE = `# MeshTRX — Руководство пользователя

## Введение

MeshTRX — система off-grid голосовой связи на базе LoRa mesh-сети. Позволяет общаться голосом, обмениваться сообщениями и передавать файлы без интернета и сотовых сетей.

## Требования

### Оборудование
- **Heltec WiFi LoRa 32 V3** — модуль связи (1 на каждого участника)
- **Android-устройство** — смартфон/планшет с Android 8.0+
- **USB-C кабель** — для первоначальной прошивки модуля

### Антенна
- Штатная антенна 868 МГц (идёт в комплекте)
- Для увеличения дальности рекомендуется внешняя антенна с SMA-разъёмом

## Быстрый старт

### 1. Прошивка модуля

1. Подключите Heltec V3 к компьютеру по USB
2. Откройте [страницу прошивки](/flash/) в Chrome или Edge
3. Нажмите "Выбрать порт" и выберите COM-порт устройства
4. Нажмите "Прошить" и дождитесь завершения

### 2. Установка приложения

1. Скачайте APK со [страницы загрузки](/download/)
2. Разрешите установку из неизвестных источников
3. Установите и откройте MeshTRX

### 3. Подключение

1. Включите Bluetooth на телефоне
2. Включите модуль Heltec (питание по USB или батарея)
3. В приложении нажмите "Подключить" — выберите устройство MeshTRX

## Голосовая связь

### PTT (Push-to-Talk)
- Зажмите кнопку PTT и говорите
- Отпустите для приёма
- Голос кодируется Codec2 (3200 bps) и передаётся через LoRa

### Групповые вызовы
- Голос транслируется на весь канал
- Все участники канала слышат передачу

### Адресный вызов
- Выберите конкретный узел из списка
- Только выбранный узел получит вызов

## Сообщения

### Отправка
- Введите текст и нажмите "Отправить"
- Сообщение доставляется через mesh-сеть
- Подтверждение доставки (✓✓)

### Доставка через ретрансляторы
- Сообщения автоматически маршрутизируются через промежуточные узлы
- TTL (время жизни) — 3 хопа по умолчанию

## Передача файлов

- Выберите файл для отправки
- Файл разбивается на фрагменты и передаётся через mesh
- Прогресс-бар показывает состояние передачи
- Рекомендуемый размер файлов: до 100 КБ

## Карта и радар

### GPS-карта
- Позиции всех узлов отображаются на карте
- Обновление координат каждые 30 секунд
- Требуется разрешение на геолокацию

### Радар
- Отображает ближайшие узлы относительно вашей позиции
- Показывает расстояние и направление

## Настройки

### Каналы LoRa
- 23 доступных канала (868 МГц диапазон)
- Все участники должны быть на одном канале
- Смена канала: Настройки → Канал LoRa

### Мощность передатчика
- Доступные значения: 2, 5, 10, 14, 17, 20, 22 dBm
- Больше мощность = больше дальность, но выше энергопотребление

### Режим ретранслятора
- Включается в настройках модуля
- Узел будет пересылать пакеты других участников
- Увеличивает покрытие сети

## Устранение неполадок

### Не подключается по BLE
- Убедитесь, что Bluetooth включён
- Перезагрузите модуль
- Удалите сопряжение и подключитесь заново

### Нет голосовой связи
- Проверьте, что оба устройства на одном канале
- Проверьте антенну модуля
- Уменьшите расстояние для теста

### Прошивка не начинается
- Используйте Chrome или Edge
- Проверьте USB-кабель (должен поддерживать данные)
- Зажмите кнопку BOOT при подключении

## Технические характеристики

| Параметр | Значение |
|----------|----------|
| Частота | 868 МГц |
| Модуляция | LoRa (SF7-SF12) |
| Мощность | до 22 dBm |
| Дальность | 5+ км (прямая видимость) |
| Кодек | Codec2 3200 bps |
| Задержка | ~160 мс |
| BLE | 5.0 |
| Чип | ESP32-S3 + SX1262 |
`;

export const USER_GUIDE_EN = `# MeshTRX — User Guide

## Introduction

MeshTRX is an off-grid voice communication system based on a LoRa mesh network. It enables voice calls, text messaging and file transfer without internet or cellular networks.

## Requirements

### Hardware
- **Heltec WiFi LoRa 32 V3** — communication module (1 per participant)
- **Android device** — smartphone/tablet with Android 8.0+
- **USB-C cable** — for initial firmware flashing

### Antenna
- Stock 868 MHz antenna (included)
- External antenna with SMA connector recommended for extended range

## Quick Start

### 1. Flash the Module

1. Connect Heltec V3 to your computer via USB
2. Open the [flash page](/flash/) in Chrome or Edge
3. Click "Select port" and choose the device COM port
4. Click "Flash" and wait for completion

### 2. Install the App

1. Download APK from the [download page](/download/)
2. Allow installation from unknown sources
3. Install and open MeshTRX

### 3. Connect

1. Enable Bluetooth on your phone
2. Power on the Heltec module (USB or battery)
3. In the app, tap "Connect" and select the MeshTRX device

## Voice Communication

### PTT (Push-to-Talk)
- Press and hold the PTT button to talk
- Release to receive
- Voice is encoded with Codec2 (3200 bps) and transmitted over LoRa

### Group Calls
- Voice is broadcast to the entire channel
- All channel participants hear the transmission

### Direct Call
- Select a specific node from the list
- Only the selected node receives the call

## Messages

### Sending
- Type your message and tap "Send"
- Message is delivered through the mesh network
- Delivery confirmation (✓✓)

### Relay Delivery
- Messages are automatically routed through intermediate nodes
- TTL (time to live) — 3 hops by default

## File Transfer

- Select a file to send
- File is split into fragments and transmitted over mesh
- Progress bar shows transfer status
- Recommended file size: up to 100 KB

## Map & Radar

### GPS Map
- Positions of all nodes displayed on the map
- Coordinates update every 30 seconds
- Location permission required

### Radar
- Shows nearby nodes relative to your position
- Displays distance and direction

## Settings

### LoRa Channels
- 23 available channels (868 MHz band)
- All participants must be on the same channel
- Change channel: Settings → LoRa Channel

### Transmit Power
- Available values: 2, 5, 10, 14, 17, 20, 22 dBm
- Higher power = longer range, but more power consumption

### Repeater Mode
- Enabled in module settings
- Node will relay packets from other participants
- Extends network coverage

## Troubleshooting

### Cannot Connect via BLE
- Make sure Bluetooth is enabled
- Restart the module
- Remove pairing and reconnect

### No Voice Communication
- Check that both devices are on the same channel
- Check the module antenna
- Reduce distance for testing

### Flashing Won't Start
- Use Chrome or Edge
- Check USB cable (must support data)
- Hold BOOT button while connecting

## Specifications

| Parameter | Value |
|-----------|-------|
| Frequency | 868 MHz |
| Modulation | LoRa (SF7-SF12) |
| Power | up to 22 dBm |
| Range | 5+ km (line of sight) |
| Codec | Codec2 3200 bps |
| Latency | ~160 ms |
| BLE | 5.0 |
| Chip | ESP32-S3 + SX1262 |
`;
