import { fileURLToPath } from 'node:url';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import { defineConfig } from 'vite';

const page = (name: string) => fileURLToPath(new URL(`./${name}.html`, import.meta.url));

export default defineConfig({
  // Served from https://app.aiassistant/ (a folder mapping), so keep every URL relative.
  base: './',
  plugins: [svelte()],
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    target: 'chrome120',
    chunkSizeWarningLimit: 1500,
    rollupOptions: {
      input: { bar: page('bar'), settings: page('settings') },
    },
  },
  // Point the app at this with AIASSISTANT_DEV_URL=http://localhost:5747 for hot reload.
  server: { port: 5747, strictPort: true },
});
