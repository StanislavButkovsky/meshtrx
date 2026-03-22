'use client';

import { useLanguage } from '@/components/LanguageProvider';
import { getFeatureIcon } from '@/components/icons/FeatureIcons';
import type { TranslationKey } from '@/lib/i18n';

const FEATURES: { titleKey: TranslationKey; descKey: TranslationKey; icon: string }[] = [
  { titleKey: 'feature.voice.title', descKey: 'feature.voice.desc', icon: 'mic' },
  { titleKey: 'feature.msg.title', descKey: 'feature.msg.desc', icon: 'message' },
  { titleKey: 'feature.file.title', descKey: 'feature.file.desc', icon: 'file' },
  { titleKey: 'feature.map.title', descKey: 'feature.map.desc', icon: 'map' },
  { titleKey: 'feature.call.title', descKey: 'feature.call.desc', icon: 'call' },
  { titleKey: 'feature.relay.title', descKey: 'feature.relay.desc', icon: 'relay' },
];

export default function FeatureGrid() {
  const { t } = useLanguage();

  return (
    <section className="max-w-6xl mx-auto px-4 py-16">
      <h2 className="text-2xl md:text-3xl font-bold text-center mb-12">
        {t('features.title')}
      </h2>
      <div className="grid md:grid-cols-2 lg:grid-cols-3 gap-6">
        {FEATURES.map((feature, i) => {
          const Icon = getFeatureIcon(feature.icon);
          return (
            <div
              key={i}
              className="p-6 rounded-xl bg-bg-card border border-border hover:border-accent/30 transition-colors group"
            >
              <div className="w-12 h-12 rounded-lg bg-accent/10 flex items-center justify-center mb-4 group-hover:bg-accent/20 transition-colors">
                <Icon className="text-accent" />
              </div>
              <h3 className="text-lg font-semibold text-text-primary mb-2">{t(feature.titleKey)}</h3>
              <p className="text-sm text-text-secondary leading-relaxed">{t(feature.descKey)}</p>
            </div>
          );
        })}
      </div>
    </section>
  );
}
