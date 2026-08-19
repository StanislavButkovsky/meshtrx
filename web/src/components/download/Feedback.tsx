'use client';

import { SITE, VERSION } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';

/**
 * Призыв к тестированию свежей сборки. Стоит сразу после блоков загрузки:
 * человек уже скачал файл, и это единственный момент, когда просьба
 * рассказать о результате не выглядит навязчивой.
 */
export default function Feedback() {
  const { t } = useLanguage();

  return (
    <div className="p-6 rounded-xl bg-bg-card border border-accent/40">
      <div className="flex items-center gap-3 mb-3">
        <span className="px-2 py-0.5 rounded text-xs font-bold bg-accent text-bg">
          v{VERSION.firmware}
        </span>
        <h3 className="text-lg font-bold text-text-primary">{t('dl.feedback.title')}</h3>
      </div>
      <p className="text-sm text-text-secondary mb-4">{t('dl.feedback.desc')}</p>
      <ul className="space-y-1 mb-5">
        {(['dl.feedback.p1', 'dl.feedback.p2', 'dl.feedback.p3'] as const).map((key) => (
          <li key={key} className="text-sm text-text-secondary flex items-start gap-2">
            <span className="text-accent mt-1">•</span>
            {t(key)}
          </li>
        ))}
      </ul>
      <div className="flex flex-wrap items-center gap-3">
        <a
          href={SITE.telegram}
          target="_blank"
          rel="noopener noreferrer"
          className="px-4 py-2 rounded-lg bg-accent text-bg font-bold text-sm hover:opacity-90 transition"
        >
          {t('dl.feedback.tg')}
        </a>
        <span className="text-sm text-text-secondary">t.me/MeshTRX</span>
      </div>
    </div>
  );
}
