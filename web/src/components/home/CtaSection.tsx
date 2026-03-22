'use client';

import Link from 'next/link';
import { SITE } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';

export default function CtaSection() {
  const { t } = useLanguage();

  return (
    <section className="max-w-6xl mx-auto px-4 py-20">
      <div className="text-center p-10 rounded-2xl bg-gradient-to-br from-accent/5 to-accent/10 border border-accent/20">
        <h2 className="text-2xl md:text-3xl font-bold mb-4">
          {t('cta.title')}
        </h2>
        <p className="text-text-secondary mb-8 max-w-md mx-auto">
          {t('cta.desc')}
        </p>
        <div className="flex flex-wrap justify-center gap-4">
          <Link
            href="/download/"
            className="px-8 py-3 bg-accent text-bg font-semibold rounded-lg hover:bg-accent-dim transition-colors glow"
          >
            {t('cta.apk')}
          </Link>
          <Link
            href="/flash/"
            className="px-8 py-3 border border-accent text-accent rounded-lg hover:bg-accent/10 transition-colors"
          >
            {t('cta.flash')}
          </Link>
          <a
            href={SITE.github}
            target="_blank"
            rel="noopener noreferrer"
            className="px-8 py-3 border border-border text-text-secondary rounded-lg hover:border-text-secondary hover:text-text-primary transition-colors"
          >
            GitHub
          </a>
        </div>
      </div>
    </section>
  );
}
