'use client';

import { useLanguage } from '@/components/LanguageProvider';
import FlashWizard from '@/components/flash/FlashWizard';

export default function FlashPage() {
  const { t } = useLanguage();

  return (
    <div className="max-w-4xl mx-auto px-4 py-12">
      <h1 className="text-3xl font-bold mb-2">{t('flash.title')}</h1>
      <p className="text-text-secondary mb-8">
        {t('flash.desc')}
      </p>
      <FlashWizard />
    </div>
  );
}
