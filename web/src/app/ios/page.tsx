'use client';

import { VERSION_IOS, DOWNLOAD_LINKS } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';

export default function IosPage() {
  const { t } = useLanguage();

  return (
    <div className="max-w-3xl mx-auto px-4 py-12">
      {/* Header */}
      <div className="text-center mb-10">
        <span className="inline-block px-3 py-1 rounded-full bg-blue-500/10 text-blue-400 text-xs font-medium mb-4">
          {t('ios.badge')}
        </span>
        <h1 className="text-3xl font-bold mb-3">{t('ios.title')}</h1>
        <p className="text-text-secondary">{t('ios.subtitle')}</p>
      </div>

      {/* Download card */}
      <div className="p-6 rounded-xl bg-bg-card border border-border mb-8">
        <div className="flex items-start gap-4">
          <div className="w-16 h-16 rounded-xl bg-blue-500/10 border border-blue-500/30 flex items-center justify-center text-2xl flex-shrink-0">
            🍎
          </div>
          <div className="flex-1">
            <h2 className="text-xl font-bold text-text-primary mb-1">{t('ios.title')}</h2>
            <div className="flex items-center gap-3 text-sm text-text-secondary mb-4">
              <span className="px-2 py-0.5 rounded bg-blue-500/10 text-blue-400 text-xs font-medium">
                v{VERSION_IOS.app}
              </span>
              <span>{VERSION_IOS.date}</span>
              <span>iOS 16+</span>
            </div>
            <a
              href={DOWNLOAD_LINKS.ipa}
              className="inline-flex items-center gap-2 px-6 py-3 bg-blue-500 text-white font-semibold rounded-lg hover:bg-blue-600 transition-colors"
            >
              <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
                <polyline points="7 10 12 15 17 10" />
                <line x1="12" x2="12" y1="15" y2="3" />
              </svg>
              {t('ios.btn')}
            </a>
            <p className="mt-3 text-xs text-text-secondary">
              {t('dl.file')}: <code className="text-text-primary">{DOWNLOAD_LINKS.ipa.split('/').pop()}</code>
            </p>
          </div>
        </div>
      </div>

      {/* What is AltStore */}
      <section className="mb-8">
        <h2 className="text-xl font-bold mb-3">{t('ios.what.title')}</h2>
        <p className="text-text-secondary mb-4">{t('ios.what.desc')}</p>
        <a
          href="https://altstore.io"
          target="_blank"
          rel="noopener noreferrer"
          className="inline-flex items-center gap-2 text-blue-400 hover:text-blue-300 transition-colors"
        >
          altstore.io
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6" />
            <polyline points="15 3 21 3 21 9" />
            <line x1="10" x2="21" y1="14" y2="3" />
          </svg>
        </a>
      </section>

      {/* Installation steps */}
      <section className="mb-8">
        <h2 className="text-xl font-bold mb-4">{t('ios.steps.title')}</h2>
        <div className="space-y-4">
          {[1, 2, 3, 4, 5, 6].map((n) => (
            <div key={n} className="flex gap-4 p-4 rounded-lg bg-bg-card border border-border">
              <div className="w-8 h-8 rounded-full bg-blue-500/20 text-blue-400 flex items-center justify-center text-sm font-bold flex-shrink-0">
                {n}
              </div>
              <div>
                <h3 className="font-semibold text-text-primary mb-1">
                  {t(`ios.step${n}.title` as any)}
                </h3>
                <p className="text-sm text-text-secondary">
                  {t(`ios.step${n}.desc` as any)}
                </p>
                {n === 1 && (
                  <a
                    href="https://altstore.io"
                    target="_blank"
                    rel="noopener noreferrer"
                    className="inline-flex items-center gap-1 mt-2 text-sm text-blue-400 hover:text-blue-300"
                  >
                    altstore.io →
                  </a>
                )}
              </div>
            </div>
          ))}
        </div>
      </section>

      {/* Re-signing */}
      <section className="mb-8">
        <h2 className="text-xl font-bold mb-3">{t('ios.resign.title')}</h2>
        <p className="text-text-secondary mb-3">{t('ios.resign.desc')}</p>
        <ul className="space-y-2 mb-4">
          {[1, 2, 3].map((n) => (
            <li key={n} className="flex items-start gap-2 text-sm text-text-secondary">
              <span className="text-blue-400 mt-0.5">•</span>
              {t(`ios.resign.${n}` as any)}
            </li>
          ))}
        </ul>
        <p className="text-sm text-text-secondary bg-amber-500/5 border border-amber-500/20 rounded-lg p-3">
          ⚠️ {t('ios.resign.manual')}
        </p>
      </section>

      {/* Limitations */}
      <section className="mb-8">
        <h2 className="text-xl font-bold mb-3">{t('ios.limits.title')}</h2>
        <ul className="space-y-2">
          {[1, 2, 3].map((n) => (
            <li key={n} className="flex items-start gap-2 text-sm text-text-secondary">
              <span className="text-amber-400 mt-0.5">⚠</span>
              {t(`ios.limits.${n}` as any)}
            </li>
          ))}
        </ul>
      </section>

      {/* Alternatives */}
      <section className="mb-8">
        <h2 className="text-xl font-bold mb-3">{t('ios.alt.title')}</h2>
        <div className="space-y-3">
          <div className="p-3 rounded-lg bg-bg-card border border-border">
            <a href="https://sideloadly.io" target="_blank" rel="noopener noreferrer"
               className="text-blue-400 hover:text-blue-300 font-medium">Sideloadly</a>
            <span className="text-text-secondary text-sm"> — {t('ios.alt.sideloadly').split(' — ')[1]}</span>
          </div>
          <div className="p-3 rounded-lg bg-bg-card border border-border">
            <span className="text-text-primary font-medium">TrollStore</span>
            <span className="text-text-secondary text-sm"> — {t('ios.alt.trollstore').split(' — ')[1]}</span>
          </div>
        </div>
      </section>
    </div>
  );
}
