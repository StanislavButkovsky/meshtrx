'use client';

import ApkDownload from '@/components/download/ApkDownload';
import FirmwareDownload from '@/components/download/FirmwareDownload';
import QrCode from '@/components/download/QrCode';
import InstallSteps from '@/components/download/InstallSteps';
import Changelog from '@/components/download/Changelog';
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
        <FirmwareDownload />
        <InstallSteps />
        <Changelog />
      </div>
    </div>
  );
}
