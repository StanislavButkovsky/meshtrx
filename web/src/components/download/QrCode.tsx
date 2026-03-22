'use client';

import { QRCodeSVG } from 'qrcode.react';
import { SITE, DOWNLOAD_LINKS } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';

export default function QrCode() {
  const { t } = useLanguage();
  const url = `${SITE.url}${DOWNLOAD_LINKS.apk}`;

  return (
    <div className="p-6 rounded-xl bg-bg-card border border-border text-center">
      <h3 className="text-lg font-bold text-text-primary mb-4">{t('dl.qr.title')}</h3>
      <div className="inline-block p-4 bg-white rounded-xl">
        <QRCodeSVG value={url} size={160} />
      </div>
      <p className="text-xs text-text-secondary mt-3">{t('dl.qr.hint')}</p>
    </div>
  );
}
