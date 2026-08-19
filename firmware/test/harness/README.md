# Тестовый стенд MeshTRX (пара устройств)

Управляет двумя платами через UART, гоняет протоколы и проверяет результат.
Телефон не нужен.

## Требования

- Обе платы прошиты dev-сборкой: `pio run -e heltec_v3_dev --target upload --upload-port /dev/ttyUSBx`
- `pyserial` (лежит в `vendor/`, ставить ничего не нужно)

## Запуск

```bash
# все тесты
PYTHONPATH=firmware/test/harness/vendor python3 firmware/test/harness/run_tests.py

# отдельные блоки
... run_tests.py --only text,voice
... run_tests.py --only files
... run_tests.py --only nack,load     # потери канала и конкурентная нагрузка
... run_tests.py --ports /dev/ttyUSB0,/dev/ttyUSB1 --logdir /tmp/meshtrx-logs
```

Логи обеих плат с метками времени пишутся в `--logdir` — по ним разбираются падения.

## Команды консоли (dev-сборка)

| Команда | Назначение |
|---------|-----------|
| `PING` / `INFO` | отклик, полное состояние (канал, мощность, heap, uptime, boot count) |
| `TESTMODE ON\|OFF` | режим стенда: TX −9 дБм, постоянный приём без телефона |
| `CH <n>` / `PWR <dBm>` | канал (0-22), мощность (−9…22) |
| `TX TEXT <BCAST\|hex4> <текст>` | текстовое сообщение |
| `TX AUDIO <count> [gap_ms]` | синтетический голосовой поток |
| `TX PTT ON\|OFF` | удержание PTT |
| `TX FILE <photo\|text\|bin\|voice\|pttvoice> <байт> <hex4>` | файл с проверяемым паттерном |
| `TX BEACON` | немедленный beacon |
| `TX CALL <all\|priv <id8>\|group\|sos>` | вызовы |
| `CALL <accept\|reject\|cancel> [seq]` | ответ на входящий вызов |
| `RX STATS` / `RX RESET` | счётчики приёма, потери по seq, RSSI/SNR |
| `LOSS <pct> [ALL]` | эмуляция потерь канала: отбрасывать долю чанков (или всех пакетов) |
| `LOAD START <text\|audio\|beacon\|mixed> <ms> [dest]` | фоновый трафик |
| `LOAD STOP` / `LOAD STATS` | остановить нагрузку / счётчики отправленного |
| `RADIO` | состояние чипа: IRQ, режим, rx_armed, уровень DIO1 |
| `RADIO TIME [RESET]` | миллисекунды в standby/RX/TX/duty — основа оценки тока |
| `DUTY ON\|OFF` | принудительный duty cycle RX, чтобы проверить режим отдельно |
| `LORA <rx\|duty\|standby>` | режим радио |
| `BLE [ADV ON\|OFF]` | состояние BLE / реклама |
| `BLE STATS` | подключения, разрывы, notify_ok/fail/retry/noconn |
| `REBOOT` | перезагрузка |

Ответы — одной строкой `EVT <NAME> key=value ...`, поэтому легко парсятся.

## Отдельные инструменты

| Скрипт | Что делает |
|--------|------------|
| `run_tests.py` | основной набор: текст, голос, файлы, маяки, вызовы, каналы, NACK, нагрузка, граничные случаи |
| `test_ble.py` | путь «телефон ↔ устройство» целиком: подключение, PIN, циклы переподключения, трафик из эфира до телефона |
| `fuzz.py` | мусор и вредные пакеты в эфир — приёмник не должен падать и течь |
| `power.py` | оценка среднего тока и ресурса батареи по фактическому времени в режимах радио |
| `soak.py` | длительный прогон (часы) с CSV: куча, перезагрузки, доли режимов |
| `battery.py` | реальный разряд аккумулятора по BLE (без USB): напряжение, средний ток, остаток |
| `probe_*.py` | точечные пробы для разбора конкретного дефекта |

```bash
PYTHONPATH=firmware/test/harness/vendor python3 firmware/test/harness/power.py --window 20
PYTHONPATH=firmware/test/harness/vendor python3 firmware/test/harness/soak.py --minutes 60 --interval 60
PYTHONPATH=firmware/test/harness/vendor python3 firmware/test/harness/test_ble.py --cycles 20
```

Адреса устройств тесты берут из самих устройств (`INFO id=…`) — порядок портов
можно менять, ничего править не нужно.
