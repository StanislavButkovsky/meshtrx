'use client';

import Link from 'next/link';
import { HARDWARE, BOARDS } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';
import type { TranslationKey } from '@/lib/i18n';

export default function HardwareSection() {
  const { t } = useLanguage();

  const specs: { labelKey: TranslationKey; value: string }[] = [
    { labelKey: 'hw.chip', value: HARDWARE.chip },
    // Частота — с единицей измерения, а она переводится: в английской сборке
    // «МГц» кириллицей смотрелось как недоделка.
    { labelKey: 'hw.lora', value: `${HARDWARE.lora} @ ${t('hw.freq')}` },
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

            <h4 className="text-sm font-semibold text-text-primary mt-8 mb-3">{t('hw.boards')}</h4>
            {/* Таблица шире экрана телефона, поэтому прокручивается сама, а не
                тянет за собой страницу — как в сравнении на странице «О проекте». */}
            <div className="overflow-x-auto -mx-2 px-2">
              <table className="w-full min-w-[440px] text-sm border-collapse">
                <thead>
                  <tr className="border-b border-border">
                    <th className="text-left font-medium text-text-secondary py-2 pr-4">{t('hw.board.col')}</th>
                    <th className="text-left font-medium text-text-secondary py-2 pr-4">{t('hw.board.what')}</th>
                    <th className="text-left font-medium text-text-secondary py-2">{t('hw.board.file')}</th>
                  </tr>
                </thead>
                <tbody>
                  {BOARDS.map((b) => (
                    <tr key={b.board} className="border-b border-border/50 align-top">
                      <td className="py-2 pr-4 text-accent font-medium whitespace-nowrap">{b.board}</td>
                      <td className="py-2 pr-4 text-text-secondary">{t(b.chipKey)}</td>
                      <td className="py-2 text-text-secondary whitespace-nowrap">{b.file}.bin</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
            <p className="text-xs text-text-secondary leading-relaxed mt-4">
              {t('hw.board.note')}{' '}
              <Link href="/download/" className="text-accent hover:underline">
                {t('nav.download')}
              </Link>
            </p>
          </div>
        </div>
      </div>
    </section>
  );
}
