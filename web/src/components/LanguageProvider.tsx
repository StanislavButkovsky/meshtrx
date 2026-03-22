'use client';

import { createContext, useContext, useState, useEffect, useCallback } from 'react';
import { type Locale, type TranslationKey, translations } from '@/lib/i18n';

interface LanguageContextType {
  locale: Locale;
  setLocale: (locale: Locale) => void;
  t: (key: TranslationKey) => string;
}

const LanguageContext = createContext<LanguageContextType>({
  locale: 'ru',
  setLocale: () => {},
  t: (key) => translations[key]?.ru ?? key,
});

export function LanguageProvider({ children }: { children: React.ReactNode }) {
  const [locale, setLocaleState] = useState<Locale>('ru');

  useEffect(() => {
    const saved = localStorage.getItem('meshtrx-lang') as Locale | null;
    if (saved && (saved === 'ru' || saved === 'en')) {
      setLocaleState(saved);
    }
  }, []);

  const setLocale = useCallback((l: Locale) => {
    setLocaleState(l);
    localStorage.setItem('meshtrx-lang', l);
    document.documentElement.lang = l;
  }, []);

  const t = useCallback((key: TranslationKey): string => {
    return translations[key]?.[locale] ?? key;
  }, [locale]);

  return (
    <LanguageContext.Provider value={{ locale, setLocale, t }}>
      {children}
    </LanguageContext.Provider>
  );
}

export function useLanguage() {
  return useContext(LanguageContext);
}
