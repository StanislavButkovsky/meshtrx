export const SITE = {
  name: 'MeshTRX',
  tagline: 'Off-grid голосовая связь через LoRa mesh-сеть',
  description: 'Голосовая связь, сообщения и файлы без интернета и сотовых сетей. Работает на Heltec V3 и V4 + Android.',
  url: 'https://meshtrx.com',
  urlRu: 'https://meshtrx.ru',
  github: 'https://github.com/StanislavButkovsky/meshtrx',
  licenseUrl: 'https://github.com/StanislavButkovsky/meshtrx/blob/master/LICENSE',
  license: 'CC BY-NC 4.0',
  telegram: 'https://t.me/MeshTRX',
  telegramEn: 'https://t.me/MeshTRX_en',
  habr: 'https://habr.com/ru/articles/1098348/',
  // Донаты: CloudTips принимает российские карты и СБП. Для зарубежных
  // способа пока нет — поэтому на английском сайте блок не показываем.
  donate: 'https://pay.cloudtips.ru/p/67ad019d',
  // Русская группа — t.me/MeshTRX, английская — t.me/MeshTRX_en; обсуждения
  // на GitHub — для тех, кто ищет ответы поиском.
  discussions: 'https://github.com/StanislavButkovsky/meshtrx/discussions',
};

export const VERSION = {
  app: '4.4.30',
  // Приложение и прошивка живут своей жизнью и нумеруются раздельно:
  // подставлять прошивке свежую дату, когда менялось только приложение, —
  // значит гнать людей перепрошивать рации впустую, и наоборот.
  firmware: '4.4.35',
  firmwareDate: '2026-10-25',
  date: '2026-10-26',
};

export const FEATURES = [
  {
    title: 'Голосовая связь',
    description: 'PTT голос через LoRa кодеком Codec2 на 3200 бит/с. Разборчиво и без интернета.',
    icon: 'mic',
  },
  {
    title: 'Сообщения',
    description: 'Текстовые сообщения с гарантированной доставкой и подтверждением.',
    icon: 'message',
  },
  {
    title: 'Передача файлов',
    description: 'Отправка файлов через mesh-сеть с разбиением на фрагменты.',
    icon: 'file',
  },
  {
    title: 'Карта и радар',
    description: 'GPS-позиции всех участников на карте. Радар ближайших узлов.',
    icon: 'map',
  },
  {
    title: 'Групповые вызовы',
    description: 'Голосовые вызовы на весь канал или адресный вызов конкретного узла.',
    icon: 'call',
  },
  {
    title: 'Ретранслятор',
    description: 'Любой узел может работать как ретранслятор, увеличивая покрытие сети.',
    icon: 'relay',
  },
  {
    title: 'Шифрование эфира',
    description: 'Кодовое слово группы закрывает голос и текст. Ключ выводит сама рация, отпечаток сверяют вслух.',
    icon: 'lock',
  },
  {
    title: 'Работа с каналами',
    description: 'Поиск самого тихого канала из 23 и перевод всей группы на него одной командой по эфиру.',
    icon: 'tune',
  },
];

export const HARDWARE = {
  name: 'Heltec WiFi LoRa 32 V3 / V4',
  chip: 'ESP32-S3',
  lora: 'SX1262',
  power: '22 dBm',
  ble: 'BLE 5.0',
  battery: 'Li-Po 3.7V',
  antenna: 'IPEX / SMA',
};

// Поддерживаются обе платы, но прошивка у них разная — и это не мелочь:
// чужая запустится, а усилителем будет управлять не тот вывод. На сайте это
// приходится повторять, потому что берут файл по названию платы, не заметив
// ревизию: она напечатана мелким шрифтом у разъёма антенны.
export const BOARDS = [
  { board: 'V3', chipKey: 'hw.board.v3', file: 'firmware-v3' },
  { board: 'V3 433 МГц', chipKey: 'hw.board.v3_433', file: 'firmware-v3-433' },
  { board: 'V4 rev 4.2', chipKey: 'hw.board.v42', file: 'firmware-v4' },
  { board: 'V4 rev 4.3', chipKey: 'hw.board.v43', file: 'firmware-v4.3' },
] as const;

export const NAV_LINKS = [
  { href: '/', label: 'Home' },
  { href: '/download/', label: 'Download' },
  { href: '/ios/', label: 'iOS' },
  { href: '/flash/', label: 'Flash' },
  { href: '/docs/', label: 'Docs' },
  { href: '/articles/', label: 'Articles' },
  { href: '/about/', label: 'About' },
];

export const VERSION_IOS = {
  app: '1.0.3',
  date: '2026-10-26',
};

export const DOWNLOAD_LINKS = {
  // Версия в имени файла — не украшение: браузеры и мессенджеры отдают
  // «тот же» файл из кеша, и люди неделю ставили старую сборку, будучи
  // уверенными, что скачали новую. Разное имя такой ошибки не допускает.
  apk: `/downloads/meshtrx-${VERSION.app}.apk`,
  ipa: `/downloads/meshtrx-${VERSION_IOS.app}.ipa`,
  firmware: `/downloads/firmware-v3-${VERSION.firmware}.bin`,
  firmwareV4: `/downloads/firmware-v4-${VERSION.firmware}.bin`,
  firmwareV43: `/downloads/firmware-v4.3-${VERSION.firmware}.bin`,
  // Платы V3 в исполнении на 433 МГц: своя сетка каналов и предел мощности
  firmwareV3_433: `/downloads/firmware-v3-433-${VERSION.firmware}.bin`,
};
