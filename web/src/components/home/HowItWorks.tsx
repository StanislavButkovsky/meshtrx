'use client';

import { useLanguage } from '@/components/LanguageProvider';
import type { TranslationKey } from '@/lib/i18n';

const STEPS: { num: string; titleKey: TranslationKey; descKey: TranslationKey }[] = [
  { num: '01', titleKey: 'how.step1.title', descKey: 'how.step1.desc' },
  { num: '02', titleKey: 'how.step2.title', descKey: 'how.step2.desc' },
  { num: '03', titleKey: 'how.step3.title', descKey: 'how.step3.desc' },
];

export default function HowItWorks() {
  const { t } = useLanguage();

  return (
    <section className="max-w-6xl mx-auto px-4 py-16">
      <h2 className="text-2xl md:text-3xl font-bold text-center mb-12">
        {t('how.title')}
      </h2>
      <div className="grid md:grid-cols-3 gap-8">
        {STEPS.map((step) => (
          <div key={step.num} className="text-center">
            <div className="inline-flex items-center justify-center w-16 h-16 rounded-full border-2 border-accent/30 text-accent text-xl font-bold mb-4">
              {step.num}
            </div>
            <h3 className="text-lg font-semibold text-text-primary mb-2">{t(step.titleKey)}</h3>
            <p className="text-sm text-text-secondary">{t(step.descKey)}</p>
          </div>
        ))}
      </div>
    </section>
  );
}
