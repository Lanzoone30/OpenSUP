// theme.js — data-theme attribute control (0=System, 1=Light, 2=Dark).

export function applyTheme(theme) {
  const themes = ["system", "light", "dark"];
  document.documentElement.setAttribute("data-theme", themes[theme] || "system");
}
