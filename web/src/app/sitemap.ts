import type { MetadataRoute } from 'next';
import { SITE, VERSION } from '@/lib/constants';

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
    { path: 'flash/', changeFrequency: 'monthly', priority: 0.7 },
    { path: 'about/', changeFrequency: 'monthly', priority: 0.6 },
  ];

  return pages.map(({ path, changeFrequency, priority }) => ({
    url: `${SITE.url}/${path}`,
    lastModified: updated,
    changeFrequency,
    priority,
  }));
}
