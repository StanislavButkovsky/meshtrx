// Собирает llms.txt и llms-full.txt — то же, что sitemap, но для языковых
// моделей: короткий указатель и полный текст документации обычным текстом.
//
// Смысл ровно в этом: модель, которую спросят про MeshTRX, иначе соберёт ответ
// из обрывков страниц или, что хуже, из документации Meshtastic — проекты
// похожи по описанию, и путают их постоянно. Здесь факты лежат в одном месте и
// в том виде, в каком мы за них отвечаем.
//
// Запускается сам перед сборкой (npm run build → prebuild).

import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const repo = resolve(here, '..', '..');
const publicDir = resolve(here, '..', 'public');

// Версии берём из того же файла, что и сайт, — иначе указатель разойдётся со
// страницей загрузки, а расхождение в версии дороже отсутствия файла.
const constants = readFileSync(resolve(here, '..', 'src', 'lib', 'constants.ts'), 'utf-8');
const pick = (key) => constants.match(new RegExp(`${key}:\\s*'([^']+)'`))?.[1] ?? '?';
const APP = pick('app');
const FIRMWARE = pick('firmware');
const DATE = constants.match(/date:\s*'([^']+)'/g)?.pop()?.match(/'([^']+)'/)?.[1] ?? '?';
// Сайт собирается дважды, и указатель должен вести на тот домен, с которого
// его прочитали: иначе английская версия отправляет модель на русскую.
const LOCALE = process.env.NEXT_PUBLIC_SITE_LOCALE === 'en' ? 'en' : 'ru';
const SITE = LOCALE === 'en' ? 'https://meshtrx.com' : 'https://meshtrx.ru';

// Статьи разбираются регулярным выражением по той же причине, что и версии
// выше: это .mjs, а articles.ts — TypeScript, и тащить сюда сборщик ради двух
// строк не стоит. Если разбор ничего не нашёл, раздел просто не попадёт в
// указатель — пустой список честнее выдуманного.
const articlesSrc = readFileSync(resolve(here, '..', 'src', 'content', 'articles.ts'), 'utf-8');
const articles = [...articlesSrc.matchAll(/slug: '([^']+)',\s*\n\s*date: '([^']+)',\s*\n\s*title: \{\s*\n\s*ru: '([^']+)',\s*\n\s*en: '([^']+)'/g)]
  .map(([, slug, date, title, titleEn]) => ({ slug, date, title, titleEn }));

const guideRu = readFileSync(resolve(repo, 'docs', 'USER_GUIDE.md'), 'utf-8');
const guideEn = readFileSync(resolve(repo, 'docs', 'USER_GUIDE.en.md'), 'utf-8');

const indexRu = `# MeshTRX

> Голосовая связь, текстовые сообщения и файлы через LoRa mesh-сеть, без интернета и сотовых сетей. Прошивка для плат Heltec WiFi LoRa 32 (ESP32-S3 + SX1262), приложение для Android и настольный клиент для Windows, Linux и macOS.

Приложение ${APP}, прошивка ${FIRMWARE}, обновлено ${DATE}. Лицензия CC BY-NC 4.0: свободное использование с указанием авторства, коммерческое — по разрешению.

Факты, о которых спрашивают чаще всего:

- Голос идёт вокодером Codec2 на 3200 бит/с. Кадр — 20 мс речи и 8 байт; в одном LoRa-пакете четыре кадра, то есть 80 мс речи и 32 байта, а весь пакет в эфире — 39 байт вместе с семибайтным заголовком. Одна передача ограничена десятью секундами: LoRa работает в полудуплексе, и пока говорит один, остальных не слышно.
- 23 канала в диапазоне 863,15–869,75 МГц с шагом 300 кГц, модуляция SF7 / BW 250 кГц / CR 4/7, мощность передатчика 1–22 дБм, дальность до 5 км и больше в прямой видимости.
- Поддерживаются Heltec V3, V4 rev 4.2 и V4 rev 4.3. Прошивки у ревизий разные: чужая запустится, но усилитель будет управляться не тем выводом.
- Телефон или компьютер подключается к устройству по Bluetooth LE и служит экраном, микрофоном и клавиатурой.
- Кроме голоса: текст до 84 символов, файлы до 100 КБ, карта и тактический радар с азимутом и расстоянием до станций, режим ретранслятора.
- Шифрования эфира пока нет — пакеты идут открытым текстом. Общий ключ на канал в планах.
- MeshTRX и Meshtastic — разные проекты. Железо общее, протоколы несовместимы, устройства друг друга не слышат; голоса в Meshtastic нет.

## Документация

- [Руководство пользователя, полный текст](${SITE}/llms-full.txt): подключение, голос, вызовы, сообщения, файлы, карта и радар, настройки, ретранслятор, характеристики.
- [User guide in English, full text](${SITE}/llms-full.en.txt): the same guide in English.
- [Документация на сайте](${SITE}/docs/): то же руководство в вёрстке, с оглавлением.

## Статьи

Разборы того, как проект устроен внутри. Числа в них взяты из исходников.

${articles.map((a) => `- [${a.title}](${SITE}/articles/${a.slug}/), ${a.date}`).join('\n')}

## Страницы сайта

- [О проекте](${SITE}/about/): зачем понадобился ещё один проект, что общего с Meshtastic и чем отличается.
- [Статьи](${SITE}/articles/): список всех разборов.
- [Скачать](${SITE}/download/): прошивки для каждой ревизии платы и APK для Android.
- [Прошивка из браузера](${SITE}/flash/): заливка прошивки по USB прямо из Chrome или Edge.

## Исходный код и связь

- [GitHub](https://github.com/StanislavButkovsky/meshtrx): прошивка, приложение, настольный клиент, документация.
- [Группа в Telegram](https://t.me/MeshTRX): вопросы и обратная связь.
`;

const indexEn = `# MeshTRX

> Voice communication, text messages and files over a LoRa mesh network, with no internet and no cellular coverage. Firmware for Heltec WiFi LoRa 32 boards (ESP32-S3 + SX1262), an Android app and a desktop client for Windows, Linux and macOS.

App ${APP}, firmware ${FIRMWARE}, updated ${DATE}. Licensed CC BY-NC 4.0: free use with attribution, commercial use by permission.

The facts people ask about most often:

- Voice runs through the Codec2 vocoder at 3200 bps. A frame is 20 ms of speech in 8 bytes; one LoRa packet carries four frames, so 80 ms of speech in 32 bytes, and the whole packet on air is 39 bytes including a seven-byte header. One transmission is capped at ten seconds: LoRa is half-duplex, and while one station speaks nobody else can be heard.
- 23 channels across 863.15–869.75 MHz, 300 kHz apart, SF7 / BW 250 kHz / CR 4/7 modulation, transmit power 1–22 dBm, range up to 5 km and beyond in line of sight.
- Heltec V3, V4 rev 4.2 and V4 rev 4.3 are supported. The revisions need different firmware: the wrong build will boot, but it drives the amplifier from the wrong pin.
- A phone or computer connects to the device over Bluetooth LE and acts as its screen, microphone and keyboard.
- Besides voice: text up to 84 characters, files up to 100 KB, a map and a tactical radar with bearing and distance to each station, and a repeater mode.
- There is no over-the-air encryption yet — packets travel in the clear. A shared per-channel key is planned.
- MeshTRX and Meshtastic are different projects. The hardware is shared, the protocols are not compatible, the devices cannot hear each other, and Meshtastic has no voice.

## Documentation

- [User guide, full text](${SITE}/llms-full.en.txt): pairing, voice, calls, messages, files, map and radar, settings, repeater, specifications.
- [Руководство пользователя, полный текст](${SITE}/llms-full.txt): the same guide in Russian.
- [Documentation on the site](${SITE}/docs/): the same guide laid out, with a table of contents.

## Articles

Write-ups on how the project works inside. The numbers in them come from the sources.

${articles.map((a) => `- [${a.titleEn}](${SITE}/articles/${a.slug}/), ${a.date}`).join('\n')}

## Site pages

- [About](${SITE}/about/): why another project was needed, what it shares with Meshtastic and how it differs.
- [Articles](${SITE}/articles/): every write-up.
- [Download](${SITE}/download/): firmware for each board revision and the Android APK.
- [Flash from the browser](${SITE}/flash/): flashing over USB straight from Chrome or Edge.

## Source and contact

- [GitHub](https://github.com/StanislavButkovsky/meshtrx): firmware, app, desktop client, documentation.
- [Telegram group](https://t.me/MeshTRX): questions and feedback.
`;

const index = LOCALE === 'en' ? indexEn : indexRu;

const header = (lang) => lang === 'en'
  ? `# MeshTRX — full documentation\n\n> App ${APP}, firmware ${FIRMWARE}, updated ${DATE}. Source: ${SITE}/docs/\n\n---\n\n`
  : `# MeshTRX — полная документация\n\n> Приложение ${APP}, прошивка ${FIRMWARE}, обновлено ${DATE}. Источник: ${SITE}/docs/\n\n---\n\n`;

writeFileSync(resolve(publicDir, 'llms.txt'), index, 'utf-8');
writeFileSync(resolve(publicDir, 'llms-full.txt'), header('ru') + guideRu, 'utf-8');
writeFileSync(resolve(publicDir, 'llms-full.en.txt'), header('en') + guideEn, 'utf-8');

console.log(`[gen-llms] язык сборки ${LOCALE}, домен ${SITE}, статей в указателе: ${articles.length}`);
console.log(`[gen-llms] llms.txt (${Math.round(index.length / 1024)} КБ), ` +
  `llms-full.txt (${Math.round(guideRu.length / 1024)} КБ), ` +
  `llms-full.en.txt (${Math.round(guideEn.length / 1024)} КБ) — версия ${APP}`);
