'use client';

import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import type { Components, UrlTransform } from 'react-markdown';

function slugify(text: string): string {
  return text
    .toLowerCase()
    .replace(/[^\wа-яё\s-]/gi, '')
    .replace(/\s+/g, '-');
}

const components: Components = {
  h1: ({ children }) => {
    const text = String(children);
    const id = slugify(text);
    return <h1 id={id}>{children}</h1>;
  },
  h2: ({ children }) => {
    const text = String(children);
    const id = slugify(text);
    return <h2 id={id}>{children}</h2>;
  },
  h3: ({ children }) => {
    const text = String(children);
    const id = slugify(text);
    return <h3 id={id}>{children}</h3>;
  },
};

interface Props {
  content: string;
  /** Добавочные обработчики разметки: статьи подставляют так свои иллюстрации. */
  extra?: Components;
  /**
   * Своя обработка адресов. По умолчанию react-markdown вычищает всё, кроме
   * известных ему схем, — это нужная защита, и снимать её целиком нельзя.
   */
  urlTransform?: UrlTransform;
}

export default function DocContent({ content, extra, urlTransform }: Props) {
  return (
    <div className="prose-mesh">
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        components={{ ...components, ...extra }}
        urlTransform={urlTransform}
      >
        {content}
      </ReactMarkdown>
    </div>
  );
}
