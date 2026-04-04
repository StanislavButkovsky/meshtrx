'use client';

import { VERSION, DOWNLOAD_LINKS } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';

export default function FirmwareDownload() {
  const { t } = useLanguage();

  return (
    <div className="p-6 rounded-xl bg-bg-card border border-border">
      <div className="flex items-start gap-4">
        <div className="w-16 h-16 rounded-xl bg-accent/10 border border-accent/30 flex items-center justify-center text-2xl flex-shrink-0">
          📡
        </div>
        <div className="flex-1">
          <h2 className="text-xl font-bold text-text-primary mb-1">{t('dl.firmware')}</h2>
          <div className="flex items-center gap-3 text-sm text-text-secondary mb-4">
            <span className="px-2 py-0.5 rounded bg-accent/10 text-accent text-xs font-medium">
              v{VERSION.firmware}
            </span>
            <span>{VERSION.date}</span>
          </div>
          <div className="flex flex-wrap gap-3">
            <a
              href={DOWNLOAD_LINKS.firmware}
              className="inline-flex items-center gap-2 px-5 py-2.5 bg-accent text-bg font-semibold rounded-lg hover:bg-accent-dim transition-colors"
            >
              <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
                <polyline points="7 10 12 15 17 10" />
                <line x1="12" x2="12" y1="15" y2="3" />
              </svg>
              Heltec V3
            </a>
            <a
              href={DOWNLOAD_LINKS.firmwareV4}
              className="inline-flex items-center gap-2 px-5 py-2.5 bg-bg border border-accent/40 text-accent font-semibold rounded-lg hover:bg-accent/10 transition-colors"
            >
              <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
                <polyline points="7 10 12 15 17 10" />
                <line x1="12" x2="12" y1="15" y2="3" />
              </svg>
              Heltec V4
              <span className="px-1.5 py-0.5 rounded text-[10px] font-bold bg-amber-500/20 text-amber-400 border border-amber-500/30">
                BETA
              </span>
            </a>
          </div>
          <p className="text-xs text-text-secondary mt-3">
            {t('dl.firmware_hint')}
          </p>
        </div>
      </div>
    </div>
  );
}
