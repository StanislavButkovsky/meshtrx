'use client';

import { USER_GUIDE, USER_GUIDE_EN } from '@/lib/docs-content';
import DocContent from '@/components/docs/DocContent';
import TableOfContents from '@/components/docs/TableOfContents';
import { useLanguage } from '@/components/LanguageProvider';

export default function DocsPage() {
  const { locale } = useLanguage();
  const content = locale === 'en' ? USER_GUIDE_EN : USER_GUIDE;

  return (
    <div className="max-w-6xl mx-auto px-4 py-12">
      <div className="flex gap-8">
        <aside className="hidden lg:block w-64 flex-shrink-0">
          <TableOfContents content={content} />
        </aside>
        <article className="flex-1 min-w-0">
          <DocContent content={content} />
        </article>
      </div>
    </div>
  );
}
