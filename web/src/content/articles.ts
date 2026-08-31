export type Localized = { ru: string; en: string };

export interface Article {
  slug: string;
  /** Дата публикации, ISO. По ней же статьи сортируются в списке. */
  date: string;
  title: Localized;
  /** Короткое описание: карточка в списке и <meta description> страницы. */
  summary: Localized;
  /** Тело статьи в markdown. Заголовки начинаются с ##: h1 — это заголовок страницы. */
  body: Localized;
}

// Статьи лежат в коде, а не в markdown-файлах рядом: сайт собирается статически,
// и разбирать файлы на этапе сборки — лишний шаг ради одного-двух текстов в год.
// Когда статей станет заметно больше, их можно вынести в docs/ и собирать
// скриптом, как уже собирается страница документации.
//
// Порядок — от новых к старым, руками. Список короткий, и явный порядок понятнее
// сортировки, которая молча переставит статьи, если в дате опечатка.
export const ARTICLES: Article[] = [
  {
    slug: 'voice-over-lora',
    date: '2026-08-31',
    title: {
      ru: 'Как голос уместился в LoRa',
      en: 'How voice fits into LoRa',
    },
    summary: {
      ru: 'LoRa придумывали для датчиков влажности, а не для разговоров. Разбираем, что пришлось сделать, чтобы в этот канал влезла речь: Codec2 на 3200 бит/с, пакет в 39 байт, предел в десять секунд — и чем за это пришлось заплатить.',
      en: 'LoRa was designed for humidity sensors, not conversations. Here is what it took to fit speech into that channel: Codec2 at 3200 bps, a 39-byte packet, a ten-second limit — and what we gave up for it.',
    },
    body: {
      ru: `Голосовая связь — то, ради чего MeshTRX вообще появился, и то, чего в похожих проектах на LoRa обычно нет. Ниже — как голос в MeshTRX устроен: почему его не должно было получиться, что пришлось выбросить и чем мы за это заплатили.

Все числа здесь взяты из исходников прошивки и клиентов, а не из описания проекта.

## Почему голос по LoRa считается невозможным

LoRa придумывали не для разговоров. Её задача — донести до шлюза десяток байт с датчика влажности, потратив на это микроватты и добравшись за несколько километров сквозь стены. Всё остальное следует отсюда: медленная модуляция, узкая полоса, маленькие пакеты.

На параметрах MeshTRX — SF7, полоса 250 кГц, избыточность 4/7 — канал даёт около 7,8 кбит/с. Это на бумаге: без учёта преамбулы, заголовков и пауз между передачами, так что на деле заметно меньше.

Для сравнения: голос в GSM — 13 кбит/с, самый скромный интернет-звонок — 8–16 кбит/с. То есть привычные способы передать речь в этот канал не помещаются вовсе. Не «работают плохо», а не помещаются. Задача, которую решает MeshTRX, начинается ровно здесь.

## Codec2: голос как описание, а не как звук

Выход нашёлся не в сжатии, а в смене подхода.

Обычный кодек хранит звуковую волну — точнее или грубее, но именно волну. [Codec2](https://www.rowetel.com/?page_id=452) Дэвида Роу устроен иначе: это вокодер. Он не передаёт волну совсем. Вместо неё в эфир уходит описание того, как речь была устроена в этот момент: основной тон, энергия, форма спектра. На другом конце декодер синтезирует речь заново — по этому описанию, с нуля.

MeshTRX использует режим 3200 — 3200 бит в секунду. Это тот же кодек и тот же режим во всех трёх частях проекта: в прошивке рации, в приложении для Android и в настольном клиенте. Один кадр это 20 мс звука, 160 отсчётов при частоте дискретизации 8 кГц, и укладывается он в **8 байт**.

Восемь байт на двадцать миллисекунд речи. Ради этого всё и затевалось: 3200 бит/с влезают в канал с запасом почти вдвое, и остаётся место на служебные данные, текст и файлы.

## Что лежит в пакете

Каждые 80 мс прошивка MeshTRX собирает один пакет: четыре кадра Codec2 подряд — 32 байта. К ним семь байт заголовка:

| Байт | Поле | Зачем |
|---|---|---|
| 0 | type | Что это: аудио, текст, файл, служебное |
| 1 | channel | Номер канала, 0–22 |
| 2 | seq | Порядковый номер пакета |
| 3 | flags | Начало и конец передачи, голосовая активация |
| 4 | ttl | Сколько ещё пересылок разрешено |
| 5–6 | sender | Последние два байта MAC — кто говорит |

Целиком голосовой пакет MeshTRX выглядит так:

![Голосовой пакет MeshTRX: семь байт заголовка и четыре кадра Codec2 по восемь байт — 39 байт в эфире на 80 мс речи](figure:packet)

Итого **39 байт в эфире, из них 32 — собственно речь**. Заголовок занимает почти пятую часть пакета, и это не расточительность: без seq приёмник не заметит потерю, без flags не поймёт, что передача кончилась, без ttl пакет пойдёт по сети кругами.

Почему четыре кадра, а не восемь и не шестнадцать? Чем больше кадров в пакете, тем меньше доля заголовка — но тем дольше копится звук перед отправкой и тем больнее обходится каждая потеря. Восемьдесят миллисекунд оказались компромиссом: потерянный пакет срезает меньше десятой доли секунды речи, и это слышно как щелчок, а не как провал в середине слова.

## Полудуплекс и предел в десять секунд

У рации MeshTRX одна антенна и один приёмопередатчик. Пока она передаёт — она не принимает. Не «принимает хуже», а не принимает вовсе.

Из этого следует вещь, которую в описаниях mesh-сетей обычно не пишут: **пока один говорит, остальных в канале нет**. Не только его собеседника — всех. Никто не может ответить, вклиниться или позвать на помощь. Канал принадлежит тому, кто нажал кнопку, и ровно до тех пор, пока он её держит.

Поэтому передача в MeshTRX ограничена десятью секундами. Ограничение стоит в двух местах сразу: в приложении, где на кнопке идёт обратный отсчёт, и в самой рации — константой в прошивке. Второе не дубль ради надёжности кода, а защита от вполне бытового случая: телефон завис, приложение убила система, кнопку зажало в кармане. Рация в этом случае замолкает сама и пишет на экране LIMIT 10s.

Десяти секунд хватает на осмысленную фразу и не хватает на монолог — так и задумано.

## Откуда берётся задержка

Ждать приходится в нескольких местах, и первое ожидание неустранимо: пока не набралось 80 мс речи, отправлять нечего. Это нижняя граница, заданная размером пакета.

Дальше добавляется передача в рацию по Bluetooth, время пакета в эфире — несколько десятков миллисекунд, обратный путь по Bluetooth в чужой телефон и буфер приёма, который сглаживает неровный приход пакетов.

В сумме получается задержка, к которой привыкаешь за минуту, но которую нельзя не заметить. Разговор в MeshTRX от неё меняется по форме: он идёт не как по телефону, а как по рации — сказал, отпустил, дождался ответа. Перебивать собеседника всё равно бесполезно, его в этот момент никто не слышит.

## Чем мы за это заплатили

Честная часть.

**Голос узнаётся хуже.** Codec2 3200, на котором работает MeshTRX, — это не сжатый звук, а заново синтезированная речь. Слова разборчивы, интонация в основном на месте, но тембр вокодер отдаёт первым: близкого человека по голосу вы, скорее всего, узнаете, а незнакомого от незнакомого отличите не всегда.

**Только речь.** Модель рассчитана на одного говорящего. Музыка, шум ветра, второй голос на фоне не передаются в принципе — на выходе будет невнятица.

**Эфир открыт.** Шифрования в MeshTRX пока нет, пакеты идут открытым текстом, и принять их может кто угодно с таким же модулем. Общий ключ на канал — в планах, но сегодня это так.

**Дальность честная, а не рекламная.** Пять километров, которые заявлены у MeshTRX, — это прямая видимость. В городе, в лесу, между этажами будет меньше, иногда сильно меньше. Ретранслятор помогает, но он же занимает канал.

## Что дальше

Голос — самая заметная часть MeshTRX, но не единственная: в той же сети живут текст, файлы, позиции на карте и радар. Как устроен MeshTRX целиком и как им пользоваться — в [документации](/docs/). Что мы делаем дальше и от чего сознательно отказались — на странице [о проекте](/about/).

Вопросы и замечания — в [группе в Telegram](https://t.me/MeshTRX). Исходники, включая всё, о чём здесь написано, — на [GitHub](https://github.com/StanislavButkovsky/meshtrx).`,
      en: `Voice is the reason MeshTRX exists, and the thing similar LoRa projects usually do not have. Here is how voice in MeshTRX works: why it should not have been possible, what had to go, and what we paid for it.

Every number below comes from the firmware and client sources, not from the project description.

## Why voice over LoRa is considered impossible

LoRa was not designed for conversations. Its job is to carry a dozen bytes from a humidity sensor to a gateway, on microwatts, across several kilometres and through walls. Everything else follows from that: slow modulation, narrow bandwidth, small packets.

At the MeshTRX settings — SF7, 250 kHz bandwidth, 4/7 coding rate — the channel gives about 7.8 kbps. That is on paper, before the preamble, the headers and the gaps between transmissions, so the real figure is noticeably lower.

For comparison: GSM voice runs at 13 kbps, and the most modest internet call at 8–16 kbps. The usual ways of carrying speech do not fit into this channel at all. Not "work poorly" — do not fit. The problem MeshTRX set out to solve starts exactly here.

## Codec2: voice as a description, not as sound

The way out was not better compression but a different approach.

An ordinary codec stores the sound wave — more or less accurately, but the wave itself. [Codec2](https://www.rowetel.com/?page_id=452) by David Rowe works differently: it is a vocoder. It does not transmit the waveform at all. What goes on air instead is a description of how the speech was shaped at that moment: pitch, energy, spectral envelope. At the far end the decoder synthesises speech anew from that description, from scratch.

MeshTRX uses mode 3200 — 3200 bits per second. It is the same codec in the same mode across all three parts of the project: the radio firmware, the Android app and the desktop client. One frame is 20 ms of audio, 160 samples at an 8 kHz sampling rate, and it fits into **8 bytes**.

Eight bytes per twenty milliseconds of speech. That is the whole point: 3200 bps fit into the channel with almost a twofold margin, leaving room for control data, text and files.

## What is inside a packet

Every 80 ms the MeshTRX firmware assembles one packet: four consecutive Codec2 frames — 32 bytes. Seven bytes of header go with them:

| Byte | Field | Purpose |
|---|---|---|
| 0 | type | What this is: audio, text, file, control |
| 1 | channel | Channel number, 0–22 |
| 2 | seq | Packet sequence number |
| 3 | flags | Start and end of transmission, voice activation |
| 4 | ttl | How many more hops are allowed |
| 5–6 | sender | Last two bytes of the MAC — who is speaking |

A complete MeshTRX voice packet looks like this:

![A MeshTRX voice packet: seven bytes of header and four 8-byte Codec2 frames — 39 bytes on air carrying 80 ms of speech](figure:packet)

That makes **39 bytes on air, 32 of them actual speech**. The header takes almost a fifth of the packet, and that is not waste: without seq the receiver would not notice a loss, without flags it would not know the transmission has ended, without ttl the packet would circle the network forever.

Why four frames and not eight or sixteen? The more frames in a packet, the smaller the header's share — but the longer audio accumulates before it is sent, and the more each loss costs. Eighty milliseconds turned out to be the compromise: a lost packet cuts out less than a tenth of a second of speech, which sounds like a click rather than a hole in the middle of a word.

## Half-duplex and the ten-second limit

A MeshTRX radio has one antenna and one transceiver. While it transmits, it does not receive. Not "receives worse" — does not receive at all.

From this follows something mesh network descriptions usually leave out: **while one person is speaking, nobody else is in the channel**. Not just their correspondent — everyone. No one can answer, cut in, or call for help. The channel belongs to whoever pressed the button, for exactly as long as they hold it.

That is why a transmission in MeshTRX is capped at ten seconds. The limit sits in two places at once: in the app, where the button counts down, and in the radio itself, as a constant in the firmware. The second one is not code redundancy but protection against an entirely ordinary case: the phone froze, the system killed the app, the button got pressed inside a pocket. The radio then goes silent on its own and shows LIMIT 10s on its screen.

Ten seconds is enough for a meaningful sentence and not enough for a monologue. That is the intent.

## Where the delay comes from

There is waiting in several places, and the first of them cannot be removed: until 80 ms of speech has accumulated, there is nothing to send. That is the floor, set by the packet size.

On top of it come the Bluetooth hop to the radio, the packet's time on air — a few tens of milliseconds, the Bluetooth hop back into someone else's phone, and the receive buffer that smooths out the uneven arrival of packets.

The result is a delay you get used to within a minute but cannot fail to notice. It changes the shape of a conversation in MeshTRX: it goes the way it goes on a radio, not on a phone — speak, release, wait for the answer. Interrupting is pointless anyway, since nobody can hear you while the other station is transmitting.

## What we paid for it

The honest part.

**Voices are harder to recognise.** Codec2 3200, the mode MeshTRX runs on, is not compressed audio but resynthesised speech. Words are clear, intonation is mostly there, but timbre is the first thing a vocoder gives up: you will probably recognise someone close to you, but you will not always tell one stranger from another.

**Speech only.** The model assumes a single speaker. Music, wind noise, a second voice in the background simply do not survive the trip — what comes out is mush.

**The air is open.** MeshTRX has no encryption yet, packets travel in the clear, and anyone with the same module can receive them. A shared per-channel key is planned, but that is how it stands today.

**The range figure is honest, not promotional.** The five kilometres claimed for MeshTRX mean line of sight. In a city, in a forest, between floors it will be less, sometimes much less. A repeater helps, but it also occupies the channel.

## What is next

Voice is the most visible part of MeshTRX but not the only one: text, files, map positions and the radar live in the same network. How MeshTRX works as a whole and how to use it is in the [documentation](/docs/). What we are doing next and what we deliberately dropped is on the [about page](/about/).

Questions and remarks go to the [Telegram group](https://t.me/MeshTRX). The sources, including everything described here, are on [GitHub](https://github.com/StanislavButkovsky/meshtrx).`,
    },
  },
];

export function articleBySlug(slug: string): Article | undefined {
  return ARTICLES.find((a) => a.slug === slug);
}

// Время чтения считается по тексту, а не проставляется руками: проставленное
// руками расходится с текстом при первой же правке, и заметить это некому.
export function readingMinutes(text: string): number {
  const words = text.trim().split(/\s+/).length;
  return Math.max(1, Math.round(words / 150));
}
