// ui-actions.js — Language/theme controls and custom-dropdown wiring.

import { $ } from "../../shared/dom.js";
import { setLang } from "../../shared/i18n.js";
import { applyTheme } from "../../shared/theme.js";
import { syncDropdown, wireDropdown, closeDropdowns } from "../../shared/dropdown.js";
import { saveSettings } from "../../shared/settings.js";

export function wireUiActions(applyI18n) {
  $("cmb_language").addEventListener("change", () => {
    setLang(parseInt($("cmb_language").value));
    applyI18n();
    syncDropdown("dd_language");
    syncDropdown("dd_theme"); // theme li labels are translated on switch
    saveSettings();
  });

  $("cmb_theme").addEventListener("change", () => {
    applyTheme(parseInt($("cmb_theme").value));
    saveSettings();
  });

  // Custom dropdown: open/close and pick on click; Esc/outer click close.
  wireDropdown("dd_quantizer");
  wireDropdown("dd_language");
  wireDropdown("dd_theme");
  document.addEventListener("click", (e) => {
    if (!e.target.closest(".dropdown")) closeDropdowns();
  });
  document.addEventListener("keydown", (e) => {
    if (e.key === "Escape") closeDropdowns();
  });
}
