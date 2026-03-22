'use client';

import { useLanguage } from '@/components/LanguageProvider';
import type { TranslationKey } from '@/lib/i18n';

const STATS: { value: string; unitKey: TranslationKey; labelKey: TranslationKey; hintKey?: TranslationKey }[] = [
  { value: '5+', unitKey: 'stats.range.unit', labelKey: 'stats.range', hintKey: 'stats.range.hint' },
  { value: '23', unitKey: 'stats.channels.unit', labelKey: 'stats.channels' },
  { value: '160', unitKey: 'stats.latency.unit', labelKey: 'stats.latency' },
  { value: '3200', unitKey: 'stats.bitrate' as TranslationKey, labelKey: 'stats.bitrate' },
];

export default function StatsBar() {
  const { t } = useLanguage();

  return (
    <section className="border-y border-border bg-bg-card/50">
      <div className="max-w-6xl mx-auto px-4 py-8">
        <div className="grid grid-cols-2 md:grid-cols-4 gap-6">
          {STATS.map((stat, i) => (
            <div key={i} className="text-center">
              <div className="text-3xl md:text-4xl font-bold text-accent glow-text">
                {stat.value}
                <span className="text-lg text-text-secondary ml-1">
                  {i === 3 ? 'bps' : t(stat.unitKey)}
                </span>
              </div>
              <div className="text-sm text-text-secondary mt-1">{t(stat.labelKey)}</div>
              {stat.hintKey && (
                <div className="text-xs text-accent/60 mt-1.5 px-2 py-0.5 rounded-full bg-accent/5 border border-accent/10 inline-block">
                  {t(stat.hintKey)}
                </div>
              )}
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
