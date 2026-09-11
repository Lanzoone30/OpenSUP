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
    });
  } catch (err) {
    console.error("SaveSettings:", err);
  }
}
