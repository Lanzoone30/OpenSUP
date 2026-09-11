// app.js — Main entry point. Vanilla JS, no framework.
// Orchestrates the per-component modules and shared utilities.

import './app.css';
import { t } from './shared/i18n.js';
import { EventsOn, OnFileDrop } from '../wailsjs/runtime/runtime.js';
import folderIcon from './components/left-control-panel/assets/folder.svg?raw';
import { $ } from './shared/dom.js';
import { state } from './shared/state.js';
import { appendLog, updateLogCount, updateLogPlaceholder, wireLogPanel } from './components/log-panel/log-panel.js';
import { setStatus, setProgressClip, updateProgress } from './components/progress/progress.js';
import { applyBDNPath, updateOutputActions, updateControlsFade, wireControlsFade, wireCardToggles, populateThreads } from './components/left-control-panel/left-control-panel.js';
import { setEncodingState, updateReadyState, wireButtons, wireOptions } from './components/btns-actions/btns-actions.js';
import { syncDropdown } from './shared/dropdown.js';
import { loadSettings } from './shared/settings.js';
import { wireUiActions } from './components/ui-actions/ui-actions.js';

// ── i18n: translate all [data-i18n] elements ──
function applyI18n() {
  document.querySelectorAll("[data-i18n]").forEach((el) => {
    // Keep the selected file names when switching language: only translate
    // the placeholder, never overwrite a chosen path.
    if (el.id === "lbl_bdn_file" && state.inputPath) return;
    if (el.id === "lbl_output_file" && state.outputPath) return;
    // The status LED label is dynamic (Working/Finished/etc): keep its
    // current state, it is re-applied at the end of this function.
    if (el.id === "lbl_progress_text") return;
    if (el.classList.contains("help-tip")) {
      // Native tooltip (title): never clipped by the scrolling column.
      el.title = t(el.dataset.i18n);
      return;
    }
    el.textContent = t(el.dataset.i18n);
  });
  // Tooltips for checkboxes
  const tips = {
    chk_allow_normal: "tipAllowNormal",
    chk_prefer_normal: "tipPreferNormal",
    chk_full_palette: "tipFullPalette",
    chk_both_formats: "tipBothFormats",
    chk_overlap: "tipOverlapBuf",
    chk_alternate_oids: "tipAlternateOids",
    chk_ignore_res: "tipIgnoreRes",
  };
  for (const [id, key] of Object.entries(tips)) {
    const el = $(id);
    if (el && el.parentElement) el.parentElement.title = t(key);
  }
  // Tooltips for selects
  const cs = $("combo_colorspace");
  if (cs) cs.title = t("colorSpaceTip");
  const qz = $("combo_quantizer");
  if (qz) qz.title = t("quantizerTip");
  const rv = $("btn_reveal_output");
  if (rv) rv.title = t("openOutputFolder");
  rv?.setAttribute("aria-label", t("openOutputFolder"));
  const fl = $("btn_log_follow");
  if (fl) {
    fl.title = t("jumpToLive");
    fl.setAttribute("aria-label", t("jumpToLive"));
  }
  // Update log count + empty-state text
  updateLogCount();
  updateLogPlaceholder();
  // Re-translate the status LED label in the new language.
  if (state.statusMode) setStatus(state.statusMode);
}

// ── Wire up Wails engine events ──
function wireEngineEvents() {
  EventsOn("engine:log", (data) => {
    if (data && data.level && data.msg) {
      appendLog(data.level, data.msg);
    }
  });

  EventsOn("engine:progress", (data) => {
    if (data) {
      updateProgress(data.percent || 0, data.epoch || 0, data.total || 0);
    }
  });

  EventsOn("engine:done", (data) => {
    setEncodingState(false);
    const fill = $("progress_fill");
    fill.classList.remove("active");
    if (data && data.success) {
      setProgressClip(1);
      setStatus("finished");
    } else if (data && data.cancelled) {
      setStatus("aborted");
      fill.classList.add("failed");
    } else {
      setStatus("failed");
      fill.classList.add("failed");
    }
    updateReadyState();
  });
}

// ── Wire up OS drag & drop ──
function wireDragAndDrop() {
  OnFileDrop((x, y, paths) => {
    if (!paths?.length) return;
    const el = document.elementFromPoint(x, y);
    const zone = el?.closest(".drop-zone");
    if (!zone) return;
    if (zone.dataset.drop === "bdn") {
      const path = paths.find((p) => p.toLowerCase().endsWith(".xml")) || paths[0];
      applyBDNPath(path);
    }
  });
}

// ── Wire up DOM events ──
function wireDomEvents() {
  wireUiActions(applyI18n);
  wireButtons();
  wireOptions();
  wireLogPanel();
  wireCardToggles();
}

// Inyect SVG icons (loaded ?raw from components/left-control-panel/assets/) into their hosts.
function injectIcons() {
  const icons = { folder: folderIcon };
  document.querySelectorAll("[data-icon]").forEach((el) => {
    const svg = icons[el.dataset.icon];
    if (svg) el.innerHTML = svg;
  });
}

async function init() {
  injectIcons();
  wireDomEvents();
  wireEngineEvents();
  wireDragAndDrop();
  await loadSettings();
  applyI18n();
  await populateThreads();
  updateControlsFade();
  updateReadyState();
  updateOutputActions();
  syncDropdown("dd_quantizer");
  syncDropdown("dd_language");
  syncDropdown("dd_theme");
  document.title = t("windowTitle");
  wireControlsFade();
}

init();
