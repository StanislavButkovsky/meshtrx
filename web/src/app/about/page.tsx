'use client';

import { SITE } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';

export default function AboutPage() {
  const { t } = useLanguage();

  const usageKeys = ['about.usage1', 'about.usage2', 'about.usage3', 'about.usage4', 'about.usage5'] as const;
  const storyKeys = ['about.story1', 'about.story2', 'about.story3', 'about.story4', 'about.story5', 'about.story6'] as const;
  const commonKeys = ['about.common1', 'about.common2', 'about.common3', 'about.common4', 'about.common5'] as const;
  const diffRows = ['r1', 'r2', 'r3', 'r4', 'r5', 'r6'] as const;
  const MESHTASTIC = 'https://meshtastic.org/';

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
          <h2 className="text-xl font-bold text-text-primary mb-3">{t('about.story')}</h2>
          <div className="space-y-4">
            {storyKeys.map((key) => (
              <p key={key} className="text-text-secondary leading-relaxed">
                {t(key)}
              </p>
            ))}
          </div>
        </section>

        <section className="p-6 rounded-xl bg-bg-card border border-border">
          <h2 className="text-xl font-bold text-text-primary mb-3">{t('about.compare')}</h2>
          <p className="text-text-secondary leading-relaxed mb-6">
            {t('about.compare.intro')}{' '}
            <a
              href={MESHTASTIC}
              target="_blank"
              rel="noopener noreferrer"
              className="text-accent hover:underline font-medium"
            >
              meshtastic.org
            </a>
          </p>

          <h3 className="text-base font-semibold text-text-primary mb-3">{t('about.common')}</h3>
          <ul className="space-y-2 text-text-secondary mb-6">
            {commonKeys.map((key) => (
              <li key={key} className="flex items-start gap-2">
                <span className="text-accent mt-1">•</span>
                {t(key)}
              </li>
            ))}
          </ul>

          <h3 className="text-base font-semibold text-text-primary mb-3">{t('about.diff')}</h3>
          {/* Таблица шире экрана телефона, поэтому прокручивается сама, а не тянет за собой страницу */}
          <div className="overflow-x-auto -mx-2 px-2">
            <table className="w-full min-w-[520px] text-sm border-collapse">
              <thead>
                <tr className="border-b border-border">
                  <th className="text-left font-medium text-text-secondary py-2 pr-4 w-1/4">
                    {t('about.diff.aspect')}
                  </th>
                  <th className="text-left font-medium text-text-secondary py-2 pr-4">Meshtastic</th>
                  <th className="text-left font-medium text-accent py-2">MeshTRX</th>
                </tr>
              </thead>
              <tbody>
                {diffRows.map((row) => (
                  <tr key={row} className="border-b border-border/50 align-top">
                    <td className="py-3 pr-4 text-text-primary">{t(`about.diff.${row}.a`)}</td>
                    <td className="py-3 pr-4 text-text-secondary">{t(`about.diff.${row}.m`)}</td>
                    <td className="py-3 text-text-secondary">{t(`about.diff.${row}.x`)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          <p className="text-text-secondary text-sm leading-relaxed mt-6">
            {t('about.compare.note')}
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
            <a
              href={MESHTASTIC}
              target="_blank"
              rel="noopener noreferrer"
              className="flex items-center justify-between p-3 rounded-lg bg-bg hover:bg-bg-hover border border-border transition-colors"
            >
              <span className="text-text-primary">Meshtastic</span>
              <span className="text-text-secondary text-sm">{MESHTASTIC}</span>
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
