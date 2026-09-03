'use client';

import Link from 'next/link';
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
            <span>{VERSION.firmwareDate}</span>
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
            <a
              href={DOWNLOAD_LINKS.firmwareV43}
              className="inline-flex items-center gap-2 px-5 py-2.5 bg-bg border border-accent/40 text-accent font-semibold rounded-lg hover:bg-accent/10 transition-colors"
            >
              <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
                <polyline points="7 10 12 15 17 10" />
                <line x1="12" x2="12" y1="15" y2="3" />
              </svg>
              Heltec V4.3
              <span className="px-1.5 py-0.5 rounded text-[10px] font-bold bg-amber-500/20 text-amber-400 border border-amber-500/30">
                beta
              </span>
            </a>
          </div>
          <p className="text-xs text-text-secondary mt-3">
            {t('dl.firmware_hint')}
          </p>

          {/* Мастер прошивки в браузере умеет только V3, владельцу V4 нужен
              esptool — а к нему, кроме самой прошивки, ещё три файла. Раньше
              они лежали в каталоге без единой ссылки: человек из группы нашёл
              их, только подставив адреса руками. */}
          <div className="mt-5 pt-4 border-t border-border">
            <h3 className="text-sm font-semibold text-text-primary mb-1">
              {t('dl.manual.title')}
            </h3>
            <p className="text-xs text-text-secondary mb-3">
              {t('dl.manual.desc')}{' '}
              <Link href="/docs/#прошивка-устройства" className="text-accent hover:underline">
                {t('dl.manual.docs')}
              </Link>
            </p>
            <div className="flex flex-wrap gap-2">
              {[
                { href: '/downloads/bootloader-v4.bin', label: 'bootloader-v4.bin', addr: '0x0' },
                { href: '/downloads/partitions-v4.bin', label: 'partitions-v4.bin', addr: '0x8000' },
                { href: '/downloads/boot_app0.bin', label: 'boot_app0.bin', addr: '0xe000' },
              ].map((f) => (
                <a
                  key={f.href}
                  href={f.href}
                  className="inline-flex items-center gap-2 px-3 py-1.5 rounded-lg bg-bg border border-border text-xs text-text-secondary hover:border-accent/40 hover:text-accent transition-colors"
                >
                  <code>{f.label}</code>
                  <span className="text-text-secondary/60">{f.addr}</span>
                </a>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
