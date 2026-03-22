'use client';

import Link from 'next/link';
import { SITE } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';
import MeshDiagram from './MeshDiagram';

export default function HeroSection() {
  const { t } = useLanguage();

  return (
    <section className="relative overflow-hidden">
      <div className="absolute inset-0 bg-gradient-to-b from-accent/5 to-transparent pointer-events-none" />
      <div className="max-w-6xl mx-auto px-4 py-20 md:py-32">
        <div className="text-center mb-12">
          <div className="inline-flex items-center gap-2 px-4 py-1.5 rounded-full bg-yellow-500/10 border border-yellow-500/30 text-yellow-400 text-sm mb-6">
            <span className="w-2 h-2 rounded-full bg-yellow-400 animate-pulse" />
            {t('hero.badge')}
          </div>
          <h1 className="text-4xl md:text-6xl font-bold mb-6">
            Mesh<span className="text-accent glow-text">TRX</span>
          </h1>
          <p className="text-xl md:text-2xl text-text-secondary max-w-2xl mx-auto mb-8">
            {t('hero.tagline')}
          </p>
          <p className="text-text-secondary max-w-xl mx-auto mb-10">
            {t('hero.desc')}
          </p>
          <div className="flex flex-wrap justify-center gap-4">
            <Link
              href="/download/"
              className="px-6 py-3 bg-accent text-bg font-semibold rounded-lg hover:bg-accent-dim transition-colors glow"
            >
              {t('hero.download')}
            </Link>
            <Link
              href="/flash/"
              className="px-6 py-3 border border-accent text-accent rounded-lg hover:bg-accent/10 transition-colors"
            >
              {t('hero.flash')}
            </Link>
            <a
              href={SITE.github}
              target="_blank"
              rel="noopener noreferrer"
              className="px-6 py-3 border border-border text-text-secondary rounded-lg hover:border-text-secondary hover:text-text-primary transition-colors"
            >
              GitHub
            </a>
          </div>
        </div>
        <MeshDiagram />
      </div>
    </section>
  );
}
