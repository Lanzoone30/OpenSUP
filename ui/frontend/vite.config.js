import { defineConfig } from 'vite';
import injectHTML from 'vite-plugin-html-inject';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

// CMakeLists.txt is the single source of the version (project(VERSION ...)).
const cmake = fileURLToPath(new URL('../../CMakeLists.txt', import.meta.url));
const version = readFileSync(cmake, 'utf8').match(/project\s*\(\s*OpenSUP\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)/)?.[1];
if (!version) throw new Error(`version not found in ${cmake}`);

// Splits index.html into per-component partials via <load src="..." />
// Everything else keeps Vite's defaults so the emitted CSS/JS stay identical.
export default defineConfig({
  plugins: [injectHTML()],
  define: { __APP_VERSION__: JSON.stringify(version) },
});
