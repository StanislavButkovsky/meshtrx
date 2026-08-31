export const SITE = {
  name: 'MeshTRX',
  tagline: 'Off-grid голосовая связь через LoRa mesh-сеть',
  description: 'Голосовая связь, сообщения и файлы без интернета и сотовых сетей. Работает на Heltec V3 + Android.',
  url: 'https://meshtrx.com',
  urlRu: 'https://meshtrx.ru',
  github: 'https://github.com/StanislavButkovsky/meshtrx',
  licenseUrl: 'https://github.com/StanislavButkovsky/meshtrx/blob/master/LICENSE',
  license: 'CC BY-NC 4.0',
  telegram: 'https://t.me/MeshTRX',
};

export const VERSION = {
  app: '4.4.5',
  // Прошивка живёт своей жизнью: в 4.4.5 менялось только приложение, и
  // подставлять ей свежую дату — значит гнать людей перепрошивать рации
  // впустую.
  firmware: '4.4.4',
  firmwareDate: '2026-08-28',
  date: '2026-08-31',
};

export const STATS = [
  { value: '5+', unit: 'км', label: 'Дальность связи', hint: 'С ретранслятором — ещё дальше' },
  { value: '23', unit: 'кан.', label: 'LoRa каналов' },
  { value: '160', unit: 'мс', label: 'Задержка голоса' },
  { value: '3200', unit: 'bps', label: 'Codec2 битрейт' },
];

export const FEATURES = [
  {
    title: 'Голосовая связь',
    description: 'PTT голос через LoRa с кодеком Codec2. Задержка ~160ms, слышно чётко.',
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
];

export const HARDWARE = {
  name: 'Heltec WiFi LoRa 32 V3',
  chip: 'ESP32-S3',
  lora: 'SX1262',
  freq: '868 МГц',
  power: '22 dBm',
  ble: 'BLE 5.0',
  battery: 'Li-Po 3.7V',
  antenna: 'IPEX / SMA',
};

export const NAV_LINKS = [
  { href: '/', label: 'Home' },
  { href: '/download/', label: 'Download' },
  { href: '/flash/', label: 'Flash' },
  { href: '/docs/', label: 'Docs' },
  { href: '/about/', label: 'About' },
];

export const DOWNLOAD_LINKS = {
  // Версия в имени файла — не украшение: браузеры и мессенджеры отдают
  // «тот же» файл из кеша, и люди неделю ставили старую сборку, будучи
  // уверенными, что скачали новую. Разное имя такой ошибки не допускает.
  apk: `/downloads/meshtrx-${VERSION.app}.apk`,
  firmware: `/downloads/firmware-v3-${VERSION.firmware}.bin`,
  firmwareV4: `/downloads/firmware-v4-${VERSION.firmware}.bin`,
  firmwareV43: `/downloads/firmware-v4.3-${VERSION.firmware}.bin`,
};
