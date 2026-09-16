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
    slug: 'encryption-in-39-bytes',
    date: '2026-09-16',
    title: {
      ru: 'Шифрование в 39 байтах',
      en: 'Encryption in 39 bytes',
    },
    summary: {
      ru: 'Сейчас эфир MeshTRX открыт, и мы говорим об этом прямо. Разбираем, почему «просто включить AES» не работает в пакете на 39 байт, сколько стоит подпись пакета в миллисекундах эфира и что реально даёт общий ключ на канал.',
      en: 'MeshTRX traffic is unencrypted today, and we say so plainly. Here is why "just turn on AES" does not work in a 39-byte packet, what an authentication tag costs in milliseconds of air time, and what a shared channel key actually buys you.',
    },
    body: {
      ru: `Сначала прямо: **сегодня в MeshTRX ничего не шифруется**. Ни голос, ни сообщения, ни файлы, ни координаты в маяках. Любой человек с такой же рацией на том же канале слышит разговор целиком. Соединение телефона с рацией по Bluetooth тоже открыто — PIN там защищает от случайного чужого подключения, а не от прослушивания.

Это стоит знать до того, как вы решите, для чего использовать такую связь. А дальше — почему так вышло и что мы собираемся с этим делать.

## Почему шифрования нет до сих пор

Это была очерёдность, а не забытая задача. Сначала требовалось понять, доходит ли живой голос через LoRa на разумное расстояние и нужно ли это кому-то, кроме автора. Шифровать несуществующую связь смысла не было.

Условие выполнено: связь работает, её гоняют в поле посторонние люди, дальность померена. Значит задача вернулась в очередь. И тут выяснилось, что «включить шифрование» — это не одна задача, а три разные.

**Скрыть содержимое.** Чтобы посторонний слышал шум вместо речи.

**Убедиться, что пакет не подменили.** Шифр сам по себе этого не даёт: злоумышленник может изменить биты, и получатель расшифрует мусор, не заметив подмены. Для этого нужна подпись пакета — код аутентичности.

**Понять, кто свой.** Это уже про ключи: откуда они берутся, как попадают на устройства и что делать, когда одна рация потерялась.

Первая задача решается почти бесплатно. Вторая стоит эфирного времени. Третья — самая сложная, и именно её просят в сообществе: «шифрование на основе обмена ключами, чтобы создавать приватные каналы».

## Бюджет: 39 байт и ни байтом больше

Голосовой пакет MeshTRX — 39 байт: 7 байт заголовка и 32 байта сжатой речи. Отправляется он каждые 80 миллисекунд, и это не наш выбор, а требование кодека.

Вот что происходит с эфиром, если добавлять к пакету криптографические поля:

| Пакет | Время в эфире | Занято канала |
|---|---|---|
| сейчас, 39 байт | 53,4 мс | 67% |
| + подпись 4 байта | 57,0 мс | 71% |
| + подпись 8 байт | 60,5 мс | 76% |
| + подпись 16 байт (полная) | 71,3 мс | 89% |
| + публичный ключ 32 байта | 85,6 мс | 107% |

Последняя строка — не опечатка. Публичный ключ современного алгоритма обмена (X25519) занимает 32 байта, и пакет с ним в канал попросту не помещается: сто семь процентов означают, что передача не успевает закончиться до начала следующей.

Отсюда первые выводы.

**Само шифрование бесплатно.** Поточный шифр не меняет длину: 32 байта речи остаются 32 байтами. Ни одного лишнего байта в эфир.

**Случайное число к каждому пакету передавать нельзя.** Обычно шифру нужен уникальный nonce, и его кладут рядом с данными — это плюс 8–12 байт. Но у нас уже есть номер пакета в заголовке, а он и так уникален в пределах передачи: из номера, адреса отправителя и номера канала счётчик собирается на обеих сторонах одинаково, ничего не передавая.

**Полная подпись слишком дорога.** Шестнадцать байт — это 89% канала под одного говорящего: ретранслятору не остаётся места вовсе, а он и так работает на пределе. Усечённая подпись в 4–8 байт даёт защиту слабее, но реалистичную по цене. Для текста, файлов и команд, где пакеты редкие, можно позволить полную.

## Потери меняют правила

Есть ещё одно ограничение, о котором в обычных системах не думают: у нас теряются пакеты. Половина голоса через ретранслятор не доходит, и это нормальный режим работы.

Значит режимы шифрования, где каждый блок зависит от предыдущего, отпадают: потеряв один пакет, приёмник не расшифрует всё остальное. Нужен режим, где каждый пакет самодостаточен — потеря одного не мешает разобрать следующий.

К счастью, это совпадает с тем, как устроен сам голос: каждый пакет уже самостоятелен, потому что иначе речь разваливалась бы от первой же потери. Номер пакета в заголовке, который сейчас служит для склейки и дедупликации, оказывается готовым счётчиком для шифра.

## Что даст общий ключ на канал — и чего не даст

Минимальный рабочий вариант, с которого мы начнём: один ключ на канал, задаётся в настройках, хранится в памяти устройства.

Что это закрывает:

- сосед с такой же рацией на том же канале больше не слушает ваш разговор;
- случайный человек, купивший модуль и залив прошивку, не попадает в чужую группу по совпадению канала;
- записанный эфир не расшифровывается задним числом без ключа.

Чего это **не** закрывает, и об этом надо говорить прямо:

- **любого, у кого есть ключ.** Ключ один на группу; ушёл человек — ключ надо менять у всех;
- **утечку через устройство.** Физический доступ к плате — это доступ к ключу в памяти;
- **сам факт передачи.** Радиоразведка видит, что кто-то вышел в эфир, на какой частоте и как долго говорил, даже не понимая слов. Против пеленгации шифрование не помогает вообще;
- **подмену, если подписи нет.** Без кода аутентичности чужой может вбросить мусор, который у вас превратится в треск.

Это честный уровень «не подслушает сосед», а не «не прочитает тот, кому очень надо». Путать эти вещи опасно: человек, который поверил в защиту, которой нет, рискует сильнее того, кто знает, что эфир открыт.

## Приватные каналы: почему это отдельная задача

Запрос из сообщества звучал шире общего ключа: обмен ключами и приватные каналы, куда попадают только свои.

Здесь упираемся в ту самую строку таблицы. Классический обмен ключами требует передать 32 байта публичного ключа, а это больше одного пакета. Разбить на два-три — можно, но обмен придётся делать надёжным: с подтверждением, повторами и защитой от вклинивания посредника. В эфире, где половина пакетов теряется, это заметная работа.

Поэтому разумнее развести способы по ситуациям:

- **ключ вводится руками или считывается с экрана.** Скучно, зато надёжно: обмен происходит вне эфира, подслушать нечего. Для группы, которая собирается вместе перед выходом, этого достаточно;
- **обмен при первой встрече по Bluetooth.** Рации рядом, телефон видит обе — ключ передаётся коротким путём, минуя радиоканал;
- **обмен по эфиру** — самый удобный и самый дорогой. Он нужен, когда людей нельзя собрать вместе, и делать его стоит последним, когда простые способы уже работают.

И отдельная деталь, которая связывает шифрование с совсем другой задачей. Чтобы ретрансляторы не повторяли чужой трафик, сети нужны зоны — понимание, какая станция своя. Идентификатор группы, который для этого нужен, и ключ группы — это одно и то же поле, если делать их вместе. Поэтому зоны и шифрование лучше не разводить по разным углам: одна задача даёт второй почти всё, что ей нужно.

## Что дальше

Порядок, который мы считаем правильным: сначала общий ключ на канал с усечённой подписью для голоса и полной для текста и файлов. Потом ввод ключа руками и через экран. Потом группы и зоны на том же идентификаторе. Обмен ключами по эфиру — последним.

Сроков не называем: проект делается по вечерам, и обещать даты было бы враньём. Но пока шифрования нет, мы будем писать об этом прямо — в документации, на странице проекта и здесь.

Как устроена сеть, где всё это будет жить, — в [статье про ретранслятор](/articles/repeater-math/). Что уже умеет продукт — в [документации](/docs/). Спорить о том, каким должен быть обмен ключами, лучше всего в [группе](https://t.me/MeshTRX): эта статья написана во многом по её вопросам.`,
      en: `First, plainly: **nothing in MeshTRX is encrypted today**. Not voice, not messages, not files, not the coordinates in beacons. Anyone with the same radio on the same channel hears the whole conversation. The Bluetooth link between phone and radio is open too — the PIN there protects against someone connecting by accident, not against eavesdropping.

That is worth knowing before you decide what to use this kind of link for. Now, why it turned out this way and what we intend to do about it.

## Why there is still no encryption

This was a matter of order, not a forgotten task. First we had to learn whether live voice actually travels over LoRa at a sensible distance, and whether anyone besides the author needed it. Encrypting a link that does not exist yet makes no sense.

That condition is met: the link works, strangers run it in the field, the range has been measured. So the task came back into the queue. And that is when it turned out that "turn on encryption" is not one task but three.

**Hide the contents.** So an outsider hears noise instead of speech.

**Make sure the packet was not tampered with.** A cipher alone does not do this: an attacker can flip bits, and the receiver will decrypt garbage without noticing. That needs an authentication tag.

**Know who is one of us.** That is about keys: where they come from, how they get onto devices, and what to do when one radio goes missing.

The first is nearly free. The second costs air time. The third is the hardest — and it is exactly what the community asked for: "encryption based on key exchange, so private channels can be created".

## The budget: 39 bytes, not one more

A MeshTRX voice packet is 39 bytes: 7 of header and 32 of compressed speech. It goes out every 80 milliseconds, and that is not our choice but the codec's requirement.

Here is what happens to the air if cryptographic fields are added:

| Packet | Time on air | Channel used |
|---|---|---|
| today, 39 bytes | 53.4 ms | 67% |
| + 4-byte tag | 57.0 ms | 71% |
| + 8-byte tag | 60.5 ms | 76% |
| + 16-byte tag (full) | 71.3 ms | 89% |
| + 32-byte public key | 85.6 ms | 107% |

The last line is not a typo. A modern key-exchange public key (X25519) is 32 bytes, and a packet carrying it simply does not fit the channel: a hundred and seven per cent means the transmission cannot finish before the next one is due.

From which the first conclusions follow.

**The encryption itself is free.** A stream cipher does not change the length: 32 bytes of speech stay 32 bytes. Not a single extra byte on air.

**A random nonce cannot be sent with every packet.** Normally a cipher needs a unique nonce, carried alongside the data — that is 8 to 12 bytes more. But we already have a packet number in the header, and it is unique within a transmission: from that number, the sender address and the channel, both sides derive the same counter without transmitting anything.

**A full tag is too expensive.** Sixteen bytes is 89% of the channel for a single speaker: nothing is left for the repeater, which already runs at its limit. A truncated 4–8 byte tag is weaker but realistic in price. For text, files and commands, where packets are rare, the full tag is affordable.

## Losses change the rules

There is another constraint that ordinary systems never face: our packets get lost. Half the voice through a repeater does not arrive, and that is normal operation.

So modes where each block depends on the previous one are out: lose one packet and the receiver cannot decrypt anything after it. We need a mode where each packet stands alone — losing one does not prevent reading the next.

Fortunately that matches how voice already works: every packet is self-contained, because otherwise speech would fall apart at the first loss. The packet number in the header, which today serves for reassembly and duplicate rejection, turns out to be a ready-made counter for the cipher.

## What a shared channel key buys — and what it does not

The minimum workable option, and where we will start: one key per channel, set in the settings, stored in the device's memory.

What it closes off:

- the neighbour with the same radio on the same channel no longer hears your conversation;
- a random person who bought a module and flashed the firmware does not land in someone else's group by sharing a channel number;
- recorded traffic cannot be decrypted after the fact without the key.

What it does **not** close off, and this must be said plainly:

- **anyone who has the key.** There is one key for the group; when someone leaves, everyone has to change it;
- **a leak through the device.** Physical access to the board is access to the key in its memory;
- **the fact of transmission.** Radio direction finding sees that someone came on air, on what frequency and for how long, without understanding a word. Against triangulation, encryption does not help at all;
- **tampering, if there is no tag.** Without an authentication code an outsider can inject garbage that turns into a crackle at your end.

This is an honest "the neighbour will not overhear" level, not "someone determined will not read it". Confusing the two is dangerous: a person who believes in protection that is not there takes more risk than one who knows the air is open.

## Private channels: a separate problem

The request from the community went beyond a shared key: key exchange and private channels that only your own people join.

Here we hit that table row again. Classic key exchange means transmitting 32 bytes of public key, which is more than one packet. Splitting it across two or three is possible, but the exchange then has to be made reliable: confirmations, retries, and protection against a man in the middle. On air, where half the packets are lost, that is real work.

So it makes sense to separate the methods by situation:

- **the key is typed in or read off a screen.** Boring, but reliable: the exchange happens off air, so there is nothing to overhear. For a group that gathers before heading out, that is enough;
- **exchange on first meeting over Bluetooth.** The radios are side by side and the phone sees both — the key travels the short way, bypassing the radio channel;
- **exchange over the air** — the most convenient and the most expensive. It is needed when people cannot be gathered in one place, and it should be built last, once the simple methods already work.

And one detail that ties encryption to an entirely different task. To stop repeaters forwarding foreign traffic, the network needs zones — a notion of which station is one of ours. The group identifier that requires, and the group key, are the same field if they are built together. So zones and encryption are better not kept in separate corners: one task gives the other almost everything it needs.

## What comes next

The order we consider right: first a shared channel key with a truncated tag for voice and a full one for text and files. Then key entry by hand and from a screen. Then groups and zones on the same identifier. Key exchange over the air, last.

We name no dates: the project is built in the evenings, and promising deadlines would be a lie. But as long as there is no encryption, we will keep saying so plainly — in the documentation, on the project page, and here.

How the network this will live in is built is in [the repeater article](/articles/repeater-math/). What the product can already do is in the [documentation](/docs/). The best place to argue about what key exchange should look like is the [Telegram group](https://t.me/MeshTRX): this article was largely written from its questions.`,
    },
  },
  {
    slug: 'repeater-math',
    date: '2026-09-15',
    title: {
      ru: 'Ретранслятор удваивает дальность и делит эфир',
      en: 'A repeater doubles your range and divides the air',
    },
    summary: {
      ru: 'Как устроена пересылка в LoRa-сети: TTL, память о переданных пакетах и арифметика канала, из которой следует, что три ретранслятора не пронесут голос. Плюс история про счётчик, из-за которого пропадала каждая вторая фраза.',
      en: 'How forwarding works in a LoRa network: TTL, memory of forwarded packets, and the channel arithmetic showing why three repeaters cannot carry voice. Plus the story of a counter that swallowed every second phrase.',
    },
    body: {
      ru: `Ретранслятор кажется простым устройством: услышал пакет — повторил. На этом описания обычно и заканчиваются, а дальше начинается то, что видно только на работающей сети. Ниже — как пересылка устроена в MeshTRX, во что она обходится и почему сеть из трёх ретрансляторов не собирается сама собой.

## Две защиты от лавины

Если ретранслятор просто повторяет всё услышанное, сеть умирает на втором устройстве: два ретранслятора начинают повторять друг друга, и один пакет ходит по кругу, пока кто-нибудь не выключится. Поэтому защит две, и они разные.

**TTL — счётчик жизни пакета.** В заголовке едет число, ретранслятор уменьшает его на единицу, и пакет с нулём дальше не идёт. В MeshTRX начальное значение равно двум: пакет проходит через два ретранслятора и останавливается. Это ограничивает глубину сети, но не защищает от дублей: два ретранслятора, слышащие один и тот же пакет, оба его повторят, и каждый повтор будет законным.

**Память о переданном.** От дублей спасает второе: ретранслятор помнит, что он уже пересылал, и повтор того же самого отбрасывает. Ключ памяти — отправитель, номер пакета и тип. Кольцевой кеш на 128 записей, срок жизни записи — тридцать секунд для текста и файлов и три секунды для голоса.

Почему для голоса срок другой, лучше всего объясняет ошибка, которую мы на этом допустили.

## Как счётчик съедал каждую вторую фразу

Номер голосового пакета — один байт, и он сбрасывался в ноль на каждом нажатии кнопки. Казалось, что так удобнее: приёмник видит нулевой номер и понимает, что началась новая фраза.

Для ретранслятора это выглядело иначе. Он помнил переданные пакеты полминуты. Вы сказали фразу — в его памяти осели номера 0, 1, 2 и дальше. Через десять секунд сказали вторую — и её пакеты пришли с теми же номерами, от того же отправителя, того же типа. Совпадение по всем полям ключа. Вторая фраза отбрасывалась целиком, как дубль.

На стенде, где голос идёт редкими пакетами и потери исключены, это выглядит недвусмысленно:

| | переслано | отброшено |
|---|---|---|
| первая фраза | 10 из 10 | 0 |
| вторая через 5 секунд | **0 из 10** | 10 |
| четвёртая через 40 секунд | 10 из 10 | 0 |

В живой сети эффект прятался за обычными потерями, и жалоба звучала как «через ретранслятор голос доходит через раз» — формулировка, по которой ничего не найдёшь.

Лечится это двумя правками. Номер стал сквозным: начало фразы приёмник узнаёт по отдельному флагу, а не по нулю. И память о голосе сократилась до трёх секунд — пакет живёт в сети доли секунды, трёх достаточно, чтобы отсечь эхо, и мало, чтобы забыть живую речь. Тридцать секунд для голоса опасны ещё и потому, что восьмибитный номер при двенадцати пакетах в секунду обходит круг за двадцать: в длинном разговоре свежий пакет совпал бы со старой записью.

## Арифметика, которую стоит посчитать заранее

Теперь главное — во что пересылка обходится эфиру.

Голосовой пакет MeshTRX — 39 байт. На рабочих параметрах (SF7, полоса 250 кГц, избыточность 4/7) он занимает эфир около 53 миллисекунд. Отправляется такой пакет каждые 80 миллисекунд — этого требует кодек.

Отсюда всё и следует:

| Кто в эфире | Занято канала |
|---|---|
| одна говорящая станция | 67% |
| она же + один ретранслятор | 133% |
| + два ретранслятора | 200% |
| + три ретранслятора | 267% |

Одна станция забирает две трети канала. Один повтор — и передача уже не помещается: на стенде через ретранслятор проходит около половины голосовых пакетов, и это не дефект прошивки, а деление ста тридцати трёх процентов на доступные сто.

Для речи потеря терпима: пропуск в 80 мс слышен как щелчок, а не как дыра в слове. Для файла это было бы катастрофой, поэтому файлы идут с подтверждением и дозапросом потерянных кусков — время растёт, зато доходит всё.

## Почему три ретранслятора не собираются в сеть

В сообществе обсуждают треугольник: три ретранслятора на высотках, ребро четыре километра, между ними прямая видимость. Идея правильная — покрытие получается на порядок больше. Но если поставить три ретранслятора в один канал и включить, произойдёт не сеть, а взаимное глушение: 267 процентов в таблице выше.

Память о переданном здесь не поможет. Она убирает дубли, но не создаёт ёмкость: каждый ретранслятор всё равно обязан повторить чужой пакет хотя бы раз, а канал один на всех.

Значит нужно другое, и оно из двух частей.

**Зоны.** Станция принадлежит своему ретранслятору, и ретранслятор повторяет только своих. Тогда соседний ретранслятор не подхватывает чужой трафик, и лавина исчезает по построению, а не по счастливому стечению обстоятельств. Технически это идентификатор группы в пакете — то же поле, которое понадобится для «свой–чужой» при шифровании.

**Магистраль вне общего канала.** Зоны без связи между собой — это три независимые сети. Соединить их можно тремя способами, и они сильно разной цены:

- **по интернету.** Ретранслятор на высотке почти наверняка окажется в сети — и тогда туннель между ретрансляторами не занимает эфир вообще и не имеет ограничений по ёмкости. Самый дешёвый вариант и самый неспортивный: сеть, которая задумывалась как независимая от инфраструктуры, в этом месте на неё опирается;
- **вторым радиомодулем.** Отдельный приёмопередатчик на другом канале, направленная антенна на соседа. Честный off-grid, но это доработка железа, а не прошивки;
- **разделением по времени на одном радио.** Ретранслятор часть времени слушает магистральный канал. Для текста, позиций и маяков сгодится; для голоса нет — поток непрерывный, и пропуски в нём складываются в шум.

Первый вариант проверяется за вечер, третий — тоже, второй требует железа. Начинать имеет смысл с того, что можно измерить.

## Что из этого следует для практики

Три вывода, которые стоит держать в голове, ставя ретранслятор.

Он увеличивает охват, но не ёмкость. Если в вашей сети и так тесно — голос идёт, файлы ходят, — ретранслятор сделает хуже, а не лучше.

Он полезнее всего там, где иначе связи нет вовсе: на краю слышимости, за домом, в низине. Ровно поэтому его и поднимают на высокую точку — не чтобы усилить, а чтобы увидеть тех, кто друг друга не видит.

И он не бесплатен для тех, кто и так слышит друг друга напрямую: их пакеты он тоже повторит, отняв у канала те самые проценты. Зоны нужны в том числе поэтому.

Как включить ретранслятор и что показывает его страница — в [документации](/docs/). Числа выше получены на стенде из двух плат; замеры в поле, споры о треугольнике и всё остальное — в [группе](https://t.me/MeshTRX).`,
      en: `A repeater looks like a simple device: hear a packet, repeat it. Descriptions usually stop there, and everything interesting starts afterwards — visible only on a working network. Below is how forwarding works in MeshTRX, what it costs, and why a network of three repeaters does not assemble itself.

## Two defences against an avalanche

If a repeater simply repeats everything it hears, the network dies at the second device: two repeaters start repeating each other, and one packet circles until someone switches off. So there are two defences, and they are different.

**TTL — the packet's life counter.** A number rides in the header, each repeater decrements it, and a packet at zero goes no further. In MeshTRX it starts at two: a packet crosses two repeaters and stops. That limits network depth but does nothing about duplicates: two repeaters hearing the same packet will both repeat it, and both repeats are legitimate.

**Memory of what was forwarded.** Duplicates are handled by the second defence: the repeater remembers what it already forwarded and drops a repeat of the same thing. The key is sender, packet number and type. A ring cache of 128 entries, with a lifetime of thirty seconds for text and files and three seconds for voice.

Why voice gets a different lifetime is best explained by the bug we made there.

## How a counter swallowed every second phrase

The voice packet number is one byte, and it was reset to zero on every press of the button. It seemed convenient: the receiver sees a zero and knows a new phrase has started.

To the repeater it looked different. It remembered forwarded packets for half a minute. You said a phrase — numbers 0, 1, 2 and onwards settled in its memory. Ten seconds later you said a second one — and its packets arrived with the same numbers, from the same sender, of the same type. A match on every field of the key. The second phrase was dropped entirely, as a duplicate.

On the bench, where voice is sent as sparse packets and losses are ruled out, it looks unambiguous:

| | forwarded | dropped |
|---|---|---|
| first phrase | 10 of 10 | 0 |
| second, 5 seconds later | **0 of 10** | 10 |
| fourth, 40 seconds later | 10 of 10 | 0 |

In a live network the effect hid behind ordinary losses, and the complaint sounded like "voice gets through the repeater every other time" — a description you cannot search for.

The cure is two changes. The number became continuous: the receiver recognises the start of a phrase by a separate flag, not by a zero. And voice memory shrank to three seconds — a packet lives in the network for fractions of a second, three seconds are enough to cut off the echo and too few to forget live speech. Thirty seconds are dangerous for voice for another reason: an eight-bit number at twelve packets per second wraps around in twenty, so in a long conversation a fresh packet would collide with an old entry.

## The arithmetic worth doing in advance

Now the main thing — what forwarding costs the air.

A MeshTRX voice packet is 39 bytes. At the working parameters (SF7, 250 kHz bandwidth, 4/7 coding rate) it occupies the air for about 53 milliseconds. Such a packet is sent every 80 milliseconds — the codec demands it.

Everything follows from that:

| On air | Channel used |
|---|---|
| one talking station | 67% |
| the same plus one repeater | 133% |
| plus two repeaters | 200% |
| plus three repeaters | 267% |

One station takes two thirds of the channel. One repeat and the transmission no longer fits: on the bench about half the voice packets make it through a repeater, and that is not a firmware defect but a hundred and thirty-three per cent divided into the available hundred.

For speech the loss is tolerable: an 80 ms gap sounds like a click, not a hole in a word. For a file it would be a disaster, which is why files travel with acknowledgements and re-requests for missing pieces — it takes longer, but everything arrives.

## Why three repeaters do not form a network

The community is discussing a triangle: three repeaters on tower blocks, four kilometres a side, line of sight between them. The idea is right — coverage grows by an order of magnitude. But put three repeaters in one channel and switch them on, and what happens is not a network but mutual jamming: the 267 per cent in the table above.

Memory of forwarded packets does not help here. It removes duplicates but creates no capacity: each repeater still has to repeat someone else's packet at least once, and the channel is shared.

So something else is needed, in two parts.

**Zones.** A station belongs to its own repeater, and a repeater repeats only its own. Then the neighbouring repeater does not pick up foreign traffic, and the avalanche disappears by construction rather than by luck. Technically that is a group identifier in the packet — the same field that will be needed for "friend or foe" once encryption arrives.

**A trunk outside the shared channel.** Zones with no link between them are three independent networks. There are three ways to connect them, at very different prices:

- **over the internet.** A repeater on a tower block will almost certainly be on a network — and then a tunnel between repeaters occupies no air at all and has no capacity limit. The cheapest option and the least sporting: a network designed to be independent of infrastructure leans on it at exactly this point;
- **with a second radio module.** A separate transceiver on another channel, a directional antenna pointed at the neighbour. Honest off-grid, but that is hardware work, not firmware;
- **by time-sharing one radio.** The repeater spends part of its time listening to the trunk channel. Fine for text, positions and beacons; not for voice — the stream is continuous, and gaps in it add up to noise.

The first option can be tested in an evening, the third too, the second needs hardware. It makes sense to start with what can be measured.

## What this means in practice

Three conclusions worth keeping in mind when putting up a repeater.

It increases coverage, not capacity. If your network is already crowded — voice flowing, files moving — a repeater will make things worse, not better.

It is most useful where there would otherwise be no link at all: at the edge of coverage, behind a building, down in a hollow. That is exactly why it goes up high — not to amplify, but to see those who cannot see each other.

And it is not free for those who already hear each other directly: it repeats their packets too, taking those same percentages out of the channel. Zones are needed for that reason as well.

How to enable a repeater and what its page shows is in the [documentation](/docs/). The numbers above come from a two-board bench; field measurements, the triangle argument and everything else live in the [Telegram group](https://t.me/MeshTRX).`,
    },
  },
  {
    slug: 'bugs-found-in-the-field',
    date: '2026-09-11',
    title: {
      ru: 'Одна нога микросхемы и пятьдесят восемь децибел',
      en: 'One chip pin and fifty-eight decibels',
    },
    summary: {
      ru: 'Разбор ошибок, которые нашлись только тогда, когда рации попали к людям: усилитель, работавший мимо, вифи, глушивший приём, крестики «не доставлено» на дошедших сообщениях. И о том, почему стенд из двух плат находит не всё.',
      en: 'A look at the bugs that surfaced only once the radios reached real people: an amplifier bypassed entirely, WiFi jamming reception, "not delivered" marks on messages that arrived. And why a two-board bench cannot catch everything.',
    },
    body: {
      ru: `Проект, который живёт на столе разработчика, выглядит работающим. Проверить это можно только одним способом: отдать устройства людям и слушать, что они скажут. Ниже — несколько ошибок, найденных именно так, и то, чем каждая из них поучительна.

## Усилитель, который был не при делах

Самая дорогая находка. На одной из ревизий платы приём работал, передача работала, связь была — но дальность оказывалась заметно хуже ожидаемой.

Причина нашлась в управлении внешним усилителем. У модуля есть нога, которая переключает тракт между передачей и приёмом. В прошивке её выставляли в неправильном состоянии, и сигнал уходил в эфир мимо усилителя. Замер до и после: минус семьдесят восемь децибел против минус двадцати. Разница в пятьдесят восемь децибел — это не «стало получше», это другая рация.

Поучительно здесь то, что никакой ошибки не было видно. Ничего не падало, ничего не выдавало сообщений, связь была. Просто дальность была вчетверо меньше той, что заложена в железо, и списать это можно было на что угодно: на антенну, на застройку, на помехи.

Вывод, который мы вынесли: «работает» и «работает как задумано» — разные вещи, и отличить их можно только замером.

## Вифи, который глушил приём

Ретранслятор поднимает у себя веб-страницу — карту станций, статистику, настройки. Для этого ему нужен вифи, и долгое время он держал сразу два режима: свою точку доступа и подключение к домашней сети.

Из сообщества пришло наблюдение: точка доступа не нужна, если ретранслятор и так подключён к сети, а лишний передатчик рядом с приёмником мешает — в том числе по питанию платы, а не только по эфиру.

Замечание оказалось верным, а цена — выше, чем мы думали. Когда после правки в одной из сборок клиентская часть вифи осталась включённой вхолостую, на стенде это дало наглядную картину: голос через ретранслятор перестал проходить вовсе — ноль пересланных пакетов из двадцати вместо обычных десяти. Убрали лишний передатчик — всё вернулось.

Поучительно здесь то, что подозрение пришло от человека, который наблюдал за устройством в работе, а не от нас. И что подтвердилось оно только цифрами.

## Крестик на дошедшем сообщении

Ошибка, которая не ломает связь, но портит доверие к ней.

Сообщение уходило, доходило до адресата, тот его читал — а у отправителя стоял крестик «не доставлено». Подтверждение о доставке терялось на обратном пути или приходило позже, чем приложение готово было ждать, и оно рисовало отказ.

Исправление получилось не в том, чтобы ждать дольше, а в том, чтобы перестать врать. Теперь у сообщения три состояния вместо двух: доставлено, ушло без подтверждения и не отправлено. Второе — честное «мы не знаем»: в полудуплексном эфире потеряться может и сам ответ. Запоздавшее подтверждение ставит галочку задним числом.

Поучительно здесь то, что интерфейс, который уверенно показывает неправду, хуже интерфейса, который признаёт неопределённость.

## Дубли, шум и прочая мелочь

Несколько находок помельче, но из той же серии — видно их только в живой сети.

**Сообщения двоились**, когда в сети появлялся ретранслятор: он честно повторял чужой пакет, в том числе пакет отправителя, и тот получал собственное сообщение обратно. В чате это выглядело как дубль. Лечится тем, что станция игнорирует пакеты со своим собственным адресом отправителя.

**Недопринятое голосовое доигрывалось шипением.** Если часть пакетов потерялась, на их месте оставались нули, а кодек добросовестно превращал нули в шум — и человек слушал полминуты шипения после короткой фразы. Теперь воспроизводится то, что дошло целым, а хвост с пропусками отбрасывается: короткая фраза лучше фразы с шумом.

**Ретранслятор путался в собственном канале** — вернее, так казалось. На странице в списке стоял один канал, в таблице другой. Разбор показал, что ошибки нет: список показывает то, что выбрал человек, а таблица — реальное состояние. Лечится не кодом, а подписью: текущий канал теперь помечен прямо в списке.

## Чего стенд не находит

У проекта есть стенд: две платы, телефон, шесть десятков проверок — текст, голос, файлы, вызовы, ретранслятор, потери, нагрузка. Он ловит многое и экономит часы.

Но ни одну из описанных выше ошибок он бы сам не нашёл, и причина понятна. На стенде платы стоят в полуметре друг от друга, эфир чистый, питание от стабильного источника, а сценарии — те, которые мы придумали заранее. Дальность там не проверишь, городские помехи не воспроизведёшь, и уж точно не воспроизведёшь ситуацию «человек гуляет с рацией, а вторая станция лежит дома».

Зато стенд отлично делает другое: он подтверждает или опровергает догадку. Почти каждая история выше выглядела одинаково — наблюдение из поля, гипотеза, попытка воспроизвести на столе. Иногда гипотеза не подтверждалась, и это тоже результат: однажды мы были уверены, что команду не слышат рации, которые слушают эфир урывками, — шесть проверок подряд показали, что дело не в этом, и искать пришлось дальше.

Поэтому самое ценное в сообщении из группы — не жалоба, а деталь. «Не работает» проверить нельзя. «На станциях канал поменялся, на ретрансляторе нет» — можно, и на это ушло двадцать минут.

Что проект умеет сегодня — в [документации](/docs/). Что делается сейчас и что отложено — на [странице проекта](/about/). Наблюдения из поля всегда кстати в [группе](https://t.me/MeshTRX).`,
      en: `A project that lives on a developer's desk looks like it works. There is only one way to check: hand the devices to people and listen to what they say. Below are several bugs found exactly that way, and what each of them teaches.

## The amplifier that was doing nothing

The most expensive find. On one board revision reception worked, transmission worked, the link was there — but range was noticeably worse than expected.

The cause was in the external amplifier control. The module has a pin that switches the path between transmit and receive. The firmware drove it into the wrong state, and the signal went on air bypassing the amplifier. Before and after: minus seventy-eight decibels against minus twenty. Fifty-eight decibels of difference is not "a bit better", it is a different radio.

What is instructive here is that no error was visible. Nothing crashed, nothing printed a message, the link was up. Range was simply four times shorter than the hardware allowed, and that could be blamed on anything: the antenna, the buildings, interference.

The lesson we took: "it works" and "it works as designed" are different things, and only a measurement tells them apart.

## The WiFi that jammed reception

The repeater hosts a web page — a map of stations, statistics, settings. That needs WiFi, and for a long time it kept two modes at once: its own access point and a connection to the home network.

An observation came from the community: the access point is not needed when the repeater is already on a network, and an extra transmitter next to a receiver gets in the way — through the board's power rails, not only through the air.

The remark was right, and the cost was higher than we thought. When, after a refactor, one build left the client side of WiFi running with nothing to connect to, the bench showed it plainly: voice through the repeater stopped getting through entirely — zero forwarded packets out of twenty instead of the usual ten. Remove the idle transmitter and everything came back.

What is instructive here is that the suspicion came from someone watching the device at work, not from us. And that it was confirmed only by numbers.

## A cross on a message that arrived

A bug that does not break the link but damages trust in it.

A message went out, reached its recipient, who read it — and the sender saw a "not delivered" cross. The delivery acknowledgement got lost on the way back, or arrived later than the app was willing to wait, and the app drew a failure.

The fix was not to wait longer but to stop lying. A message now has three states instead of two: delivered, sent without acknowledgement, and not sent. The second is an honest "we do not know": in half-duplex air the reply itself can be lost. A late acknowledgement ticks the message off retroactively.

What is instructive here is that an interface confidently showing a falsehood is worse than one admitting uncertainty.

## Duplicates, noise and other small things

A few smaller finds, from the same family — visible only in a live network.

**Messages doubled** once a repeater appeared: it dutifully repeated other stations' packets, including the sender's own, and the sender received its own message back. In the chat that looked like a duplicate. The cure is for a station to ignore packets carrying its own sender address.

**A partially received voice message played out as hiss.** If some packets were lost, zeroes stayed in their place, and the codec faithfully turned zeroes into noise — so a short phrase was followed by half a minute of hissing. Now what arrived intact is played and the gap-ridden tail is dropped: a short phrase beats a phrase with noise.

**The repeater seemed confused about its own channel.** On its page the drop-down showed one channel and the table another. Investigation showed there was no bug: the list shows what a person picked, the table shows the real state. The fix was not code but a label — the current channel is now marked in the list itself.

## What a bench cannot find

The project has a test bench: two boards, a phone, some sixty checks — text, voice, files, calls, repeater, losses, load. It catches a lot and saves hours.

But it would not have found a single one of the bugs above on its own, and the reason is clear. On the bench the boards sit half a metre apart, the air is clean, power comes from a stable supply, and the scenarios are the ones we thought of in advance. You cannot test range there, cannot reproduce city interference, and certainly cannot reproduce "a person is out walking with one radio while the other sits at home".

What the bench does superbly is something else: it confirms or refutes a hypothesis. Almost every story above followed the same shape — an observation from the field, a hypothesis, an attempt to reproduce it on the desk. Sometimes the hypothesis failed, and that is a result too: we were once sure the command was being missed by radios that listen to the air in short bursts — six consecutive checks showed that was not it, and the search had to continue.

That is why the most valuable thing in a message from the group is not the complaint but the detail. "It does not work" cannot be checked. "The nodes changed channel, the repeater did not" can be — and that took twenty minutes.

What the project can do today is in the [documentation](/docs/). What is being worked on and what is deferred is on the [project page](/about/). Observations from the field are always welcome in the [Telegram group](https://t.me/MeshTRX).`,
    },
  },
  {
    slug: 'channels-and-interference',
    date: '2026-09-11',
    title: {
      ru: 'Тесный эфир: помехи, каналы и как перевести всю группу разом',
      en: 'A crowded band: interference, channels, and moving a whole group at once',
    },
    summary: {
      ru: 'Диапазон 868 МГц в городе делят домофоны, счётчики и сигнализации. Как рация научилась искать тихий канал, почему сменить его нужно сразу у всех и какие ошибки мы допустили, пока это делали.',
      en: 'In a city the 868 MHz band is shared with door phones, meters and alarms. How the radio learned to find a quiet channel, why everyone has to move at once, and the mistakes we made getting there.',
    },
    body: {
      ru: `Дальность зависит не только от мощности и антенны. Есть третий участник, о котором вспоминают в последнюю очередь, — сам эфир, в котором, кроме вас, живёт много кто ещё.

## Кто ещё живёт на 868 МГц

Диапазон 863–870 МГц открыт для маломощных устройств, и этим пользуются не только радиолюбители. Там работают домофоны, метеостанции, счётчики воды и электричества, автомобильные сигнализации, датчики в квартирах. В городе это заметный фон, и он неравномерный: на одной частоте тихо, на соседней постоянно кто-то щёлкает.

Пока канал выбирался наугад, проверить это было нечем. Поэтому в прошивку добавили простую вещь: рация проходит по всем двадцати трём каналам, слушает эфир и показывает уровень шума на каждом.

Одна деталь оказалась важной. Уровень берётся как самый громкий отсчёт серии, а не как средний. Помеха редко держится ровно: она бьёт вспышками, и по среднему канал выглядит тихим, а на деле раз в секунду в нём проезжает чужой пакет — ровно то, что убивает голос. Максимум честнее.

Первый же замер на столе показал разницу в пятнадцать децибел между самым тихим и самым шумным каналом. Это много: пятнадцать децибел — это разница между связью и её отсутствием на дальней дистанции.

## Почему один канал ничего не решает

Дальше выяснилось неприятное. Найти тихий канал мало — на него нужно перевести всех: рации, которые в этот момент у других людей, и ретранслятор, который может стоять на крыше или в другом конце города.

Из сообщества это прозвучало прямо: канал определяется хорошо, но раздать его на все станции неоткуда, приходится обходить каждую руками. А когда часть станций гуляет с людьми по городу, обойти их невозможно в принципе.

## Команда, которая идёт по воздуху

Решение выглядит просто: рация рассылает в эфир команду «всем перейти на такой-то канал через десять секунд». Все, кто её услышал, включая ретранслятор, уходят одновременно.

Просто оно только на словах. Вот что пришлось учесть.

**Команда идёт тремя копиями подряд.** Эфир полудуплексный, одиночный пакет теряется легко, а цена потери здесь высокая: не услышавшая станция останется на старом канале и для группы исчезнет.

**Отправитель уходит последним.** Пока он на прежнем канале, команду ещё можно повторить. Уйди он первым — и отставшие остались бы без связи с тем, кто их звал.

**Отставшему нужен путь назад.** Станция, перешедшая на новый канал и никого там не услышавшая, сама возвращается на прежний. Иначе одна потерянная команда означала бы поход к устройству руками.

## Где мы ошиблись

Дальше началось то, ради чего эту статью и стоит читать.

**Ошибка первая: страховка, которая вредит.** Возврат «не слышу никого — ухожу обратно» задумывался для одинокой рации. Но ретранслятор молчит всегда: он только повторяет чужое, сам в эфир не лезет. Он честно уходил вместе с группой, две минуты никого не слышал — и возвращался. Станции оставались на новом канале, ретранслятор на старом, и связь рвалась ровно в той точке, ради которой ретранслятор и поднят.

Из группы это пришло одной фразой: на станциях канал поменялся, на ретрансляторе нет.

**Ошибка вторая: неверная мерка тишины.** Те же две минуты стояли и у обычной рации. А группа может просто молчать: маяк присутствия идёт раз в пять минут, и за две минуты «тишина» означает всего лишь, что никто не разговаривал. Станции по краям сети успевали счесть себя одинокими и уходили обратно — со стороны это выглядело как «работает через раз».

Теперь окно десять минут, а каждая станция после перехода трижды отмечается в эфире с разбросом по времени, чтобы соседи её услышали. Ретранслятор, получивший команду с собственной страницы, не возвращается вовсе: человек нажал кнопку осознанно.

**Ошибка третья, техническая.** Смена частоты не возобновляет приём сама по себе — после неё нужна явная команда. Без неё рация уходила на новый канал глухой: передавать могла, слышать нет. И отдельно: номер канала хранился в двух местах, в радиомодуле и в основной программе, и ретранслятор переключался по-настоящему, а на странице и в приложении показывал старый.

## Что из этого следует

Главный вывод не про каналы. Он про страховки.

Механизм «если что-то пошло не так, вернись в прежнее состояние» кажется безобидным и добавляется легко. Но он срабатывает по косвенному признаку — по тишине, по таймауту, по отсутствию ответа, — и признак этот легко спутать с нормой. Молчащая группа выглядит как потерянная группа. Ретранслятор, который по своей природе молчит, выглядит потерянным всегда.

Поэтому вторая версия возврата устроена иначе: разные сроки для разных ролей, явная отметка в эфире после перехода и полный отказ от возврата там, где решение принял человек.

Подробности о поиске тихого канала и смене канала у всех — в [документации](/docs/). Что происходит в проекте сейчас — в [группе](https://t.me/MeshTRX).`,
      en: `Range does not depend only on power and antenna. There is a third participant, usually remembered last — the air itself, which you share with a lot of other things.

## Who else lives on 868 MHz

The 863–870 MHz band is open to low-power devices, and it is not only radio amateurs who use it. Door phones, weather stations, water and electricity meters, car alarms, apartment sensors all live there. In a city that is a noticeable background, and an uneven one: one frequency is quiet, the next one has someone clicking away constantly.

While the channel was chosen by guesswork, there was no way to check. So the firmware got a simple addition: the radio walks through all twenty-three channels, listens, and shows the noise level on each.

One detail turned out to matter. The level is taken as the loudest sample of a series, not the average. Interference rarely holds steady: it comes in bursts, and by the average a channel looks quiet while in reality someone's packet drives through it once a second — exactly what kills voice. The maximum is more honest.

The very first bench measurement showed a fifteen-decibel spread between the quietest and the noisiest channel. That is a lot: fifteen decibels is the difference between having a link and not having one at distance.

## Why one channel solves nothing

Then came the unpleasant part. Finding a quiet channel is not enough — everyone has to move to it: radios currently in other people's hands, and the repeater, which may sit on a roof or across town.

The community put it plainly: the quiet channel is detected fine, but there is no way to hand it out to all the nodes, so each one has to be set by hand. And when some of the stations are out walking with people, setting them by hand is impossible in principle.

## A command that travels by air

The solution sounds simple: the radio broadcasts "everyone move to channel N in ten seconds". Everyone who hears it, the repeater included, moves at the same time.

Simple in words only. Here is what had to be accounted for.

**The command goes out as three copies in a row.** The air is half-duplex, a single packet is easily lost, and the cost of losing this one is high: a station that misses it stays on the old channel and disappears from the group.

**The sender leaves last.** While it is still on the old channel, the command can be repeated. Had it left first, stragglers would have lost contact with the very station that called them.

**A straggler needs a way back.** A station that moved to the new channel and heard nobody there returns to the old one on its own. Otherwise one lost command would mean walking up to the device.

## Where we got it wrong

Now for the part actually worth reading.

**Mistake one: a safeguard that harms.** The "I hear nobody, I am going back" rule was designed for a lone radio. But a repeater is always silent: it only repeats other people's traffic and never speaks for itself. It dutifully moved with the group, heard nobody for two minutes — and went back. The stations stayed on the new channel, the repeater on the old one, and the link broke at exactly the point the repeater was raised for.

From the group this arrived as a single sentence: the nodes changed channel, the repeater did not.

**Mistake two: the wrong measure of silence.** The same two minutes applied to ordinary radios. But a group can simply be quiet: the presence beacon goes out once every five minutes, so two minutes of "silence" only means nobody was talking. Stations at the edges of the network had time to decide they were alone and went back — which looked from outside like "it works every other time".

The window is now ten minutes, and after moving, every station announces itself three times with a random spread so that neighbours hear it. A repeater that received the command from its own page does not go back at all: a person pressed that button deliberately.

**Mistake three, a technical one.** Changing frequency does not resume reception by itself — it needs an explicit command afterwards. Without it the radio arrived on the new channel deaf: able to transmit, unable to hear. And separately: the channel number was kept in two places, in the radio module and in the main program, so the repeater really did switch while its page and the app still showed the old channel.

## What follows from this

The main lesson is not about channels. It is about safeguards.

A "if something went wrong, return to the previous state" mechanism looks harmless and is easy to add. But it fires on an indirect signal — silence, a timeout, a missing reply — and that signal is easy to confuse with normality. A quiet group looks like a lost group. A repeater, silent by its very nature, looks lost permanently.

So the second version of the fallback works differently: different timeouts for different roles, an explicit announcement on air after the move, and no fallback at all where a person made the decision.

Details on finding a quiet channel and moving everyone at once are in the [documentation](/docs/). What is happening in the project right now is in the [Telegram group](https://t.me/MeshTRX).`,
    },
  },
  {
    slug: 'range-in-the-field',
    date: '2026-09-11',
    title: {
      ru: 'Пять километров на бумаге, два в поле',
      en: 'Five kilometres on paper, two in the field',
    },
    summary: {
      ru: 'Первые замеры дальности на живых рациях: устойчивый голос до двух километров, текст дальше. Почему голос сдаётся раньше текста, что меняет ретранслятор и почему сравнение с сетями для датчиков нечестное.',
      en: 'First range measurements on live radios: solid voice up to two kilometres, text further out. Why voice gives up before text does, what a repeater changes, and why comparing this to sensor networks is unfair.',
    },
    body: {
      ru: `Любой проект на LoRa начинается с цифры дальности, и почти всегда эта цифра взята из идеальных условий. У MeshTRX в описании стоит пять километров — прямая видимость, чистый эфир, ничего между антеннами. Эта статья о том, что показали первые замеры в поле, когда рации попали к людям, которые ходят с ними по городу.

## Что получилось на самом деле

Первый развёрнутый отчёт из сообщества выглядел так: устойчивая голосовая связь до двух километров, дальше двух — проходят текстовые сообщения. Голос при этом переставал доходить раньше — около километра в плотной застройке, при том что текст на том же маршруте ходил почти до двух.

Это не разочарование, это полезное знание. Пять километров остаются правдой для прямой видимости — но именно для неё. В городе, между домами, с рацией в кармане расклад другой, и честнее говорить о нём.

## Почему голос сдаётся раньше текста

Разница не в мощности и не в антенне — в том, как устроена передача.

Текстовое сообщение это один пакет. Ему нужно одно удачное окно в эфире. Не дошло — приложение повторит через несколько секунд, потом ещё раз, и рано или поздно попадёт в момент, когда помеха отступила. Человек этого даже не замечает: сообщение приходит с задержкой в несколько секунд.

Голос так не умеет. Речь идёт потоком: двенадцать пакетов в секунду, и каждый должен успеть дойти вовремя. Повторить нельзя — пока повтор долетит, момент уже прошёл. Десяток пакетов подряд должен пройти через тот же эфир, в котором текст пробивался с третьей попытки.

Отсюда правило, которое стоит помнить: там, где голос уже не проходит, текст обычно ещё работает. И наоборот — если пропал и текст, дело не в голосе, а в связи как таковой.

## Про сравнение с сетями для датчиков

В той же переписке прозвучало наблюдение: соседние узлы на другой прошивке, тоже на LoRa, добивают дальше. Это правда, и причина понятна.

Сети для датчиков по умолчанию работают на медленных режимах — большой коэффициент расширения, узкая полоса. Чем медленнее передача, тем дальше её слышно: это физика, а не качество прошивки. Плата за медленность — пропускная способность. На таком режиме в эфир пролезает короткое сообщение раз в несколько секунд, и никакой речи там быть не может.

MeshTRX работает на быстром режиме именно потому, что несёт голос. Голос требует канала, канал требует скорости, скорость отнимает дальность. Это не недоработка, а выбор, сделанный сознательно, и его стоит знать заранее.

Дальний режим для текста — отдельная передача, медленная и бьющая дальше, — в планах есть. Он не заменит основной, а дополнит его: текст уйдёт дальше там, где голос уже не живёт.

## Что делает ретранслятор

Ретранслятор удваивает путь: рация добивает до него, он добивает дальше. На практике это подтвердилось — голос через ретранслятор начал доходить туда, куда напрямую не доходил.

Но у ретранслятора есть цена, и о ней говорят редко. Он работает в том же канале и в том же полудуплексе, что и все: пока он повторяет чужой пакет, он не слышит следующий. На стенде это видно в числах — из двадцати голосовых пакетов через ретранслятор проходит около половины. Для речи это терпимо: пропуск в восемьдесят миллисекунд слышен как щелчок, а не как дыра. Для файла было бы катастрофой, поэтому файлы через ретранслятор идут с подтверждением и дозапросом потерянных кусков.

Вывод простой: ретранслятор увеличивает охват, но не увеличивает ёмкость эфира. Он полезен там, где иначе связи нет вовсе.

## Что дальше

Замеры продолжаются, и следующий шаг очевиден: направленные антенны и высокая точка. Ретранслятор на десятом этаже в центре города, в прямой видимости — это совсем другой расклад, чем ретранслятор на столе.

Дальше встаёт вопрос, который уже обсуждают в сообществе: как связать несколько ретрансляторов между собой так, чтобы каждый обслуживал своих и не мешал чужим. Это не решается одной настройкой — нужны зоны, привязка станций к своему ретранслятору и отдельный путь между ретрансляторами. Такая работа стоит в планах, и начинать её имеет смысл после того, как замеры с высокой точкой покажут реальные расстояния.

Замеры важнее предположений. Именно поэтому эта статья написана по числам из поля, а не по расчётам.

Как MeshTRX устроен целиком — в [документации](/docs/). Что сделано и что отложено — на [странице проекта](/about/). Замеры, наблюдения и споры — в [группе](https://t.me/MeshTRX).`,
      en: `Every LoRa project starts with a range figure, and that figure almost always comes from ideal conditions. MeshTRX claims five kilometres: line of sight, clean air, nothing between the antennas. This article is about what the first field measurements showed once the radios reached people who carry them around a city.

## What actually happened

The first detailed report from the community read like this: solid voice up to two kilometres, and beyond two, text still gets through. Voice gave up earlier — around a kilometre in dense housing, while text on the same route worked almost out to two.

That is not a disappointment, it is useful knowledge. Five kilometres remains true for line of sight — but only for line of sight. In a city, between buildings, with the radio in a pocket, the numbers are different, and it is more honest to talk about those.

## Why voice gives up before text

The difference is not power or antenna. It is how the transmission works.

A text message is one packet. It needs one good window in the air. Did not make it? The app retries a few seconds later, then again, and sooner or later it lands in a moment when the interference stepped back. The person barely notices: the message arrives a few seconds late.

Voice cannot do that. Speech is a stream: twelve packets a second, each of which must arrive on time. Retrying is pointless — by the time the retry lands, the moment is gone. A dozen packets in a row must cross the same air that text was pushing through on its third attempt.

Hence a rule worth remembering: where voice no longer gets through, text usually still works. And the other way round — if text is gone too, the problem is not voice but the link itself.

## About comparisons with sensor networks

The same discussion raised a point: neighbouring nodes running different firmware, also on LoRa, reach further. That is true, and the reason is clear.

Sensor networks default to slow modes — a high spreading factor, a narrow band. The slower the transmission, the further it is heard: that is physics, not firmware quality. The price of slowness is throughput. In such a mode a short message squeezes into the air every few seconds, and speech is simply out of the question.

MeshTRX runs a fast mode precisely because it carries voice. Voice needs bandwidth, bandwidth needs speed, speed costs range. That is not an oversight but a deliberate choice, and one worth knowing up front.

A long-range mode for text — a separate, slower transmission that reaches further — is on the roadmap. It will not replace the main mode but complement it: text will travel to places where voice no longer lives.

## What a repeater changes

A repeater doubles the path: the radio reaches it, and it reaches further. In practice this was confirmed — voice through the repeater started arriving where it did not arrive directly.

But a repeater has a cost that is rarely mentioned. It works in the same channel and the same half-duplex as everyone else: while it repeats someone's packet, it cannot hear the next one. On the test bench this shows up in numbers — of twenty voice packets, about half make it through the repeater. For speech that is tolerable: an eighty-millisecond gap sounds like a click, not a hole. For a file it would be a disaster, which is why files travel with acknowledgements and re-requests for the missing pieces.

The conclusion is simple: a repeater increases coverage, not the capacity of the air. It helps where there would otherwise be no link at all.

## What comes next

Measurements continue, and the next step is obvious: directional antennas and a high vantage point. A repeater on the tenth floor in the city centre, in line of sight, is an entirely different proposition from a repeater on a desk.

Beyond that lies a question the community is already discussing: how to link several repeaters so that each serves its own stations without interfering with the others. That is not one setting — it needs zones, stations bound to their own repeater, and a separate path between repeaters. Such work is on the roadmap, and it makes sense to start it after measurements from a high point show the real distances.

Measurements beat assumptions. That is exactly why this article is written from field numbers rather than calculations.

How MeshTRX works as a whole is in the [documentation](/docs/). What is done and what is deferred is on the [project page](/about/). Measurements, observations and arguments happen in the [Telegram group](https://t.me/MeshTRX).`,
    },
  },
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
