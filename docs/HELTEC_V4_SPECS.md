# Heltec WiFi LoRa 32 V4 — Спецификация

> Модель: HTIT-WB32LAF (ревизия V4.3.1)
> Источник: https://heltec.org/project/wifi-lora-32-v4/

---

## 1. MCU

- **ESP32-S3R2** (Xtensa 32-bit dual-core LX7)
- CPU: 240 МГц
- ROM: 384 КБ
- SRAM: 512 КБ
- RTC SRAM: 16 КБ
- PSRAM: 2 МБ (QSPI)
- Flash: **16 МБ** external (QIO, 80 МГц) — V3 имеет 8 МБ
- USB VID/PID: `0x303a` / `0x1001`
- Deep sleep: **< 20 мкА**

---

## 2. LoRa

- **Semtech SX1262**
- Чувствительность RX: **-137 dBm** (SF12, BW=125 кГц)
- Частоты: 433, 470-510, 863-928 МГц
- Без PA: **21 ± 1 dBm**
- С PA (GC1109): **28 ± 1 dBm**

---

## 3. PA (Power Amplifier) — опционально

Выбирается при компиляции. Три варианта:

| Вариант | Define | Max TX |
|---------|--------|--------|
| Без PA (default) | `USE_NONE_PA` | 21 dBm |
| **GC1109** | `USE_GC1109_PA` | 28 dBm |
| KCT8103L | `USE_KCT8103L_PA` | 28 dBm |

### GC1109 пины

| Функция | GPIO | Описание |
|---------|------|----------|
| `LORA_PA_POWER` | **7** | Питание PA (HIGH = вкл) |
| `LORA_PA_EN` (CSD) | **2** | Enable |
| `LORA_PA_TX_EN` (CTX) | **46** | TX/RX переключатель (HIGH = TX) |

### KCT8103L пины

| Функция | GPIO |
|---------|------|
| `LORA_PA_POWER` | **7** |
| `LORA_PA_CSD` | **2** |
| `LORA_PA_CTX` | **5** |

Даташит GC1109: `https://resource.heltec.cn/download/WiFi_LoRa_32_V4/datasheet/GC1109_EN_V0.9.2.pdf`

### RX LNA

Опция в меню платы — использовать PA как LNA на приёме.

---

## 4. Распиновка

### LoRa SPI

| Функция | GPIO |
|---------|------|
| SCK | **9** |
| MOSI | **10** |
| MISO | **11** |
| NSS (CS) | **8** |
| RST | **12** |
| DIO1 | **14** |
| BUSY | **13** |

### OLED I2C

| Функция | GPIO |
|---------|------|
| SDA | **17** |
| SCL | **18** |
| RST | **21** |

Дисплей: 0.96" OLED 128×64, **SSD1315** (совместим с SSD1306)

### Управление

| Функция | GPIO | Примечание |
|---------|------|-----------|
| **Vext** | **36** | Внешнее питание (OLED, сенсоры). LOW = вкл |
| **LED** | **35** | Встроенный белый LED |
| **PRG Button** | **0** | Кнопка boot/user |

### I2C (внешние сенсоры)

| Функция | GPIO |
|---------|------|
| SDA | **3** |
| SCL | **4** |

### UART

| Функция | GPIO |
|---------|------|
| TX | **43** |
| RX | **44** |

### Battery ADC

- GPIO: **1** (ADC1_CH0)
- Делитель: 2×470 кОм
- Диапазон: 3.3V - 4.4V (3200-4150 мВ)

---

## 5. USB

- **USB Type-C** с native ESP32-S3 USB
- **CP2102 убран** (ключевое отличие от V3!)
- CDC on boot: enabled
- 1200-baud touch reset: **не нужен**
- USB JTAG debugging доступен

### PlatformIO upload

```ini
[env:heltec_wifi_lora_32_V4]
upload_protocol = esptool
upload_speed = 921600
# Без 1200-baud reset
```

---

## 6. Питание

- Батарея: **SH1.25-2P** (JST 1.25мм, 2-pin)
- Вход: 3.3V - 4.4V (одна ячейка LiPo)
- **Solar вход: SH1.25-2P**, 4.7V - 6V — нет на V3!
- Deep sleep: **< 20 мкА**

---

## 7. Беспроводная связь

- **WiFi**: 802.11 b/g/n, до 150 Мбит/с
- **Bluetooth**: 5.0 BLE + Mesh
- **2.4 ГГц антенна**: IPEX 1.0 + встроенная FPC (V3 только IPEX)

---

## 8. Разъёмы

| Разъём | Тип | Назначение |
|--------|-----|-----------|
| LoRa антенна | IPEX 1.0 (U.FL) | 868 МГц |
| WiFi/BLE антенна | IPEX 1.0 + FPC | 2.4 ГГц |
| Батарея | SH1.25-2P | LiPo 3.7V |
| Solar | SH1.25-2P | 4.7-6V вход |
| GNSS | SH1.25-8P | GPS/GLONASS — нет на V3 |
| USB | Type-C | Питание + программирование |

---

## 9. Размеры

- **51.7 × 25.4 × 10.7 мм**
- Вес: 35 г
- Рабочая температура: -20 ... +70 °C

---

## 10. Отличия V4 от V3

| Параметр | V3 | V4 |
|----------|----|----|
| USB | **CP2102** (UART bridge) | **Native ESP32-S3 USB** |
| Flash | 8 МБ integrated | **16 МБ external** |
| PA | Нет | **GC1109 / KCT8103L** |
| Max TX power | 21 dBm | **28 dBm** (с PA) |
| Solar вход | Нет | **SH1.25-2P, 4.7-6V** |
| GNSS разъём | Нет | **SH1.25-8P** |
| OLED driver | SSD1306 | **SSD1315** (совместим) |
| Кол-во пинов | 36 | **40** |
| 2.4G антенна | IPEX only | **IPEX + FPC** |
| Pin plating | Silver | **Gold** |

### Одинаковые пины V3 и V4

- LoRa SPI: SCK=9, MOSI=10, MISO=11, NSS=8, RST=12, DIO1=14, BUSY=13
- OLED: SDA=17, SCL=18, RST=21
- Vext: GPIO36
- LED: GPIO35
- PRG: GPIO0

---

## 11. Документация Heltec

| Ресурс | URL |
|--------|-----|
| Страница продукта | https://heltec.org/project/wifi-lora-32-v4/ |
| Схема V4.3 | https://resource.heltec.cn/download/WiFi_LoRa_32_V4/Schematic/HTIT-WB32LAF_V4.3.pdf |
| Pinmap | https://resource.heltec.cn/download/WiFi_LoRa_32_V4/Pinmap/V4_pinmap.png |
| Datasheet V4.2 | https://resource.heltec.cn/download/WiFi_LoRa_32_V4/datasheet/WiFi_LoRa_32_V4.2.0.pdf |
| ESP32-S3 datasheet | https://resource.heltec.cn/download/WiFi_LoRa_32_V4/datasheet/esp32-s3_datasheet_en.pdf |
| GC1109 PA datasheet | https://resource.heltec.cn/download/WiFi_LoRa_32_V4/datasheet/GC1109_EN_V0.9.2.pdf |
| SSD1315 OLED datasheet | https://resource.heltec.cn/download/WiFi_LoRa_32_V4/datasheet/SSD1315_1.0_withcomand_.pdf |
| SX1261/1262 datasheet | https://resource.heltec.cn/download/WiFi_LoRa_32_V4/datasheet/SX1261_2%20V2-2.pdf |

---

## 12. Заметки для MeshTRX firmware

### Совместимость с текущей прошивкой

Текущие pin definitions в `lora_radio.h` и `oled_display.h` **совпадают** с V4. Прошивка уже поддерживает V4 (env `heltec_wifi_lora_32_V4` в platformio.ini).

### PA управление (V4)

Для правильной работы GC1109 на V4 нужно управлять тремя GPIO:
```cpp
// TX режим
digitalWrite(7, HIGH);   // PA power ON
digitalWrite(2, HIGH);   // PA enable
digitalWrite(46, HIGH);  // TX mode

// RX режим
digitalWrite(46, LOW);   // RX mode

// Sleep / power save
digitalWrite(7, LOW);    // PA power OFF (экономия ~5-10 мА)
```

**Важно**: `LORA_PA_POWER` (GPIO7) нужно явно выставлять HIGH для подачи питания на PA. При энергосбережении — выставить LOW для полного отключения PA.

### Upload V4

V4 не использует CP2102, поэтому:
- Порт: `/dev/ttyACM0` (вместо `/dev/ttyUSB0` на V3)
- Не нужен 1200-baud reset для входа в bootloader
- Скорость: 921600 baud
