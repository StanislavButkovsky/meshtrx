import type { Metadata } from 'next';
import { SITE } from '@/lib/constants';
import { SITE_LOCALE, SITE_URL, DOMAIN, SITE_META, OG_IMAGE } from '@/lib/site-locale';
import { LanguageProvider } from '@/components/LanguageProvider';
import Header from '@/components/layout/Header';
import Footer from '@/components/layout/Footer';
import './globals.css';

export const metadata: Metadata = {
  title: {
    default: SITE_META.title,
    template: `%s | ${SITE.name}`,
  },
  description: SITE_META.description,
  metadataBase: new URL(SITE_URL),
  // Домены больше не копии друг друга, а две языковые версии. hreflang говорит
  // об этом прямо, canonical у каждой указывает на себя: раньше обе ссылались
  // на meshtrx.com, и русская версия объявляла себя дублем английской.
  alternates: {
    canonical: SITE_URL,
    // Слэш на конце — ради единообразия с остальными адресами сайта, он собран
    // с trailingSlash. Для корня это ничего не меняет: голый домен и домен со
    // слэшем — один и тот же запрос GET /. На вложенных страницах слэш уже
    // существенен, и там он проставляется (см. страницу статьи).
    languages: {
      ru: `${DOMAIN.ru}/`,
      en: `${DOMAIN.en}/`,
      'x-default': `${DOMAIN.en}/`,
    },
  },
  openGraph: {
    title: SITE.name,
    description: SITE_META.description,
    url: SITE_URL,
    siteName: SITE.name,
    type: 'website',
    locale: SITE_LOCALE === 'ru' ? 'ru_RU' : 'en_US',
    images: [OG_IMAGE],
  },
  // Без явной карточки Telegram и X показывают ссылку голым текстом, а на
  // ссылку с картинкой нажимают заметно чаще — а ссылками на проект делятся
  // в основном как раз там.
  twitter: {
    card: 'summary_large_image',
    title: SITE.name,
    description: SITE_META.description,
    images: [OG_IMAGE.url],
  },
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang={SITE_LOCALE}>
      <body className="min-h-screen flex flex-col">
        <LanguageProvider>
          <Header />
          <main className="flex-1 pt-16">{children}</main>
          <Footer />
        </LanguageProvider>
      </body>
    </html>
  );
}
