'use client';

import { isWebSerialSupported, getBrowserName } from '@/lib/flash-utils';
import { useLanguage } from '@/components/LanguageProvider';

export default function BrowserCheck() {
  const { t } = useLanguage();
  const supported = isWebSerialSupported();
  const browser = getBrowserName();

  if (supported) {
    return (
      <div className="p-4 rounded-lg bg-accent/10 border border-accent/30 text-sm">
        <span className="text-accent font-medium">{t('flash.supported')}</span>
        <span className="text-text-secondary ml-2">({browser})</span>
      </div>
    );
  }

  return (
    <div className="p-4 rounded-lg bg-red-500/10 border border-red-500/30 text-sm">
      <p className="text-red-400 font-medium mb-1">{t('flash.unsupported')}</p>
      <p className="text-text-secondary">
        {t('flash.unsupported.desc')}
        {browser !== 'unknown' && ` (${browser})`}
      </p>
    </div>
  );
}
