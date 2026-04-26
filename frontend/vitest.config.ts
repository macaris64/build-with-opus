import { defineConfig } from 'vitest/config'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  test: {
    globals: true,
    environment: 'jsdom',
    setupFiles: ['./src/test/setup.ts'],
    coverage: {
      provider: 'v8',
      reporter: ['text', 'lcov'],
      // Scope to new Titan files + modified existing files only
      include: [
        'src/store/titanStore.ts',
        'src/store/marsStore.ts',
        'src/store/uiStore.ts',
        'src/lib/titanCommGenerator.ts',
        'src/lib/apidRegistry.ts',
        'src/lib/hkDecoder.ts',
        'src/hooks/useTitanBackendSync.ts',
        'src/panels/TitanPacketPanel.tsx',
      ],
      thresholds: { lines: 80, functions: 80, branches: 80 },
    },
  },
})
