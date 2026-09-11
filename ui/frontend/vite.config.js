import { defineConfig } from 'vite';
import injectHTML from 'vite-plugin-html-inject';

// Splits index.html into per-component partials via <load src="..." />
// Everything else keeps Vite's defaults so the emitted CSS/JS stay identical.
export default defineConfig({
  plugins: [injectHTML()],
});
