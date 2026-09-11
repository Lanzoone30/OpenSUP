// dom.js — DOM lookup shortcuts.

export const $ = (id) => document.getElementById(id);

export function numOr(el, def) {
  if (!el) return def;
  const v = parseFloat(el.value);
  return Number.isFinite(v) ? v : def;
}
