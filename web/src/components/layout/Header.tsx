'use client';

import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { useState } from 'react';
import { useLanguage } from '@/components/LanguageProvider';
import type { TranslationKey } from '@/lib/i18n';
import MobileMenu from './MobileMenu';

const NAV_ITEMS: { href: string; key: TranslationKey }[] = [
  { href: '/', key: 'nav.home' },
  { href: '/download/', key: 'nav.download' },
  { href: '/flash/', key: 'nav.flash' },
  { href: '/docs/', key: 'nav.docs' },
  { href: '/about/', key: 'nav.about' },
];

export default function Header() {
  const pathname = usePathname();
  const [mobileOpen, setMobileOpen] = useState(false);
  const { locale, setLocale, t } = useLanguage();

  return (
    <header className="fixed top-0 left-0 right-0 z-50 bg-bg/80 backdrop-blur-md border-b border-border">
      <div className="max-w-6xl mx-auto px-4 h-16 flex items-center justify-between">
        <Link href="/" className="flex items-center gap-2 group">
          <div className="w-8 h-8 rounded-lg bg-accent/10 border border-accent/30 flex items-center justify-center group-hover:bg-accent/20 transition-colors">
            <span className="text-accent font-bold text-sm">M</span>
          </div>
          <span className="font-bold text-lg text-text-primary">
            Mesh<span className="text-accent">TRX</span>
          </span>
        </Link>

        <div className="hidden md:flex items-center gap-1">
          <nav className="flex items-center gap-1">
            {NAV_ITEMS.map((link) => {
              const isActive = pathname === link.href || pathname === link.href.replace(/\/$/, '');
              return (
                <Link
                  key={link.href}
                  href={link.href}
                  className={`px-3 py-2 rounded-lg text-sm transition-colors ${
                    isActive
                      ? 'text-accent bg-accent/10'
                      : 'text-text-secondary hover:text-text-primary hover:bg-bg-hover'
                  }`}
                >
                  {t(link.key)}
                </Link>
              );
            })}
          </nav>
          <div className="ml-2 flex items-center border border-border rounded-lg overflow-hidden text-xs">
            <button
              onClick={() => setLocale('ru')}
              className={`px-2.5 py-1.5 transition-colors ${
                locale === 'ru' ? 'bg-accent/10 text-accent' : 'text-text-secondary hover:text-text-primary'
              }`}
            >
              RU
            </button>
            <button
              onClick={() => setLocale('en')}
              className={`px-2.5 py-1.5 transition-colors ${
                locale === 'en' ? 'bg-accent/10 text-accent' : 'text-text-secondary hover:text-text-primary'
              }`}
            >
              EN
            </button>
          </div>
        </div>

        <div className="md:hidden flex items-center gap-2">
          <div className="flex items-center border border-border rounded-lg overflow-hidden text-xs">
            <button
              onClick={() => setLocale('ru')}
              className={`px-2 py-1.5 transition-colors ${
                locale === 'ru' ? 'bg-accent/10 text-accent' : 'text-text-secondary'
              }`}
            >
              RU
            </button>
            <button
              onClick={() => setLocale('en')}
              className={`px-2 py-1.5 transition-colors ${
                locale === 'en' ? 'bg-accent/10 text-accent' : 'text-text-secondary'
              }`}
            >
              EN
            </button>
          </div>
          <button
            onClick={() => setMobileOpen(!mobileOpen)}
            className="p-2 text-text-secondary hover:text-text-primary"
            aria-label="Menu"
          >
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
              {mobileOpen ? (
                <path d="M18 6L6 18M6 6l12 12" />
              ) : (
                <path d="M3 12h18M3 6h18M3 18h18" />
              )}
            </svg>
          </button>
        </div>
      </div>

      <MobileMenu open={mobileOpen} onClose={() => setMobileOpen(false)} navItems={NAV_ITEMS} />
    </header>
  );
}
