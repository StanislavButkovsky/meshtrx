import type { Config } from 'tailwindcss';

const config: Config = {
  content: [
    './src/components/**/*.{js,ts,jsx,tsx,mdx}',
    './src/app/**/*.{js,ts,jsx,tsx,mdx}',
  ],
  theme: {
    extend: {
      colors: {
        bg: '#0E1A33',
        'bg-card': '#14233F',
        'bg-hover': '#1B2C4E',
        accent: '#FABE14',
        'accent-dim': '#E0A800',
        'text-primary': '#EEF2FA',
        'text-secondary': '#A9B4CC',
        secondary: '#5B9BFF',
        border: '#22355A',
      },
      fontFamily: {
        mono: ['JetBrains Mono', 'Fira Code', 'monospace'],
      },
    },
  },
  plugins: [],
};
export default config;
