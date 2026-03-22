'use client';

import { useLanguage } from '@/components/LanguageProvider';
import type { TranslationKey } from '@/lib/i18n';

const STEPS: { num: number; titleKey: TranslationKey; descKey: TranslationKey }[] = [
  { num: 1, titleKey: 'dl.step1.title', descKey: 'dl.step1.desc' },
  { num: 2, titleKey: 'dl.step2.title', descKey: 'dl.step2.desc' },
  { num: 3, titleKey: 'dl.step3.title', descKey: 'dl.step3.desc' },
  { num: 4, titleKey: 'dl.step4.title', descKey: 'dl.step4.desc' },
];

export default function InstallSteps() {
  const { t } = useLanguage();

  return (
    <div className="p-6 rounded-xl bg-bg-card border border-border">
      <h3 className="text-lg font-bold text-text-primary mb-4">{t('dl.install.title')}</h3>
      <div className="space-y-4">
        {STEPS.map((step) => (
          <div key={step.num} className="flex gap-4">
            <div className="w-8 h-8 rounded-full bg-accent/10 border border-accent/30 flex items-center justify-center flex-shrink-0">
              <span className="text-accent text-sm font-bold">{step.num}</span>
            </div>
            <div>
              <h4 className="text-sm font-semibold text-text-primary">{t(step.titleKey)}</h4>
              <p className="text-sm text-text-secondary">{t(step.descKey)}</p>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
