import type { Config } from 'tailwindcss';

const config: Config = {
  content: [
    './src/components/**/*.{js,ts,jsx,tsx,mdx}',
    './src/app/**/*.{js,ts,jsx,tsx,mdx}',
  ],
  theme: {
    extend: {
      colors: {
        bg: '#141414',
        'bg-card': '#1e1e1e',
        'bg-hover': '#2a2a2a',
        accent: '#4ade80',
        'accent-dim': '#22c55e',
        'text-primary': '#f5f5f5',
        'text-secondary': '#a3a3a3',
        border: '#333333',
      },
      fontFamily: {
        mono: ['JetBrains Mono', 'Fira Code', 'monospace'],
      },
    },
  },
  plugins: [],
};
export default config;
