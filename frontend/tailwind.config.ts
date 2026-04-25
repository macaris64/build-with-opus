import type { Config } from 'tailwindcss'

export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  darkMode: 'class',
  theme: {
    extend: {
      fontFamily: {
        mono: ['"JetBrains Mono"', 'ui-monospace', 'monospace'],
      },
      colors: {
        space: {
          950: '#020408',
          900: '#080d14',
          800: '#0f1a24',
          700: '#1a2a3a',
        },
      },
      animation: {
        'ping-slow': 'ping 2s cubic-bezier(0, 0, 0.2, 1) infinite',
      },
      keyframes: {
        'flash-green': {
          '0%, 100%': { borderColor: 'transparent' },
          '50%': { borderColor: '#00ff88' },
        },
      },
    },
  },
  plugins: [],
} satisfies Config
