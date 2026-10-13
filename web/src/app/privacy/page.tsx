'use client';

import { useLanguage } from '@/components/LanguageProvider';

// Политика конфиденциальности сайта и приложений MeshTRX. Нужна магазинам
// (RuStore, App Store) и людям: приложение просит доступ к микрофону,
// местоположению и Bluetooth, и человек вправе знать, куда это уходит.
//
// Пишется по фактам кода, а не по шаблону: своих серверов для данных людей
// у проекта нет, всё живёт на телефоне и уходит только по радио на рации
// других людей. Поменяется поведение приложения — поменять и этот текст.

const EDITION = { ru: '13 октября 2026', en: 'October 13, 2026' };
const EMAIL = 'stanislav.butkovskiy@mail.ru';

type Section = { h: string; p: string[] };

const RU: Section[] = [
  {
    h: '1. Общие положения',
    p: [
      '1.1. Настоящая Политика в отношении обработки персональных данных (далее — Политика) составлена в соответствии с Федеральным законом от 27.07.2006 № 152-ФЗ «О персональных данных» и определяет, какие данные обрабатывает Бутковский Станислав Витальевич (далее — Оператор) и как они защищены.',
      '1.2. Политика применяется к сайтам https://meshtrx.ru и https://meshtrx.com (далее — Сайт), мобильным приложениям MeshTRX для Android и iOS и настольному клиенту (далее — Приложение).',
      '1.3. Политика опубликована в свободном доступе по адресу https://meshtrx.ru/privacy/.',
    ],
  },
  {
    h: '2. Главное коротко',
    p: [
      'У MeshTRX нет учётных записей и нет серверов, на которых хранились бы ваши сообщения, голос, контакты или координаты. Всё, что вы видите в Приложении, хранится только на вашем телефоне. Голос, текст, файлы и координаты уходят по радио на рации других людей — только когда вы сами говорите, отправляете или включили маяк. Оператор этих данных не получает.',
    ],
  },
  {
    h: '3. Какие данные и для чего обрабатываются',
    p: [
      '3.1. Сайт. Сайт не использует cookie для отслеживания и не подключает сервисы веб-аналитики. Веб-сервер ведёт стандартный журнал запросов: IP-адрес, время запроса, запрошенный адрес страницы или файла, сведения о браузере. Цель — работа и защита Сайта от сбоев и атак. Журнал хранится не более 14 дней и затем удаляется автоматически. Сервер расположен в России.',
      '3.2. Приложение — данные на телефоне. Переписка, голосовые сообщения, полученные и отправленные файлы, список услышанных станций, история вызовов, позывной и настройки хранятся только в памяти телефона. Срок хранения переписки и файлов задаётся в настройках Приложения; всё удаляется кнопкой «Очистить чат» или удалением Приложения.',
      '3.3. Приложение — передача по радио. Приложение работает через радиомодуль LoRa, подключённый к телефону по Bluetooth. По вашему действию модуль передаёт в эфир: голос — только пока нажата кнопка передачи или работает голосовая активация; текстовые сообщения, фотографии и файлы — только те, что вы отправили; ваш позывной; координаты вашего местоположения — в периодическом маяке, если вы разрешили Приложению доступ к местоположению. Эти данные получают рации других пользователей в зоне связи. Оператор их не получает и не хранит.',
      '3.4. Радиоэфир общедоступен: без ключа шифрования переданное может принять любой с совместимым приёмником. Приложение позволяет задать общий ключ группы (шифрование AES-256); тогда голос и текст могут прочитать только рации с тем же ключом. Решение о шифровании и о передаче координат принимает пользователь.',
      '3.5. Приложение — обращения в интернет. Раз в сутки и по кнопке «Проверить обновления» Приложение запрашивает с Сайта файл https://meshtrx.ru/latest.json с номерами последних версий; при этом сервер видит IP-адрес (п. 3.1). Карта в Android-версии загружает изображения местности с серверов OpenStreetMap (OpenStreetMap Foundation): им передаются IP-адрес и границы показываемого участка карты. В iOS-версии карта работает через Apple MapKit на условиях Apple. Другие данные Приложение в интернет не отправляет, рекламы и аналитики в нём нет.',
      '3.6. Оператор не обрабатывает специальные категории персональных данных и биометрические персональные данные и не принимает решений, порождающих юридические последствия, на основании исключительно автоматизированной обработки.',
    ],
  },
  {
    h: '4. Разрешения Приложения',
    p: [
      'Bluetooth — найти радиомодуль и держать с ним связь. Микрофон — голосовая связь и голосовые сообщения, только во время передачи. Местоположение — ваша точка на карте и радаре и координаты в маяке для других участников; на Android старых версий оно также требуется системой для поиска Bluetooth-устройств. Фото и файлы — только те, что вы сами выбрали для отправки. Уведомления — о входящих сообщениях и вызовах, когда Приложение свёрнуто. Работа в фоне — связь с радиомодулем, пока телефон в кармане; при этом в шторке показано постоянное уведомление. Отказ от любого разрешения отключает только связанную с ним функцию.',
    ],
  },
  {
    h: '5. Передача третьим лицам и защита',
    p: [
      '5.1. Оператор не продаёт персональные данные и не передаёт их третьим лицам, кроме случаев, предусмотренных законом. Карты загружаются напрямую с серверов OpenStreetMap или Apple (п. 3.5), Оператор в этом не участвует.',
      '5.2. Сайт работает только по HTTPS, доступ к серверу ограничен. Данные Приложения не покидают телефон иначе, чем по радио по вашему действию, и могут быть защищены ключом шифрования группы.',
    ],
  },
  {
    h: '6. Права пользователя',
    p: [
      '6.1. Пользователь вправе получать информацию об обработке своих данных, требовать их уточнения, блокирования или уничтожения, отозвать согласие и обжаловать действия Оператора в Роскомнадзоре или в суде.',
      `6.2. Запросы направляются по адресу ${EMAIL}. Оператор отвечает в течение 10 рабочих дней; срок может быть продлён не более чем на 5 рабочих дней с уведомлением о причинах. Данные, которые хранятся только на вашем телефоне, Оператору недоступны — их вы удаляете сами в Приложении.`,
    ],
  },
  {
    h: '7. Заключительные положения',
    p: [
      '7.1. Оператор может изменять Политику. Новая редакция действует с момента публикации на Сайте; дата редакции указана вверху страницы.',
      `7.2. Вопросы по Политике — по адресу ${EMAIL} или в группе проекта https://t.me/MeshTRX.`,
    ],
  },
];

const EN: Section[] = [
  {
    h: '1. General',
    p: [
      '1.1. This Privacy Policy describes what data Stanislav Butkovskiy (the "Operator") processes and how it is protected, in accordance with Russian Federal Law No. 152-FZ "On Personal Data".',
      '1.2. It covers the websites https://meshtrx.ru and https://meshtrx.com (the "Site"), the MeshTRX apps for Android and iOS and the desktop client (the "App").',
    ],
  },
  {
    h: '2. In short',
    p: [
      'MeshTRX has no accounts and no servers that store your messages, voice, contacts or location. Everything you see in the App stays on your phone. Voice, text, files and coordinates go over the radio to other people\'s radios — only when you talk, send something or have the beacon on. The Operator never receives this data.',
    ],
  },
  {
    h: '3. What is processed and why',
    p: [
      '3.1. Site. No tracking cookies and no web analytics. The web server keeps a standard request log (IP address, time, requested URL, browser details) to operate and protect the Site; it is deleted automatically after 14 days. The server is located in Russia.',
      '3.2. App — on the phone. Chats, voice messages, files, the list of heard stations, call history, callsign and settings are stored only on the phone and are removed with "Clear chat" or by uninstalling the App.',
      '3.3. App — over the radio. On your action the LoRa module transmits: voice only while push-to-talk or voice activation is on; texts, photos and files you send; your callsign; your coordinates in the periodic beacon if you granted location access. Other users\' radios in range receive this; the Operator does not.',
      '3.4. The radio channel is public: without an encryption key anyone with a compatible receiver can receive it. You can set a shared group key (AES-256) so that only radios with the same key can read voice and text.',
      '3.5. App — internet. Once a day and on "Check for updates" the App fetches https://meshtrx.ru/latest.json (the server sees your IP, see 3.1). The Android map loads tiles from OpenStreetMap servers, which receive your IP and the visible map area; the iOS map uses Apple MapKit under Apple\'s terms. Nothing else is sent; there are no ads and no analytics.',
    ],
  },
  {
    h: '4. App permissions',
    p: [
      'Bluetooth — find and stay connected to the radio. Microphone — voice, only while transmitting. Location — your position on the map and radar and in the beacon; older Android versions also require it to scan for Bluetooth devices. Photos and files — only those you pick to send. Notifications — incoming messages and calls while the App is in the background. Background operation — keeps the radio link while the phone is in your pocket, with a persistent notification. Denying a permission only disables the related feature.',
    ],
  },
  {
    h: '5. Your rights and contact',
    p: [
      `You may request information about, correction or deletion of your data and withdraw consent by writing to ${EMAIL}; we reply within 10 working days. Data kept only on your phone is not accessible to the Operator — you delete it yourself in the App. Questions are also welcome at https://t.me/MeshTRX.`,
    ],
  },
];

export default function PrivacyPage() {
  const { locale } = useLanguage();
  const ru = locale === 'ru';
  const sections = ru ? RU : EN;

  return (
    <div className="max-w-3xl mx-auto px-4 py-12">
      <h1 className="text-3xl font-bold mb-2">
        {ru ? 'Политика конфиденциальности' : 'Privacy Policy'}
      </h1>
      <p className="text-text-secondary mb-8">
        {ru ? `Редакция от ${EDITION.ru}.` : `Last updated ${EDITION.en}.`}
      </p>
      <div className="space-y-6">
        {sections.map((s) => (
          <section key={s.h} className="p-6 rounded-xl bg-bg-card border border-border">
            <h2 className="text-xl font-bold text-text-primary mb-3">{s.h}</h2>
            <div className="space-y-3">
              {s.p.map((para) => (
                <p key={para.slice(0, 40)} className="text-text-secondary leading-relaxed">{para}</p>
              ))}
            </div>
          </section>
        ))}
      </div>
    </div>
  );
}
