'use client';

import Link from 'next/link';
import ApkDownload from '@/components/download/ApkDownload';
import FirmwareDownload from '@/components/download/FirmwareDownload';
import QrCode from '@/components/download/QrCode';
import InstallSteps from '@/components/download/InstallSteps';
import Changelog from '@/components/download/Changelog';
import Feedback from '@/components/download/Feedback';
import { useLanguage } from '@/components/LanguageProvider';

export default function DownloadPage() {
  const { t } = useLanguage();

  return (
    <div className="max-w-4xl mx-auto px-4 py-12">
      <h1 className="text-3xl font-bold mb-8">
        {t('dl.title')}
      </h1>
      <div className="grid gap-6">
        <div className="grid md:grid-cols-3 gap-6">
          <div className="md:col-span-2">
            <ApkDownload />
          </div>
          <QrCode />
        </div>

        {/* iOS link */}
        <Link href="/ios/" className="block p-4 rounded-xl bg-bg-card border border-border hover:border-blue-500/50 transition-colors">
          <div className="flex items-center gap-3">
            <span className="text-2xl">🍎</span>
            <div className="flex-1">
              <span className="font-semibold text-text-primary">{t('ios.title')}</span>
              <span className="ml-2 px-2 py-0.5 rounded bg-blue-500/10 text-blue-400 text-xs font-medium">
                {t('ios.badge')}
              </span>
            </div>
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" className="text-text-secondary">
              <polyline points="9 18 15 12 9 6" />
            </svg>
          </div>
        </Link>

        <FirmwareDownload />
        <Feedback />
        <InstallSteps />
        <Changelog />
      </div>
    </div>
  );
}
