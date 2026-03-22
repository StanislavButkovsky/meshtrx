'use client';

import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import type { Components } from 'react-markdown';

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

export default function DocContent({ content }: { content: string }) {
  return (
    <div className="prose-mesh">
      <ReactMarkdown remarkPlugins={[remarkGfm]} components={components}>
        {content}
      </ReactMarkdown>
    </div>
  );
}
