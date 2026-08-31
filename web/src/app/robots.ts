import type { MetadataRoute } from 'next';
import { SITE } from '@/lib/constants';

export const dynamic = 'force-static';

export default function robots(): MetadataRoute.Robots {
  return {
    rules: [
      {
        userAgent: '*',
        allow: '/',
        // В /downloads/ лежат прошивки и APK — десятки мегабайт на каждый
        // релиз. Искать их всё равно приходят на страницу загрузки, а качать
        // их роботами незачем: это только трафик сервера.
        disallow: '/downloads/',
      },
    ],
    sitemap: `${SITE.url}/sitemap.xml`,
  };
}
