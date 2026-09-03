// СГЕНЕРИРОВАНО автоматически из docs/USER_GUIDE.md и docs/USER_GUIDE.en.md.
// Не правьте этот файл: правки затрёт следующая сборка. Правьте руководство
// в docs/ — скрипт web/scripts/sync-docs.mjs перенесёт их сюда.

export const USER_GUIDE = `# MeshTRX — Руководство пользователя

> Приложение 4.4.5 | Прошивка 4.4.4 | Обновлено: 2026-08-31

---

## Содержание

1. [Обзор](#обзор)
2. [Аппаратура](#аппаратура)
3. [Первое подключение](#первое-подключение)
3.1. [Настольный клиент](#настольный-клиент-windows-linux-macos)
4. [Голосовая связь (PTT)](#голосовая-связь-ptt)
5. [Режимы прослушивания](#режимы-прослушивания)
6. [Система вызовов](#система-вызовов)
7. [Текстовые сообщения](#текстовые-сообщения)
8. [Передача файлов](#передача-файлов)
9. [Карта и радар](#карта-и-радар)
10. [Настройки](#настройки)
11. [Режим ретранслятора](#режим-ретранслятора)
12. [Кнопка на устройстве](#кнопка-на-устройстве)
13. [Индикация](#индикация)
14. [Технические характеристики](#технические-характеристики)

---

## Обзор

**MeshTRX** — децентрализованная голосовая mesh-сеть на LoRa + BLE. Два (или более) устройства Heltec WiFi LoRa 32 связываются по LoRa на расстоянии до 5+ км. К каждому устройству подключается по Bluetooth BLE либо Android-смартфон, либо компьютер с настольным клиентом.

\`\`\`
[Телефон A] <--BLE--> [Heltec A] <--LoRa 868МГц--> [Heltec B] <--BLE--> [Компьютер B]
\`\`\`

### Что можно делать

- Голосовая связь в режиме PTT или VOX
- Текстовые сообщения (до 84 символов)
- Передача фото и файлов (до 100 КБ)
- Отображение станций на карте и тактическом радаре
- Общие, личные и групповые вызовы
- Режим ретранслятора с WiFi-мониторингом
- 23 канала в диапазоне 863–870 МГц
- Работа с телефона (Android) или с компьютера (Windows, Linux, macOS)

---

## Аппаратура

### Устройство: Heltec WiFi LoRa 32 (V3 или V4)

| Параметр | Значение |
|----------|----------|
| MCU | ESP32-S3 (WiFi + BLE 5.0) |
| LoRa | Semtech SX1262 |
| Дисплей | OLED 128x64 (I2C) |
| Диапазон | 863–870 МГц (EU868) |
| Питание | USB-C или LiPo аккумулятор |
| Мощность TX | 1–22 дБм (настраивается) |

Поддерживаются обе ревизии платы. Прошивка у них **разная** — берите файл,
в имени которого указана ваша:

| Плата | Что внутри | Файл прошивки |
|-------|-----------|---------------|
| V3 | SX1262 без внешнего усилителя | \`firmware-v3-<версия>.bin\` |
| V4 rev 4.2 | усилитель GC1109 | \`firmware-v4-<версия>.bin\` |
| V4 rev 4.3 | усилитель KCT8103L | \`firmware-v4.3-<версия>.bin\` |

Ревизия напечатана на самой плате мелким шрифтом рядом с разъёмом антенны.
Прошивка от чужой ревизии запустится, но усилитель будет управляться не тем
выводом: устройство станет слышать хуже или не выйдет в эфир вовсе.

### Что нужно

- 2+ устройства Heltec WiFi LoRa 32 (V3 или V4)
- на каждое устройство — Android-телефон (5.0+) **или** компьютер с
  настольным клиентом
- USB-C кабель для прошивки
- антенна на 868 МГц, прикрученная до подачи питания: передача без антенны
  выводит из строя выходной каскад

---

## Первое подключение

### 1. Прошивка устройства

Проще всего — прошить прямо из браузера: откройте [страницу прошивки](/flash/)
в Chrome или Edge, подключите устройство по USB и выберите свою ревизию платы.
Готовые файлы лежат на [странице загрузки](/download/), если хотите прошивать
своими средствами (см. таблицу ревизий выше). Из исходников:

\`\`\`bash
cd firmware
pio run -e heltec_wifi_lora_32_V3  --target upload --upload-port /dev/ttyUSB0   # V3
pio run -e heltec_wifi_lora_32_V4  --target upload --upload-port /dev/ttyUSB0   # V4 rev 4.2
pio run -e heltec_wifi_lora_32_V43 --target upload --upload-port /dev/ttyUSB0   # V4 rev 4.3
\`\`\`

После обновления прошивки **обновите и приложение**: они договариваются по
общему протоколу, и старая пара «новая прошивка + старое приложение» может
не найти друг друга по Bluetooth.

**Прошивка вручную, через esptool.** Понадобится, если прошиваете V4: мастер на
сайте заливает только образ V3. Одного файла прошивки мало — после полного
стирания на плате не остаётся ни загрузчика, ни таблицы разделов, и она уходит
в бесконечную перезагрузку. Нужны четыре файла, каждый по своему адресу:

\`\`\`bash
# esptool 5.x — команда и ключи через дефисы
esptool --chip esp32s3 --port COM3 --baud 921600 \\
  write-flash -z --flash-mode dio --flash-freq 80m --flash-size 16MB \\
  0x0     bootloader-v4.bin \\
  0x8000  partitions-v4.bin \\
  0xe000  boot_app0.bin \\
  0x10000 firmware-v4.3-<версия>.bin

# esptool 4.x — то же самое, но через подчёркивания
esptool.py --chip esp32s3 --port COM3 --baud 921600 \\
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB \\
  0x0     bootloader-v4.bin \\
  0x8000  partitions-v4.bin \\
  0xe000  boot_app0.bin \\
  0x10000 firmware-v4.3-<версия>.bin
\`\`\`

Версию покажет \`esptool version\`. В пятой версии команды и ключи переехали на
дефисы, а сам вызов стал просто \`esptool\`; со старым написанием она откажется
работать.

Два места, где ошибаются чаще всего:

- **Загрузчик у ESP32-S3 лежит по адресу \`0x0\`, а не \`0x1000\`.** Адрес \`0x1000\`
  — от старого ESP32; с ним плата включается и сразу перезагружается, по кругу.
- **У V4 флеш на 16 МБ**, поэтому \`--flash_size 16MB\`. У V3 — 8 МБ, и файлы
  загрузчика с таблицей разделов у него свои: \`bootloader.bin\` и
  \`partitions.bin\`.

Все четыре файла лежат на [странице загрузки](/download/), в блоке «Ручная
прошивка V4» под кнопками с прошивками.


### 2. Установка приложения

Скачайте APK со [страницы загрузки](/download/) и разрешите установку из
неизвестных источников — Android спросит об этом сам. Все версии подписаны
одним ключом, поэтому обновление ставится поверх предыдущего, без удаления.

Из исходников:

\`\`\`bash
cd android/MeshTRX
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
\`\`\`

### 3. Подключение по BLE

1. Откройте приложение MeshTRX
2. Перейдите на вкладку **Настройки**
3. Нажмите **Подключить** — начнётся сканирование
4. Устройство появится как **MeshTRX-XXXX**
5. На экране устройства нажмите кнопку (>1 сек) — появится **PIN**
6. Введите PIN в приложении
7. Статус изменится на **● Подключено** (зелёный)

**Если телефон не видит устройство в списке Bluetooth.** Проверьте, что и
телефон, и устройство на свежих версиях: до 4.4.3 устройство выходило в эфир
без имени, поэтому телефон его не видел и не показывал в списке. Обновите обе
стороны — по отдельности не поможет. Если не помогло, выключите и включите
питание устройства: брошенное соединение оно отпускает само, но не мгновенно.

---

## Настольный клиент (Windows / Linux / macOS)

С компьютера доступно то же, что с телефона: голос, сообщения, вызовы, файлы,
радар, карта, настройки и ретранслятор. Плюс журнал диагностики, которого в
телефоне нет.

\`\`\`bash
python3 -m venv .venv
.venv/bin/pip install -r desktop/requirements.txt
.venv/bin/python desktop/run.py
\`\`\`

Клиенту нужна системная библиотека Codec2 (\`libcodec2\` в Linux и macOS,
\`codec2.dll\` рядом с программой в Windows). Если её нет, клиент скажет об этом
при запуске, а не останется молча без голоса.

Передача голоса — удержание кнопки мышью или пробелом, с тем же пределом в 10
секунд и тем же обратным отсчётом, что в телефоне. Подробности и разбор
устройства программы — в [desktop/README.md](https://github.com/StanislavButkovsky/meshtrx/blob/master/desktop/README.md).

---

## Голосовая связь (PTT)

### Кодек

Используется **Codec2 3200 bps** — обеспечивает разборчивую речь при минимальном использовании радиоканала.

| Параметр | Значение |
|----------|----------|
| Битрейт | 3200 bps |
| Фрейм | 20 мс / 8 байт |
| Пакет | 4 фрейма = 32 байта = 80 мс |
| LoRa-пакет | 39 байт (7 заголовок + 32 аудио) |
| Время в эфире | ~53 мс |

Задержка складывается из нескольких мест. Первое неустранимо: пока не набралось
80 мс речи, отправлять нечего — это нижняя граница, заданная размером пакета.
Дальше добавляется передача в устройство по Bluetooth (интервал соединения
30–50 мс), время пакета в эфире, обратный путь по Bluetooth и буфер
воспроизведения. Сквозную задержку на живом железе мы не мерили, поэтому числа
здесь не приводим: разговор идёт как по рации — сказал, отпустил, дождался
ответа.

### Сколько можно говорить: предел 10 секунд

Одна передача занимает канал целиком. LoRa работает в полудуплексе: пока
кто-то говорит, остальные не могут ни ответить, ни позвать на помощь — их
просто не слышно. Поэтому длительность речи ограничена **десятью секундами**
во всех режимах:

| Где | Что происходит на десятой секунде |
|-----|-----------------------------------|
| Удержание кнопки (PTT) | передача прекращается сама, на кнопке идёт обратный отсчёт; чтобы продолжить, отпустите кнопку и нажмите заново |
| Голосовая активация (VOX) | то же самое, с тем же отсчётом на кнопке; чтобы продолжить, сделайте паузу — передача начнётся с новой фразы |
| Голосовое сообщение в чате | запись останавливается, сообщение уходит как есть |
| Адресный голос | то же самое: пишется до 10 секунд и отправляется файлом |

Предел продублирован в самом устройстве, а не только в приложении: если
телефон завис или кнопка залипла, рация всё равно замолчит на десятой секунде
и освободит канал. На экране устройства при этом появится \`LIMIT 10s\`.

Говорить дольше — просто нажать кнопку ещё раз. Пауза между передачами нужна
и по существу: за это время собеседник успевает ответить, а сеть — пропустить
чужие сообщения и вызовы.

### Режим PTT (Push-To-Talk)

1. Убедитесь что переключатель **PTT/VOX** в положении **PTT**
2. Нажмите и **удерживайте** большую круглую кнопку
3. Говорите — статус покажет **● передача… осталось N с**
4. Отпустите кнопку — передача прекратится
5. Собеседник услышит короткий двухтональный сигнал окончания передачи
   (его можно выключить в настройках, раздел «Аудио»)

Если не отпустить, на десятой секунде передача завершится сама.

### Режим VOX (голосовая активация)

1. Переключите **PTT/VOX** в положение **VOX**
2. Передача начнётся автоматически при обнаружении голоса
3. Статусы: \`...\` (атака) → \`>>> TX <<<\` (передача) → \`TX (пауза)\` (затухание)

**Настройки VOX** (вкладка Настройки):
- **Порог VOX** (0–5000) — чувствительность, чем ниже тем чувствительнее
- **Задержка VOX** (200–2000 мс) — пауза перед окончанием передачи

### Громкая связь

Кнопка динамика (справа сверху от PTT кнопки):
- **Зелёная** — динамик включён (по умолчанию)
- **Серая** — динамик выключен (звук через наушник)

### Шумоподавление (PTT RMS)

В настройках слайдер **PTT RMS** (0–1000):
- **0** — отключено (передаётся всё, по умолчанию)
- **50–300** — лёгкая фильтрация фонового шума
- **300+** — агрессивная фильтрация (только громкая речь)

### Громкость приёма

Слайдер **Громкость приёма** в настройках (50%–300%, по умолчанию 200%).

---

## Режимы прослушивания

На PTT экране вверху две кнопки:

| Режим | Описание |
|-------|----------|
| **Все** | Слышите все передачи на канале |
| **Мои** | Слышите только адресованные вам вызовы |

Активный режим подсвечен зелёным.

---

## Система вызовов

### Типы вызовов

| Тип | Кнопка | Описание |
|-----|--------|----------|
| **Общий** | ОБЩИЙ (синяя) | Вызов всем на канале |
| **Личный** | ВЫЗВАТЬ (зелёная) | Вызов конкретному абоненту |
| **Групповой** | Через picker | Вызов группе (до 8 участников) |

### Как вызвать

1. **Общий вызов**: нажмите кнопку **ОБЩИЙ** — все на канале получат уведомление
2. **Личный вызов**: нажмите **ВЫЗВАТЬ** → выберите абонента из списка → вызов отправлен
3. Входящий вызов: появится overlay с кнопками **ПРИНЯТЬ** / **ОТКЛОНИТЬ**

### Последние вызовы

Внизу PTT экрана — скроллируемый список последних вызовов:
- Показывает уникальные записи (дубли заменяются)
- Направление: → исходящий, ← входящий
- Цвет по типу: синий (общий), зелёный (личный), жёлтый (группа)
- Нажатие — повторный вызов

---

## Текстовые сообщения

### Отправка

1. Перейдите на вкладку **Чат**
2. Введите текст (до 84 символов)
3. Нажмите кнопку отправки (зелёная стрелка)
4. По умолчанию — broadcast на канал

### Адресная отправка

1. Нажмите кнопку **@** рядом с полем ввода
2. Выберите получателя из списка
3. Над полем ввода появится **Кому: [имя]**
4. Сообщение получит только адресат

### Фильтрация

Используйте выпадающий список фильтра для просмотра сообщений от конкретного абонента или всех.

---

## Передача файлов

### Отправка фото

1. Перейдите на вкладку **Файлы**
2. Нажмите **Фото** — откроется галерея
3. Выберите фото — приложение само уменьшит его до 320×426 точек,
   пережмёт в JPEG и срежет метаданные; на выходе обычно 5–20 КБ
4. Подтвердите отправку
5. Прогресс виден в списке

### Отправка файлов

1. Нажмите **Файл** — откроется файловый менеджер
2. Выберите файл (макс. 100 КБ)
3. Передача стартует автоматически

### Параметры передачи

| Параметр | Значение |
|----------|----------|
| Макс. размер | 100 КБ |
| Размер чанка | 120 байт |
| Интервал | 100 мс |
| Скорость | ~1.2 КБ/с |
| 11 КБ фото | ~10 секунд |

Сто килобайт — это потолок для любого файла, а не размер, до которого
дожимается снимок: подготовленное приложением фото занимает 5–20 КБ и уходит
секунд за десять. Потолок стоит по двум причинам сразу. Первая — память
устройства: файл целиком лежит в оперативке рации, а её там около сотни
килобайт свободной, поэтому запрос на больший размер прошивка отклоняет, а не
обрывает передачу на середине. Вторая — эфир: канал один и полудуплексный, и
всё время передачи остальные не могут ни поговорить, ни отправить сообщение.
Отсюда и правило: чем меньше файл, тем меньше вы занимаете общий канал.

### Действия с файлами

Нажмите на файл в списке:
- **Поделиться** — отправить в другое приложение
- **Повторить** — повторная отправка
- **Удалить** — удалить из истории

---

## Карта и радар

### Карта (OpenStreetMap)

- Отображает ваше местоположение и все обнаруженные станции
- Маркеры с позывными и уровнем сигнала
- Пунктирные линии к станциям (цвет по давности)
- Кнопки:
  - **Я** — центрировать на своей позиции
  - **Все** — показать все станции

### Радар (тактический)

- Чёрный фон с зелёными кольцами дальности
- Компасная ориентация (реальный азимут)
- Станции отображаются относительно вашей позиции
- Кнопки масштаба +/− (100м–100км)
- Кнопка переключения контраста (обычный/яркий для улицы)
- Автомасштаб по дальней станции (5 км по умолчанию)
- Яркость по RSSI
- Позывной на точке

### GPS

Используется Android LocationManager (работает без GMS). Обновление каждые 15 секунд. Статус GPS отображается на экране карты.

---

## Настройки

### Подключение
- **Подключить/Отключить** — управление BLE соединением
- **Новое устройство** — забыть текущее и найти новое

### Позывной
- До 8 символов, отображается в шапке и в beacon

### Радио
- **Канал** (0–22) — выбор рабочей частоты (863.15–869.75 МГц)
- **Мощность TX** (1–22 дБм) — дальность передачи
- **Duty Cycle EU868** — ограничение 1% для соответствия EU нормам

### Аудио
- **Громкость приёма** (50–300%)
- **PTT RMS** (0–1000) — шумоподавление
- **Звук окончания передачи** — короткий сигнал, которым заканчивается чужая
  речь. Можно выключить, если мешает; настройка запоминается (с версии 4.4.5)
- **Порог VOX** (0–5000)
- **Задержка VOX** (200–2000 мс)

Тембр самого голоса не настраивается: в эфир он идёт кодеком Codec2 с
постоянной скоростью 3200 бит/с, и «модуляции» как отдельной регулировки нет.
Разборчивость меняют громкостью приёма и порогом PTT RMS: чем выше порог, тем
больше тихих звуков отсекается до передачи.

Начиная с версии 4.4.5 все настройки этого раздела запоминаются и переживают
перезапуск приложения. В более ранних версиях они сбрасывались к заводским
после каждого закрытия — если подобранный порог VOX «сам собой» возвращался к
800, дело было в этом.

### Beacon
- Интервал отправки маячка (Никогда / 1–60 мин / 1 час)

### Список станций
- Таймаут удаления неактивных (15 мин – 24 часа)

### История файлов
- Срок хранения (7 / 14 / 30 / 90 дней / Без ограничений)

### Ретранслятор
- См. раздел [Режим ретранслятора](#режим-ретранслятора)

### Язык
- Русский / English — переключение мгновенное

### Применить и сохранить
Кнопка отправляет настройки радио на устройство и сохраняет в NVS.

---

## Режим ретранслятора

Устройство может работать как автономный ретранслятор — принимает пакеты LoRa и пересылает их с уменьшением TTL.

### Что ретранслируется

| Тип пакета | Ретрансляция |
|------------|-------------|
| Голос (0xA0) | Да |
| Текст (0xB0) | Да |
| Файлы (0xC0, 0xC1, 0xC3) | Да |
| Beacon (0xD0) | Да |
| Вызовы (0xE0–0xE6) | Да |
| File ACK (0xC2) | Нет |

### Включение

1. Перейдите в **Настройки** → секция **Ретранслятор**
2. Опционально введите **WiFi SSID** и **пароль** для подключения к сети
3. Опционально введите **статический IP**
4. Нажмите **ВКЛЮЧИТЬ РЕТРАНСЛЯТОР**
5. Подтвердите в диалоге
6. Устройство перезагрузится в режиме ретранслятора

### WiFi мониторинг

После включения устройство поднимает WiFi:
- **Без SSID**: создаёт точку доступа \`MeshTRX-Repeater\` (пароль: \`meshtrx123\`)
- **С SSID**: подключается к указанной сети (fallback на AP при неудаче)

Веб-интерфейс доступен по адресу:
- AP режим: \`http://192.168.4.1\`
- STA режим: по указанному или полученному IP

### Веб-интерфейс

Страница обновляется каждые 5 секунд и показывает:
- **Uptime** — время работы
- **Канал** — текущий + возможность смены через dropdown
- **TX Power** — мощность передатчика
- **Forwarded / Dropped** — счётчики пакетов
- **По типам**: Audio, Text, File, Beacon
- **RSSI диапазон** — мин/макс уровень сигнала
- **IP адрес** — текущий

### Смена канала

Канал можно сменить:
- Через **веб-интерфейс** (dropdown + кнопка Set)
- Через **приложение** (подключиться по BLE и сменить в настройках)

### Выключение

- Через **приложение**: Настройки → **Выключить ретранслятор** (BLE доступен в режиме ретранслятора)
- Устройство перезагрузится в нормальный режим

### Дедупликация

- Кеш 64 записи, окно 30 секунд
- TTL уменьшается на 1 при каждой ретрансляции
- Пакеты с TTL=0 не ретранслируются
- Случайная задержка 10–50 мс для избежания коллизий

---

## Кнопка на устройстве

### В нормальном режиме

| Нажатие | Действие |
|---------|----------|
| Короткое (<1 сек) | Включить экран на 30 сек |
| Среднее (>1 сек) | Показать PIN и имя устройства на 10 сек |
| Долгое (>3 сек) | Выключить устройство |

### В режиме ретранслятора

| Нажатие | Действие |
|---------|----------|
| Короткое | Включить экран |
| Среднее (>1 сек) | Сбросить статистику |
| Длинное (>3 сек) | Выйти из режима ретранслятора |
| Долгое (>8 сек) | Выключить устройство |

### Выключение и включение

Выключателя питания на плате нет, поэтому его роль играет кнопка. Держите её
три секунды: на экране крупно появится **SLEEP**, и устройство уснёт — радио,
усилитель и экран обесточиваются, потребление падает до микроампер, то есть
батареи хватает на месяцы ожидания.

Включается тем же нажатием: короткого хватает. Выход из этого сна — полный
старт устройства, как после подачи питания, поэтому связь и настройки
восстанавливаются сами. В журнале это видно строкой \`reason=DEEPSLEEP
wake=BUTTON\`.

В режиме ретранслятора порог больше — восемь секунд: там короткие удержания
уже заняты сбросом статистики и выходом из режима. Пока вы держите кнопку, на
экране идёт обратный отсчёт, так что вслепую держать не придётся.

---

## Индикация

### OLED дисплей

**Нормальный режим**: канал, частота, RSSI, SNR, мощность TX, BLE статус, напряжение батареи (2 знака), VOX статус.

**Режим ретранслятора**: \`** REPEATER **\`, канал, частота, счётчики FWD/DRP, последний RSSI/SNR, TTL.

Экран автоматически выключается через 30 секунд.

### LED (GPIO35)

| Паттерн | Значение |
|---------|----------|
| Постоянно горит | Передача (TX) |
| Мигание ~300 мс | BLE ожидает подключения |
| Короткая вспышка 5 сек | BLE подключено |
| Быстрое мигание | Передача файла |
| Короткий импульс | Приём пакета (RX) |

### Шапка приложения

| Элемент | Положение | Описание |
|---------|-----------|----------|
| Позывной | Слева сверху | Крупный белый текст |
| Имя устройства | Слева снизу | Мелкий серый текст |
| Статус | Справа сверху | ● Подключено (зелёный) / Отключено (серый) |
| Канал + частота | Справа снизу | Зелёный текст "CH 5 · 864.65 MHz" |

### Нижняя панель навигации

5 вкладок: **PTT**, **Чат**, **Файлы**, **Карта**, **Настройки**. Активная вкладка подсвечена зелёным.

---

## Технические характеристики

### LoRa радио

| Параметр | Значение |
|----------|----------|
| Диапазон | 863.15–869.75 МГц |
| Каналы | 23 (шаг 300 кГц) |
| Bandwidth | 250 кГц |
| Spreading Factor | 7 |
| Coding Rate | 4/5 |
| Sync Word | 0x34 |
| Мощность | 1–22 дБм |
| Дальность | до 5+ км (прямая видимость) |

### Аудио кодек

| Параметр | Значение |
|----------|----------|
| Кодек | Codec2 3200 bps |
| Частота дискретизации | 8000 Гц |
| Фрейм | 160 сэмплов = 20 мс = 8 байт |
| Пакет | 4 фрейма = 32 байта = 80 мс |
| LoRa пакет | 39 байт (7 заголовок + 32 аудио) |

### BLE протокол

| Параметр | Значение |
|----------|----------|
| Сервис | Nordic UART Service (NUS) |
| MTU | 128 байт |
| Команды | 40+ (0x01–0x28) |
| Аудио пакет | 36 байт (cmd + flags + отправитель + 32 payload) |
| Авторизация | 4-значный PIN (из MAC) |

### Пакеты LoRa

| Тип | ID | Размер |
|-----|-----|--------|
| Голос | 0xA0 | 39 байт |
| Текст | 0xB0 | до 91 байт |
| Файл (заголовок) | 0xC0 | 36 байт |
| Файл (чанк) | 0xC1 | до 128 байт |
| Файл (конец) | 0xC3 | 6 байт |
| Beacon | 0xD0 | 36 байт |
| Вызовы | 0xE0–0xE6 | 8–47 байт |

### Батарея

| Параметр | Значение |
|----------|----------|
| ADC | GPIO1 через делитель |
| Управление | GPIO37 enable |
| Калибровка | множитель 5.55 |
| Усреднение | 8 замеров |
| Диапазон | 3.0V (0%) – 4.2V (100%) |
`;

export const USER_GUIDE_EN = `# MeshTRX — User Guide

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

\`\`\`
[Phone A] <--BLE--> [Heltec A] <--LoRa 868 MHz--> [Heltec B] <--BLE--> [Computer B]
\`\`\`

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
| V3 | SX1262, no external amplifier | \`firmware-v3-<version>.bin\` |
| V4 rev 4.2 | GC1109 amplifier | \`firmware-v4-<version>.bin\` |
| V4 rev 4.3 | KCT8103L amplifier | \`firmware-v4.3-<version>.bin\` |

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

\`\`\`bash
cd firmware
pio run -e heltec_wifi_lora_32_V3  --target upload --upload-port /dev/ttyUSB0   # V3
pio run -e heltec_wifi_lora_32_V4  --target upload --upload-port /dev/ttyUSB0   # V4 rev 4.2
pio run -e heltec_wifi_lora_32_V43 --target upload --upload-port /dev/ttyUSB0   # V4 rev 4.3
\`\`\`

After updating the firmware, **update the app as well**: the two agree on a shared protocol, and the pair "new firmware + old app" may fail to find each other over Bluetooth.

**Flashing by hand, with esptool.** You will need this for a V4: the wizard on the site only writes the V3 image. The firmware file alone is not enough — after a full erase the board has neither a bootloader nor a partition table left, and it goes into an endless reboot loop. Four files are needed, each at its own address:

\`\`\`bash
# esptool 5.x — the command and flags use dashes
esptool --chip esp32s3 --port COM3 --baud 921600 \\
  write-flash -z --flash-mode dio --flash-freq 80m --flash-size 16MB \\
  0x0     bootloader-v4.bin \\
  0x8000  partitions-v4.bin \\
  0xe000  boot_app0.bin \\
  0x10000 firmware-v4.3-<version>.bin

# esptool 4.x — the same thing with underscores
esptool.py --chip esp32s3 --port COM3 --baud 921600 \\
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB \\
  0x0     bootloader-v4.bin \\
  0x8000  partitions-v4.bin \\
  0xe000  boot_app0.bin \\
  0x10000 firmware-v4.3-<version>.bin
\`\`\`

\`esptool version\` tells you which one you have. In version 5 the commands and flags moved to dashes and the entry point became plain \`esptool\`; the old spelling is refused.

The two mistakes people make most often:

- **On the ESP32-S3 the bootloader goes to \`0x0\`, not \`0x1000\`.** \`0x1000\` is the address for the older ESP32; with it the board powers up and immediately reboots, over and over.
- **The V4 has 16 MB of flash**, hence \`--flash_size 16MB\`. The V3 has 8 MB and its own bootloader and partition table: \`bootloader.bin\` and \`partitions.bin\`.

All four files sit on the [download page](/download/), in the "Flashing a V4 by hand" block under the firmware buttons.


### 2. Installing the app

Download the APK from the [download page](/download/) and allow installation from unknown sources — Android will ask about this itself. Every release is signed with the same key, so an update installs over the previous one without uninstalling.

From source:

\`\`\`bash
cd android/MeshTRX
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
\`\`\`

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

\`\`\`bash
python3 -m venv .venv
.venv/bin/pip install -r desktop/requirements.txt
.venv/bin/python desktop/run.py
\`\`\`

The client needs the system Codec2 library (\`libcodec2\` on Linux and macOS, \`codec2.dll\` next to the program on Windows). If it is missing, the client says so at startup instead of silently staying mute.

To transmit, hold the button with the mouse or press the space bar — with the same ten-second limit and the same countdown as on the phone. Details and a walk-through of the program are in [desktop/README.md](https://github.com/StanislavButkovsky/meshtrx/blob/master/desktop/README.md).

---

## Voice (PTT)

### Codec

**Codec2 at 3200 bps** — intelligible speech at minimal use of the radio channel.

| Parameter | Value |
|-----------|-------|
| Bitrate | 3200 bps |
| Frame | 20 ms / 8 bytes |
| Packet | 4 frames = 32 bytes = 80 ms |
| LoRa packet | 39 bytes (7 header + 32 audio) |
| Time on air | ~53 ms |

The delay comes from several places. The first cannot be removed: until 80 ms of
speech has accumulated there is nothing to send — that is the floor, set by the
packet size. On top of it come the Bluetooth hop to the device (connection
interval 30–50 ms), the packet's time on air, the Bluetooth hop back and the
playback buffer. We have not measured the end-to-end delay on real hardware, so
no figure is given here: a conversation goes the way it goes on a radio — speak,
release, wait for the answer.

### How long you may talk: the ten-second limit

A single transmission takes the whole channel. LoRa is half-duplex: while somebody is talking, the others can neither answer nor call for help — they simply are not heard. So speech is limited to **ten seconds** in every mode:

| Where | What happens at the tenth second |
|-------|----------------------------------|
| Holding the button (PTT) | transmission stops by itself, the button shows a countdown; to continue, release it and press again |
| Voice activation (VOX) | the same, with the same countdown; to continue, pause — transmission restarts with the new phrase |
| Voice message in chat | recording stops and the message is sent as it is |
| Addressed voice | the same: up to 10 seconds are recorded and sent as a file |

The limit is duplicated inside the device itself, not just in the app: if the phone freezes or the button sticks, the radio still falls silent at the tenth second and frees the channel. \`LIMIT 10s\` appears on the device screen.

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
3. States: \`...\` (attack) → \`>>> TX <<<\` (transmitting) → \`TX (pause)\` (hangtime)

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
3. Pick a photo — the app scales it down to 320×426, re-encodes it as JPEG
   and strips the metadata; the result is usually 5–20 KB
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

The hundred kilobytes is a ceiling for any file, not the size a photo is squeezed to: a photo prepared by the app takes 5–20 KB and goes out in about ten seconds. The ceiling exists for two reasons at once. First, the device's memory: an incoming file is held whole in the radio's RAM, and there is about a hundred kilobytes of it free, so a request for more is refused outright rather than dropped halfway. Second, the air: the channel is single and half-duplex, and for the whole transfer nobody else can talk or send a message. Hence the rule of thumb: the smaller the file, the less of the shared channel you take.

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
- **Without an SSID**: it creates the access point \`MeshTRX-Repeater\` (password: \`meshtrx123\`)
- **With an SSID**: it joins that network (falling back to the access point on failure)

The web interface is available at:
- AP mode: \`http://192.168.4.1\`
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

The same button turns it back on; a short press is enough. Waking from this sleep is a full start, exactly as after applying power, so the link and the settings come back on their own. In the log it shows as \`reason=DEEPSLEEP wake=BUTTON\`.

In repeater mode the threshold is longer — eight seconds: there the shorter holds are already taken by resetting the statistics and leaving the mode. While you hold the button a countdown runs on the screen, so you are not holding it blind.

---

## Indicators

### OLED display

**Normal mode**: channel, frequency, RSSI, SNR, TX power, BLE status, battery voltage (two decimals), VOX status.

**Repeater mode**: \`** REPEATER **\`, channel, frequency, FWD/DRP counters, last RSSI/SNR, TTL.

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
| Packet | 4 frames = 32 bytes = 80 ms |
| LoRa packet | 39 bytes (7 header + 32 audio) |

### BLE protocol

| Parameter | Value |
|-----------|-------|
| Service | Nordic UART Service (NUS) |
| MTU | 128 bytes |
| Commands | 40+ (0x01–0x28) |
| Audio packet | 36 bytes (cmd + flags + sender + 32 payload) |
| Authorisation | a 4-digit PIN (derived from the MAC) |

### LoRa packets

| Type | ID | Size |
|------|-----|------|
| Voice | 0xA0 | 39 bytes |
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
`;
