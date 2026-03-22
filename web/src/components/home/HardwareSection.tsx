'use client';

import { HARDWARE } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';
import type { TranslationKey } from '@/lib/i18n';

export default function HardwareSection() {
  const { t } = useLanguage();

  const specs: { labelKey: TranslationKey; value: string }[] = [
    { labelKey: 'hw.chip', value: HARDWARE.chip },
    { labelKey: 'hw.lora', value: `${HARDWARE.lora} @ ${HARDWARE.freq}` },
    { labelKey: 'hw.power', value: HARDWARE.power },
    { labelKey: 'hw.ble', value: HARDWARE.ble },
    { labelKey: 'hw.battery', value: HARDWARE.battery },
    { labelKey: 'hw.antenna', value: HARDWARE.antenna },
  ];

  return (
    <section className="bg-bg-card/50 border-y border-border">
      <div className="max-w-6xl mx-auto px-4 py-16">
        <h2 className="text-2xl md:text-3xl font-bold text-center mb-12">
          {t('hw.title')}
        </h2>
        <div className="max-w-2xl mx-auto">
          <div className="p-6 rounded-xl bg-bg border border-border">
            <div className="flex items-center gap-4 mb-6">
              <div className="w-16 h-16 rounded-xl bg-accent/10 border border-accent/30 flex items-center justify-center text-2xl">
                📡
              </div>
              <div>
                <h3 className="text-lg font-bold text-text-primary">{HARDWARE.name}</h3>
                <p className="text-sm text-text-secondary">{t('hw.subtitle')}</p>
              </div>
            </div>
            <div className="grid grid-cols-2 gap-4">
              {specs.map((spec) => (
                <div key={spec.labelKey} className="flex justify-between items-center py-2 border-b border-border/50">
                  <span className="text-sm text-text-secondary">{t(spec.labelKey)}</span>
                  <span className="text-sm text-accent font-medium">{spec.value}</span>
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>
    </section>
  );
}
