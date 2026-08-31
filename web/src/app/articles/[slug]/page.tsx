import type { Metadata } from 'next';
import { notFound } from 'next/navigation';
import { ARTICLES, articleBySlug } from '@/content/articles';
import { SITE } from '@/lib/constants';
import { SITE_LOCALE, SITE_URL, DOMAIN, OG_IMAGE } from '@/lib/site-locale';
import ArticleView from '@/components/articles/ArticleView';

// Экспорт статический, поэтому адреса всех статей должны быть известны на
// сборке: собирается ровно то, что вернёт generateStaticParams, чужой slug
// просто не существует.
//
// dynamicParams = false здесь не нужен и вреден. На сборку он не влияет —
// сервера, который мог бы достроить страницу на лету, у статического экспорта
// нет вовсе, — зато ломает dev: там fallbackMode выводится из dynamicParams, и
// при false Next решает, что generateStaticParams не экспортирован, и отдаёт
// 500 на каждую статью.
export async function generateStaticParams() {
  return ARTICLES.map((article) => ({ slug: article.slug }));
}

// Заголовок берётся на языке сборки: на meshtrx.com в html лежит английский,
// на meshtrx.ru — русский. Поисковику и превью в мессенджере достаётся ровно
// это, переключатель в шапке до них не доходит.
export function generateMetadata({ params }: { params: { slug: string } }): Metadata {
  const article = articleBySlug(params.slug);
  if (!article) return {};

  return {
    title: article.title[SITE_LOCALE],
    description: article.summary[SITE_LOCALE],
    openGraph: {
      title: article.title[SITE_LOCALE],
      description: article.summary[SITE_LOCALE],
      type: 'article',
      publishedTime: article.date,
      url: `${SITE_URL}/articles/${article.slug}/`,
      images: [OG_IMAGE],
    },
    twitter: {
      card: 'summary_large_image',
      title: article.title[SITE_LOCALE],
      description: article.summary[SITE_LOCALE],
      images: [OG_IMAGE.url],
    },
    alternates: {
      canonical: `${SITE_URL}/articles/${article.slug}/`,
      languages: {
        ru: `${DOMAIN.ru}/articles/${article.slug}/`,
        en: `${DOMAIN.en}/articles/${article.slug}/`,
        'x-default': `${DOMAIN.en}/articles/${article.slug}/`,
      },
    },
  };
}

export default function ArticlePage({ params }: { params: { slug: string } }) {
  const article = articleBySlug(params.slug);
  if (!article) notFound();

  // Разметка Schema.org: по ней поисковик понимает, что это статья, а не ещё
  // одна страница сайта, — и берёт дату, автора и картинку из неё, а не
  // угадывает по вёрстке.
  const jsonLd = {
    '@context': 'https://schema.org',
    '@type': 'Article',
    headline: article.title[SITE_LOCALE],
    description: article.summary[SITE_LOCALE],
    datePublished: article.date,
    dateModified: article.date,
    image: OG_IMAGE.url,
    inLanguage: SITE_LOCALE,
    mainEntityOfPage: `${SITE_URL}/articles/${article.slug}/`,
    author: { '@type': 'Organization', name: SITE.name, url: SITE_URL },
    publisher: { '@type': 'Organization', name: SITE.name, url: SITE_URL },
    about: { '@type': 'SoftwareApplication', name: SITE.name, url: SITE_URL },
  };

  return (
    <>
      <script
        type="application/ld+json"
        dangerouslySetInnerHTML={{ __html: JSON.stringify(jsonLd) }}
      />
      <ArticleView article={article} />
    </>
  );
}
