'use client';

import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { useLanguage } from '@/components/LanguageProvider';
import type { TranslationKey } from '@/lib/i18n';

interface Props {
  open: boolean;
  onClose: () => void;
  navItems: { href: string; key: TranslationKey }[];
}

export default function MobileMenu({ open, onClose, navItems }: Props) {
  const pathname = usePathname();
  const { t } = useLanguage();

  if (!open) return null;

  return (
    <div className="md:hidden border-t border-border bg-bg/95 backdrop-blur-md">
      <nav className="flex flex-col p-4 gap-1">
        {navItems.map((link) => {
          const isActive = pathname === link.href || pathname === link.href.replace(/\/$/, '');
          return (
            <Link
              key={link.href}
              href={link.href}
              onClick={onClose}
              className={`px-4 py-3 rounded-lg text-sm transition-colors ${
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
    </div>
  );
}
