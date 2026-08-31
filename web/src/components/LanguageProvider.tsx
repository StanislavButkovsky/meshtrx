'use client';

import { createContext, useContext, useState, useEffect, useCallback } from 'react';
import { type Locale, type TranslationKey, translations } from '@/lib/i18n';
import { SITE_LOCALE } from '@/lib/site-locale';

interface LanguageContextType {
  locale: Locale;
  setLocale: (locale: Locale) => void;
  t: (key: TranslationKey) => string;
}

const LanguageContext = createContext<LanguageContextType>({
  locale: SITE_LOCALE,
  setLocale: () => {},
  t: (key) => translations[key]?.[SITE_LOCALE] ?? key,
});

export function LanguageProvider({ children }: { children: React.ReactNode }) {
  // Начинаем с языка сборки: он же в отданном HTML, и расхождение вызвало бы
  // мигание при гидрации. Сохранённый выбор человека подхватывается следом и
  // перебивает домен — тот задаёт только значение по умолчанию.
  const [locale, setLocaleState] = useState<Locale>(SITE_LOCALE);

  useEffect(() => {
    const saved = localStorage.getItem('meshtrx-lang') as Locale | null;
    if (saved && (saved === 'ru' || saved === 'en')) {
      setLocaleState(saved);
      document.documentElement.lang = saved;
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
