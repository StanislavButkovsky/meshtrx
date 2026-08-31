import type { MetadataRoute } from 'next';
import { SITE, VERSION } from '@/lib/constants';
import { ARTICLES } from '@/content/articles';

// Экспорт статический, поэтому карта считается один раз при сборке.
export const dynamic = 'force-static';

// Сайт отдаётся с двух доменов, meshtrx.com и meshtrx.ru, одним и тем же
// содержимым. В карте указан только основной: иначе поисковик видит две копии
// каждой страницы и сам решает, какую считать настоящей.
export default function sitemap(): MetadataRoute.Sitemap {
  const updated = new Date(VERSION.date);

  const pages: Array<{
    path: string;
    changeFrequency: 'weekly' | 'monthly';
    priority: number;
  }> = [
    { path: '', changeFrequency: 'monthly', priority: 1 },
    // Страница загрузки меняется каждый релиз — на ней версии и файлы
    { path: 'download/', changeFrequency: 'weekly', priority: 0.9 },
    { path: 'docs/', changeFrequency: 'weekly', priority: 0.8 },
    { path: 'articles/', changeFrequency: 'monthly', priority: 0.7 },
    { path: 'flash/', changeFrequency: 'monthly', priority: 0.7 },
    { path: 'about/', changeFrequency: 'monthly', priority: 0.6 },
  ];

  // У статьи своя дата: она не переписывается с каждым релизом, и подставлять
  // ей дату версии — значит каждый раз звать поисковик перечитывать текст,
  // который не менялся.
  const articles: MetadataRoute.Sitemap = ARTICLES.map((article) => ({
    url: `${SITE.url}/articles/${article.slug}/`,
    lastModified: new Date(article.date),
    changeFrequency: 'yearly',
    priority: 0.6,
  }));

  return [
    ...pages.map(({ path, changeFrequency, priority }) => ({
      url: `${SITE.url}/${path}`,
      lastModified: updated,
      changeFrequency,
      priority,
    })),
    ...articles,
  ];
}
