// settings.js — Persisted UI settings via Go.

import { $ } from "./dom.js";
import { state } from "./state.js";
import { go } from "./bridge.js";
import { setLang } from "./i18n.js";
import { applyTheme } from "./theme.js";
import { syncDropdown } from "./dropdown.js";

export async function loadSettings() {
  const app = go();
  if (app?.LoadSettings) {
    try {
      const s = await app.LoadSettings();
      if (s) {
        // Wails serializes with the Go json tags (lowercase), matching
        // the generated models.ts — NOT the Go field names.
        setLang(s.language || 0);
        $("cmb_language").value = String(s.language || 0);
        syncDropdown("dd_language");
        applyTheme(s.theme || 0);
        $("cmb_theme").value = String(s.theme || 0);
        syncDropdown("dd_theme");
        if (s.last_bdn_dir)  state.lastBDNDir = s.last_bdn_dir;
        if (s.last_output_dir)  state.lastOutDir = s.last_output_dir;
        // Engine Options: restore only keys present in the file.
        const flags = {
          allow_normal: "chk_allow_normal",
          prefer_normal: "chk_prefer_normal",
          overlap: "chk_overlap",
          full_palette: "chk_full_palette",
          alternate_oids: "chk_alternate_oids",
          both_formats: "chk_both_formats",
          ignore_resolution: "chk_ignore_resolution",
          tight_ods: "chk_tight_ods",
        };
        for (const [key, id] of Object.entries(flags)) {
          if (s[key] !== undefined) $(id).checked = s[key];
        }
        const qz = $("combo_quantizer");
        if (s.quantizer !== undefined &&
            [...qz.options].some((o) => o.value === String(s.quantizer))) {
          qz.value = String(s.quantizer);
        }
        // Collapsed card state is not restored: the markup defines
        // Parameters/Engine expanded and Advanced collapsed on every start.
      }
    } catch (err) {
      console.error("LoadSettings:", err);
    }
  }
}

export async function saveSettings() {
  const app = go();
  if (!app?.SaveSettings) return;
  try {
    await app.SaveSettings({
      Language:   parseInt($("cmb_language").value),
      Theme:      parseInt($("cmb_theme").value),
      LastBDNDir: state.lastBDNDir || "",
      LastOutDir: state.lastOutDir || "",
      AllowNormal:      $("chk_allow_normal").checked,
      PreferNormal:     $("chk_prefer_normal").checked,
      Overlap:          $("chk_overlap").checked,
      FullPalette:      $("chk_full_palette").checked,
      AlternateOids:    $("chk_alternate_oids").checked,
      BothFormats:      $("chk_both_formats").checked,
      IgnoreResolution: $("chk_ignore_resolution").checked,
      TightOds:         $("chk_tight_ods").checked,
      Quantizer:        parseInt($("combo_quantizer").value),
    });
  } catch (err) {
    console.error("SaveSettings:", err);
  }
}
