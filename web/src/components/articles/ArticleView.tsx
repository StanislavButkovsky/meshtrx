'use client';

import Link from 'next/link';
import { defaultUrlTransform } from 'react-markdown';
import type { Components } from 'react-markdown';
import DocContent from '@/components/docs/DocContent';
import PacketDiagram from '@/components/articles/PacketDiagram';
import { useLanguage } from '@/components/LanguageProvider';
import { readingMinutes, type Article } from '@/content/articles';

// Иллюстрации живут в коде, а не в markdown: схема пакета должна пересчитываться
// по тем же числам, что и текст, и переключаться на второй язык вместе с ним.
// В тексте статьи стоит обычная картинка с адресом figure:имя — так автор видит
// в markdown, где будет иллюстрация, а рисует её React.
const FIGURES: Record<string, () => JSX.Element> = {
  packet: PacketDiagram,
};

// react-markdown по умолчанию вычищает адреса с незнакомой схемой, и figure:
// в том числе — картинка приходила бы с пустым src. Пропускаем ровно свою
// схему, всё остальное чистится как раньше.
const allowFigureUrls = (url: string) =>
  url.startsWith('figure:') ? url : defaultUrlTransform(url);

const figureRenderer: Components = {
  img: ({ src, alt }) => {
    const name = typeof src === 'string' && src.startsWith('figure:') ? src.slice(7) : null;
    const Figure = name ? FIGURES[name] : undefined;
    if (!Figure) return <img src={src} alt={alt} />;

    return (
      <figure className="my-8 p-4 rounded-xl bg-bg-card border border-border">
        <Figure />
        {alt && <figcaption className="text-xs text-text-secondary mt-3 text-center">{alt}</figcaption>}
      </figure>
    );
  },
  // Иллюстрация приходит внутри абзаца, а <figure> внутри <p> ломает разметку:
  // браузер закрывает абзац сам, и React ругается на несовпадение при гидрации.
  p: ({ children }) => {
    const only = Array.isArray(children) ? children.filter((c) => c !== '\n') : [children];
    const isFigure =
      only.length === 1 &&
      typeof only[0] === 'object' &&
      only[0] !== null &&
      (only[0] as { props?: { src?: unknown } }).props?.src !== undefined;
    return isFigure ? <>{children}</> : <p>{children}</p>;
  },
};

export default function ArticleView({ article }: { article: Article }) {
  const { locale, t } = useLanguage();

  return (
    <div className="max-w-3xl mx-auto px-4 py-12">
      <Link
        href="/articles/"
        className="text-sm text-text-secondary hover:text-accent transition-colors"
      >
        ← {t('articles.back')}
      </Link>

      <h1 className="text-3xl font-bold mt-6 mb-3">{article.title[locale]}</h1>
      <div className="flex items-center gap-2 text-xs text-text-secondary mb-8">
        <time dateTime={article.date}>{article.date}</time>
        <span>•</span>
        <span>
          {readingMinutes(article.body[locale])} {t('articles.reading')}
        </span>
      </div>

      <article>
        <DocContent content={article.body[locale]} extra={figureRenderer} urlTransform={allowFigureUrls} />
      </article>
    </div>
  );
}
