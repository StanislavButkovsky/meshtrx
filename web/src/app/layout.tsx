import type { Metadata } from 'next';
import { SITE, OG_IMAGE } from '@/lib/constants';
import { LanguageProvider } from '@/components/LanguageProvider';
import Header from '@/components/layout/Header';
import Footer from '@/components/layout/Footer';
import './globals.css';

export const metadata: Metadata = {
  title: {
    default: `${SITE.name} — Off-grid голосовая связь`,
    template: `%s | ${SITE.name}`,
  },
  description: SITE.description,
  metadataBase: new URL(SITE.url),
  openGraph: {
    title: SITE.name,
    description: SITE.description,
    url: SITE.url,
    siteName: SITE.name,
    type: 'website',
    images: [OG_IMAGE],
  },
  // Без явной карточки Telegram и X показывают ссылку голым текстом, а на
  // ссылку с картинкой нажимают заметно чаще — а ссылками на проект делятся
  // в основном как раз там.
  twitter: {
    card: 'summary_large_image',
    title: SITE.name,
    description: SITE.description,
    images: [OG_IMAGE.url],
  },
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="ru">
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
