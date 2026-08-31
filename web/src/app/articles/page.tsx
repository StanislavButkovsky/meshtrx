'use client';

import Link from 'next/link';
import { ARTICLES, readingMinutes } from '@/content/articles';
import { useLanguage } from '@/components/LanguageProvider';

export default function ArticlesPage() {
  const { locale, t } = useLanguage();

  return (
    <div className="max-w-3xl mx-auto px-4 py-12">
      <h1 className="text-3xl font-bold mb-3">{t('articles.title')}</h1>
      <p className="text-text-secondary leading-relaxed mb-8">{t('articles.subtitle')}</p>

      {ARTICLES.length === 0 ? (
        <p className="text-text-secondary">{t('articles.empty')}</p>
      ) : (
        <div className="space-y-4">
          {ARTICLES.map((article) => (
            <Link
              key={article.slug}
              href={`/articles/${article.slug}/`}
              className="group block p-6 rounded-xl bg-bg-card border border-border hover:border-accent/40 transition-colors"
            >
              <div className="flex items-center gap-2 text-xs text-text-secondary mb-3">
                <time dateTime={article.date}>{article.date}</time>
                <span>•</span>
                <span>
                  {readingMinutes(article.body[locale])} {t('articles.reading')}
                </span>
              </div>
              <h2 className="text-xl font-bold text-text-primary group-hover:text-accent transition-colors mb-2">
                {article.title[locale]}
              </h2>
              <p className="text-text-secondary leading-relaxed mb-3">{article.summary[locale]}</p>
              <span className="text-accent text-sm">{t('articles.read')} →</span>
            </Link>
          ))}
        </div>
      )}
    </div>
  );
}
