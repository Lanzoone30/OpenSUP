// ui-actions.js — Language/theme segmented controls and quantizer dropdown.

import { $ } from "../../shared/dom.js";
import { setLang } from "../../shared/i18n.js";
import { applyTheme } from "../../shared/theme.js";
import { wireDropdown, closeDropdowns } from "../../shared/dropdown.js";
import { syncSeg } from "../../shared/segmented.js";
import { saveSettings } from "../../shared/settings.js";

export function wireUiActions(applyI18n) {
  $("cmb_language").addEventListener("change", () => {
    setLang(parseInt($("cmb_language").value));
    applyI18n();
    syncSeg("seg_language");
    saveSettings();
  });

  $("cmb_theme").addEventListener("change", () => {
    applyTheme(parseInt($("cmb_theme").value));
    syncSeg("seg_theme");
    saveSettings();
  });

  wireSeg("seg_language", "cmb_language");
  wireSeg("seg_theme", "cmb_theme");

  // Custom dropdown: open/close and pick on click; Esc/outer click close.
  wireDropdown("dd_quantizer");
  document.addEventListener("click", (e) => {
    if (!e.target.closest(".dropdown")) closeDropdowns();
  });
  document.addEventListener("keydown", (e) => {
    if (e.key === "Escape") closeDropdowns();
  });
}

function wireSeg(segId, selectId) {
  const seg = $(segId);
  const sel = $(selectId);
  if (!seg || !sel) return;
  seg.addEventListener("click", (e) => {
    const opt = e.target.closest(".seg-opt");
    if (!opt || sel.value === opt.dataset.val) return;
    sel.value = opt.dataset.val;
    sel.dispatchEvent(new Event("change"));
    syncSeg(segId);
  });
}
