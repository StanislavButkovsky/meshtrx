import type { MetadataRoute } from 'next';
import { VERSION } from '@/lib/constants';
import { SITE_URL } from '@/lib/site-locale';
import { ARTICLES } from '@/content/articles';

// Экспорт статический, поэтому карта считается один раз при сборке.
export const dynamic = 'force-static';

// У каждого домена своя карта со своими адресами: meshtrx.ru и meshtrx.com —
// не копии, а русская и английская версии, и связаны они через hreflang в
// layout. Раньше карта на обоих доменах указывала на meshtrx.com.
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
    url: `${SITE_URL}/articles/${article.slug}/`,
    lastModified: new Date(article.date),
    changeFrequency: 'yearly',
    priority: 0.6,
  }));

  return [
    ...pages.map(({ path, changeFrequency, priority }) => ({
      url: `${SITE_URL}/${path}`,
      lastModified: updated,
      changeFrequency,
      priority,
    })),
    ...articles,
  ];
}
