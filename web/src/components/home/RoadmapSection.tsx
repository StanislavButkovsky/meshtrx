'use client';

import { useLanguage } from '@/components/LanguageProvider';
import type { TranslationKey } from '@/lib/i18n';

const PLANS: { status: 'soon' | 'planned'; titleKey: TranslationKey; descKey: TranslationKey }[] = [
  { status: 'soon', titleKey: 'roadmap.v4.title', descKey: 'roadmap.v4.desc' },
  { status: 'soon', titleKey: 'roadmap.encrypt.title', descKey: 'roadmap.encrypt.desc' },
  { status: 'planned', titleKey: 'roadmap.devices.title', descKey: 'roadmap.devices.desc' },
  { status: 'planned', titleKey: 'roadmap.desktop.title', descKey: 'roadmap.desktop.desc' },
  { status: 'planned', titleKey: 'roadmap.mesh.title', descKey: 'roadmap.mesh.desc' },
];

export default function RoadmapSection() {
  const { t } = useLanguage();

  return (
    <section className="max-w-6xl mx-auto px-4 py-16">
      <div className="text-center mb-12">
        <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-yellow-500/10 border border-yellow-500/30 text-yellow-400 text-xs mb-4">
          {t('roadmap.badge')}
        </div>
        <h2 className="text-2xl md:text-3xl font-bold">
          {t('roadmap.title')}
        </h2>
        <p className="text-text-secondary mt-3 max-w-lg mx-auto">
          {t('roadmap.desc')}
        </p>
      </div>
      <div className="grid md:grid-cols-2 lg:grid-cols-3 gap-4">
        {PLANS.map((item, i) => (
          <div
            key={i}
            className="p-5 rounded-xl bg-bg-card border border-border hover:border-yellow-500/30 transition-colors"
          >
            <div className="flex items-center gap-2 mb-3">
              <span
                className={`px-2 py-0.5 rounded text-xs font-medium ${
                  item.status === 'soon'
                    ? 'bg-accent/10 text-accent'
                    : 'bg-yellow-500/10 text-yellow-400'
                }`}
              >
                {t(item.status === 'soon' ? 'roadmap.soon' : 'roadmap.planned')}
              </span>
            </div>
            <h3 className="text-base font-semibold text-text-primary mb-2">{t(item.titleKey)}</h3>
            <p className="text-sm text-text-secondary leading-relaxed">{t(item.descKey)}</p>
          </div>
        ))}
      </div>
    </section>
  );
}
