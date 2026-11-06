// app.js — Main entry point. Vanilla JS, no framework.
// Orchestrates the per-component modules and shared utilities.

import './app.css';
import { t, getLang } from './shared/i18n.js';
import { EventsOn, OnFileDrop } from '../wailsjs/runtime/runtime.js';
import folderIcon from './components/left-control-panel/assets/folder.svg?raw';
import eraserIcon from './components/left-control-panel/assets/eraser.svg?raw';
import monitorIcon from './components/ui-actions/assets/monitor.svg?raw';
import sunIcon from './components/ui-actions/assets/sun.svg?raw';
import moonIcon from './components/ui-actions/assets/moon.svg?raw';
import { $ } from './shared/dom.js';
import { state } from './shared/state.js';
import { appendLog, updateLogCount, updateLogPlaceholder, wireLogPanel, updateLogButtons } from './components/log-panel/log-panel.js';
import { setStatus, setProgressClip, updateProgress } from './components/progress/progress.js';
import { applyBDNPath, updateOutputActions, updateControlsFade, wireControlsFade, wireCardToggles, populateThreads, updateScopeHint, flashOutputReady } from './components/left-control-panel/left-control-panel.js';
import { setEncodingState, updateReadyState, wireButtons, wireOptions } from './components/btns-actions/btns-actions.js';
import { syncSeg } from './shared/segmented.js';
import { syncDropdown, refreshDropdown } from './shared/dropdown.js';
import { loadSettings } from './shared/settings.js';
import { wireUiActions } from './components/ui-actions/ui-actions.js';

// ── i18n: translate all [data-i18n] elements ──
function applyI18n() {
  document.documentElement.lang = getLang() === 1 ? "es" : "en";
  document.querySelectorAll("[data-i18n]").forEach((el) => {
    // Keep the selected file names when switching language: only translate
    // the placeholder, never overwrite a chosen path.
    if (el.id === "lbl_bdn_file" && state.inputPath) return;
    if (el.id === "lbl_output_file" && state.outputPath) return;
    // The status LED label is dynamic (Working/Finished/etc): keep its
    // current state, it is re-applied at the end of this function.
    if (el.id === "lbl_progress_text") return;
    if (el.id === "btn_encode" && state.isEncoding) {
      el.textContent = t("encodeRunning");
      return;
    }
    if (el.classList.contains("help-tip")) {
      // Native tooltip (title): never clipped by the scrolling column.
      el.title = t(el.dataset.i18n);
      el.setAttribute("aria-label", el.title);
      return;
    }
    el.textContent = t(el.dataset.i18n);
  });
  // Checkbox labels carry their tip as a native tooltip on the text itself.
  document.querySelectorAll("[data-i18n-tip]").forEach((el) => {
    el.title = t(el.dataset.i18nTip);
  });
  const themeTips = { 0: "themeSystem", 1: "themeLight", 2: "themeDark" };
  document.querySelectorAll("#seg_theme .seg-opt").forEach((el) => {
    const label = t(themeTips[el.dataset.val]);
    el.title = label;
    el.setAttribute("aria-label", label);
  });
  // Tooltips for selects
  const cs = $("combo_colorspace");
  if (cs) cs.title = t("colorSpaceTip");
  const qz = $("combo_quantizer");
  if (qz) qz.title = t("quantizerTip");
  const rv = $("btn_reveal_output");
  if (rv) rv.title = t("openOutputFolder");
  rv?.setAttribute("aria-label", t("openOutputFolder"));
  const cl = $("btn_clear_scope");
  if (cl) {
    cl.title = t("clearSelections");
    cl.setAttribute("aria-label", t("clearSelections"));
  }
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
  updateScopeHint();
  // Closed dropdown labels mirror their translated option text.
  document.querySelectorAll(".dropdown").forEach((dd) => refreshDropdown(dd.id));
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
      updateLogButtons();
    }
  });

  EventsOn("engine:done", (data) => {
    setEncodingState(false);
    const fill = $("progress_fill");
    fill.classList.remove("active");
    if (data && data.success) {
      setProgressClip(1);
      setStatus("finished");
      flashOutputReady();
    } else if (data && data.cancelled) {
      appendLog("warn", t("abortLogMsg"));
      setStatus("aborted");
      fill.classList.add("failed");
      $("lbl_eta").textContent = "—";
    } else {
      setStatus("failed");
      fill.classList.add("failed");
    }
    updateReadyState();
    updateLogButtons();
  });
}

// ── Wire up OS drag & drop ──
function wireDragAndDrop() {
  OnFileDrop((x, y, paths) => {
    if (!paths?.length || state.isEncoding) return;
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
  const icons = { folder: folderIcon, eraser: eraserIcon, monitor: monitorIcon, sun: sunIcon, moon: moonIcon };
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
  updateScopeHint();
  await loadSettings();
  applyI18n();
  await populateThreads();
  updateControlsFade();
  updateReadyState();
  updateOutputActions();
  syncDropdown("dd_quantizer");
  syncSeg("seg_language");
  syncSeg("seg_theme");
  document.title = t("windowTitle");
  wireControlsFade();
}

init();
