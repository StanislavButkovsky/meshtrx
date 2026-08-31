import type { Locale } from './i18n';

// Язык сборки. Сайт собирается дважды из одного кода: meshtrx.ru — по-русски,
// meshtrx.com — по-английски, nginx отдаёт каждому домену свой каталог.
//
// Переключатель в шапке никуда не делся и продолжает работать на обоих
// доменах: переводы целиком лежат в бандле, и выбор человека, сохранённый в
// localStorage, важнее языка домена. Домен задаёт только значение по умолчанию
// — и, что важнее, язык самого HTML: заголовок, описание и lang. Поисковик и
// превью ссылки в мессенджере JS не выполняют и видят ровно то, что в файле.
//
// Переменную подставляет сборка (см. scripts/deploy.sh). Без неё — русский,
// как было до разделения, чтобы `npm run dev` вёл себя привычно.
export const SITE_LOCALE: Locale = process.env.NEXT_PUBLIC_SITE_LOCALE === 'en' ? 'en' : 'ru';

export const DOMAIN: Record<Locale, string> = {
  ru: 'https://meshtrx.ru',
  en: 'https://meshtrx.com',
};

/** Адрес той версии сайта, которую сейчас собираем. */
export const SITE_URL = DOMAIN[SITE_LOCALE];

/** Адрес версии на другом языке — для hreflang. */
export const OTHER_LOCALE: Locale = SITE_LOCALE === 'ru' ? 'en' : 'ru';

// Тексты для <head>. Они не могут прийти из i18n через хук: метаданные
// собираются на сервере, где хуков нет, поэтому лежат обычными строками.
export const META: Record<Locale, { title: string; description: string; tagline: string }> = {
  ru: {
    title: 'MeshTRX — Off-grid голосовая связь',
    description:
      'Голосовая связь, сообщения и файлы без интернета и сотовых сетей. Работает на Heltec V3 + Android.',
    tagline: 'Off-grid голосовая связь через LoRa mesh-сеть',
  },
  en: {
    title: 'MeshTRX — Off-grid voice communication',
    description:
      'Voice, messages and files with no internet and no cellular network. Runs on Heltec V3 + Android.',
    tagline: 'Off-grid voice communication over a LoRa mesh network',
  },
};

export const SITE_META = META[SITE_LOCALE];

// Картинка для ссылки в мессенджере и поисковой выдаче. 1200×630 — размер, на
// который рассчитывают Telegram, X и Facebook; меньше — обрежут или покажут
// маленькой иконкой. Адрес свой у каждой сборки: превью должно вести на тот же
// домен, с которого пришла ссылка.
export const OG_IMAGE = {
  url: `${SITE_URL}/og.png`,
  width: 1200,
  height: 630,
  alt: `MeshTRX — ${SITE_META.tagline}`,
};
