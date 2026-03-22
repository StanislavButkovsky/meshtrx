'use client';

import { SITE } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';

export default function Footer() {
  const { t } = useLanguage();

  return (
    <footer className="border-t border-border mt-auto">
      <div className="max-w-6xl mx-auto px-4 py-8 flex flex-col md:flex-row items-center justify-between gap-4 text-sm text-text-secondary">
        <div className="flex items-center gap-2">
          <span className="font-bold text-text-primary">
            Mesh<span className="text-accent">TRX</span>
          </span>
          <span>— {t('footer.tagline')}</span>
        </div>
        <div className="flex items-center gap-4 flex-wrap justify-center">
          <a href={SITE.github} target="_blank" rel="noopener noreferrer" className="hover:text-accent transition-colors">
            GitHub
          </a>
          <span>•</span>
          <a href={SITE.telegram} target="_blank" rel="noopener noreferrer" className="hover:text-accent transition-colors">
            Telegram
          </a>
          <span>•</span>
          <a href={SITE.licenseUrl} target="_blank" rel="noopener noreferrer" className="hover:text-accent transition-colors">
            {SITE.license}
          </a>
          <span>•</span>
          <span>meshtrx.com / meshtrx.ru</span>
        </div>
      </div>
    </footer>
  );
}
