import type { Metadata } from 'next';
import { notFound } from 'next/navigation';
import { ARTICLES, articleBySlug } from '@/content/articles';
import { SITE, OG_IMAGE } from '@/lib/constants';
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

// В разметку идёт русский заголовок: страница отдаётся с lang="ru", а язык
// переключается уже в браузере. Поисковику и мессенджеру достаётся то, что
// лежит в html, и переключатель до них не доходит.
export function generateMetadata({ params }: { params: { slug: string } }): Metadata {
  const article = articleBySlug(params.slug);
  if (!article) return {};

  return {
    title: article.title.ru,
    description: article.summary.ru,
    openGraph: {
      title: article.title.ru,
      description: article.summary.ru,
      type: 'article',
      publishedTime: article.date,
      url: `${SITE.url}/articles/${article.slug}/`,
      images: [OG_IMAGE],
    },
    twitter: {
      card: 'summary_large_image',
      title: article.title.ru,
      description: article.summary.ru,
      images: [OG_IMAGE.url],
    },
    alternates: {
      canonical: `${SITE.url}/articles/${article.slug}/`,
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
    headline: article.title.ru,
    description: article.summary.ru,
    datePublished: article.date,
    dateModified: article.date,
    image: OG_IMAGE.url,
    inLanguage: 'ru',
    mainEntityOfPage: `${SITE.url}/articles/${article.slug}/`,
    author: { '@type': 'Organization', name: SITE.name, url: SITE.url },
    publisher: { '@type': 'Organization', name: SITE.name, url: SITE.url },
    about: { '@type': 'SoftwareApplication', name: SITE.name, url: SITE.url },
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
