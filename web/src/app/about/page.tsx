'use client';

import { SITE } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';

export default function AboutPage() {
  const { t } = useLanguage();

  const usageKeys = ['about.usage1', 'about.usage2', 'about.usage3', 'about.usage4', 'about.usage5'] as const;

  return (
    <div className="max-w-3xl mx-auto px-4 py-12">
      <h1 className="text-3xl font-bold mb-8">{t('about.title')}</h1>

      <div className="space-y-8">
        <section className="p-6 rounded-xl bg-bg-card border border-border">
          <h2 className="text-xl font-bold text-text-primary mb-3">MeshTRX</h2>
          <p className="text-text-secondary leading-relaxed mb-4">
            {t('about.desc1')}
          </p>
          <p className="text-text-secondary leading-relaxed">
            {t('about.desc2')}
          </p>
        </section>

        <section className="p-6 rounded-xl bg-bg-card border border-border">
          <h2 className="text-xl font-bold text-text-primary mb-3">{t('about.usage')}</h2>
          <ul className="space-y-2 text-text-secondary">
            {usageKeys.map((key) => (
              <li key={key} className="flex items-start gap-2">
                <span className="text-accent mt-1">•</span>
                {t(key)}
              </li>
            ))}
          </ul>
        </section>

        <section className="p-6 rounded-xl bg-bg-card border border-border">
          <h2 className="text-xl font-bold text-text-primary mb-3">{t('about.license')}</h2>
          <p className="text-text-secondary leading-relaxed mb-3">
            {t('about.license.desc')}{' '}
            <a
              href={SITE.licenseUrl}
              target="_blank"
              rel="noopener noreferrer"
              className="text-accent hover:underline font-medium"
            >
              {SITE.license}
            </a>.
          </p>
          <p className="text-text-secondary text-sm leading-relaxed">
            {t('about.license.terms')}
          </p>
        </section>

        <section className="p-6 rounded-xl bg-bg-card border border-border">
          <h2 className="text-xl font-bold text-text-primary mb-3">{t('about.links')}</h2>
          <div className="space-y-3">
            <a
              href={SITE.github}
              target="_blank"
              rel="noopener noreferrer"
              className="flex items-center justify-between p-3 rounded-lg bg-bg hover:bg-bg-hover border border-border transition-colors"
            >
              <span className="text-text-primary">GitHub</span>
              <span className="text-text-secondary text-sm">{SITE.github}</span>
            </a>
            <a
              href={SITE.telegram}
              target="_blank"
              rel="noopener noreferrer"
              className="flex items-center justify-between p-3 rounded-lg bg-bg hover:bg-bg-hover border border-border transition-colors"
            >
              <span className="text-text-primary">Telegram</span>
              <span className="text-text-secondary text-sm">{SITE.telegram}</span>
            </a>
            <div className="flex items-center justify-between p-3 rounded-lg bg-bg border border-border">
              <span className="text-text-primary">{t('about.site')}</span>
              <span className="text-text-secondary text-sm">{SITE.url} / {SITE.urlRu}</span>
            </div>
          </div>
        </section>
      </div>
    </div>
  );
}
