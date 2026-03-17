import { fileURLToPath, URL } from 'node:url'
import { defineConfig } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'

export default defineConfig({
  plugins: [svelte()],
  base: './',
  build: {
    outDir: fileURLToPath(new URL('../view', import.meta.url)),
    emptyOutDir: true,
    assetsDir: 'assets',
  },
})
