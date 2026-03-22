'use client';

import { VERSION, DOWNLOAD_LINKS } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';

export default function ApkDownload() {
  const { t } = useLanguage();

  return (
    <div className="p-6 rounded-xl bg-bg-card border border-border">
      <div className="flex items-start gap-4">
        <div className="w-16 h-16 rounded-xl bg-accent/10 border border-accent/30 flex items-center justify-center text-2xl flex-shrink-0">
          📱
        </div>
        <div className="flex-1">
          <h2 className="text-xl font-bold text-text-primary mb-1">{t('dl.android')}</h2>
          <div className="flex items-center gap-3 text-sm text-text-secondary mb-4">
            <span className="px-2 py-0.5 rounded bg-accent/10 text-accent text-xs font-medium">
              v{VERSION.app}
            </span>
            <span>{VERSION.date}</span>
            <span>Android 8.0+</span>
          </div>
          <a
            href={DOWNLOAD_LINKS.apk}
            className="inline-flex items-center gap-2 px-6 py-3 bg-accent text-bg font-semibold rounded-lg hover:bg-accent-dim transition-colors glow"
          >
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
              <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
              <polyline points="7 10 12 15 17 10" />
              <line x1="12" x2="12" y1="15" y2="3" />
            </svg>
            {t('dl.btn')}
          </a>
        </div>
      </div>
    </div>
  );
}
