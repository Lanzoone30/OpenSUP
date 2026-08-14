// app.js — Main entry point. Vanilla JS, no framework.
// Handles i18n, theming, state, and Wails event wiring.

import './app.css';
import { t, fmt, setLang, getLang } from './i18n.js';
import { EventsOn, OnFileDrop } from '../wailsjs/runtime/runtime.js';

// ── State ──
const state = {
  inputPath: "",
  outputPath: "",
  isEncoding: false,
  logEntries: 0,
  encodingStart: 0,
  lastBDNDir: "",
  lastOutDir: "",
};

// ── DOM shortcuts ──
const $ = (id) => document.getElementById(id);

// ── i18n: translate all [data-i18n] elements ──
function applyI18n() {
  document.querySelectorAll("[data-i18n]").forEach((el) => {
    el.textContent = t(el.dataset.i18n);
  });
  // Tooltips for checkboxes
  const tips = {
    chk_allow_normal: "tipAllowNormal",
    chk_prefer_normal: "tipPreferNormal",
    chk_full_palette: "tipFullPalette",
    chk_both_formats: "tipBothFormats",
    chk_overlap: "tipOverlapBuf",
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
  // Update log count + empty-state text
  updateLogCount();
  updateLogPlaceholder();
}

// ── Theme: 0=System, 1=Light, 2=Dark ──
function applyTheme(theme) {
  const themes = ["system", "light", "dark"];
  document.documentElement.setAttribute("data-theme", themes[theme] || "system");
}

// ── Log area ──
function appendLog(level, msg) {
  const area = $("txt_log");
  const entry = document.createElement("span");
  entry.className = "log-entry " + levelNameToCss(level);
  entry.textContent = msg + "\n";
  area.appendChild(entry);
  area.scrollTop = area.scrollHeight;
  state.logEntries++;
  updateLogCount();
  // Remove empty-state once we have content
  if (area.dataset.empty && area.children.length > 0) {
    area.removeAttribute("data-empty");
  }
}

function levelNameToCss(level) {
  const map = {
    ldebug: "debug", hdebug: "debug",
    info: "info", iinfo: "info", einfo: "info",
    warn: "warn",
    pass: "pass",
    error: "error", fail: "fail", fatal: "fatal",
  };
  return map[level] || "info";
}

// Sync a segmented pill group with its hidden <select> source of truth.
function syncSeg(id) {
  const sel = $(id);
  const seg = document.querySelector(`.seg[data-target="${id}"]`);
  if (!sel || !seg) return;
  const value = sel.value;
  seg.querySelectorAll(".seg-btn").forEach((btn) => {
    const active = btn.dataset.val === value;
    btn.classList.toggle("active", active);
    btn.setAttribute("aria-pressed", String(active));
  });
}

function updateLogCount() {
  const el = $("lbl_log_lines");
  if (el) el.textContent = fmt(t("logLines"), state.logEntries);
}

function updateLogPlaceholder() {
  const area = $("txt_log");
  if (!area) return;
  if (state.logEntries === 0) {
    area.setAttribute("data-empty", getLang() === 1
      ? "Sin actividad aún. Selecciona un archivo y presiona PROCESAR para comenzar."
      : "No activity yet. Select a file and press ENCODE to begin.");
  } else {
    area.removeAttribute("data-empty");
  }
}

function clearLog() {
  const area = $("txt_log");
  area.innerHTML = "";
  state.logEntries = 0;
  updateLogCount();
  updateLogPlaceholder();
}

// ── Progress + ETA ──
function updateProgress(percent, epoch, total) {
  const fill = $("progress_fill");
  fill.style.width = percent + "%";
  fill.classList.remove("failed");

  if (total > 0 && epoch > 0) {
    const elapsed = Date.now() - state.encodingStart;
    if (epoch < total) {
      const remainingMs = elapsed * (total - epoch) / epoch;
      const remainingS = Math.floor(remainingMs / 1000);
      const eta = remainingS >= 60
        ? `${Math.floor(remainingS / 60)}m ${remainingS % 60}s`
        : `${remainingS}s`;
      const prefix = getLang() === 1 ? "Época " : "Epoch ";
      $("lbl_eta").textContent = prefix + epoch + "/" + total + " · ETA " + eta;
    } else {
      const prefix = getLang() === 1 ? "Época " : "Epoch ";
      $("lbl_eta").textContent = prefix + epoch + "/" + total;
    }
  }
}

// ── Button state management ──
function setEncodingState(encoding) {
  state.isEncoding = encoding;
  $("btn_encode").disabled = encoding;
  $("btn_abort").disabled = !encoding;
  $("btn_select_bdn").disabled = encoding;
  $("btn_set_output").disabled = encoding;
}

function updateReadyState() {
  $("btn_encode").disabled = state.isEncoding || !state.inputPath || !state.outputPath;
}

// ── Wails Go bindings (via runtime bridge) ──
const go = () => window.go?.main?.App;

function applyBDNPath(path) {
  if (!path) return;
  state.inputPath = path;
  state.lastBDNDir = path.substring(0, Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\")));
  $("lbl_bdn_file").textContent = path.split(/[\\/]/).pop();
  updateReadyState();
}

async function selectBDN() {
  const app = go();
  if (!app?.SelectBDN) return;
  try {
    const path = await app.SelectBDN();
    applyBDNPath(path);
  } catch (err) {
    console.error("SelectBDN:", err);
  }
}

async function setOutput() {
  const app = go();
  if (!app?.SetOutput) return;
  try {
    const path = await app.SetOutput();
    if (path) {
      state.outputPath = path;
      state.lastOutDir = path.substring(0, Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\")));
      $("lbl_output_file").textContent = path.split(/[\\/]/).pop();
      updateReadyState();
    }
  } catch (err) {
    console.error("SetOutput:", err);
  }
}

function startEncode() {
  const app = go();
  if (!app?.StartEncode) return;
  state.encodingStart = Date.now();

  const cfg = {
    InputPath:       state.inputPath,
    OutputPath:      state.outputPath,
    Quantizer:       parseInt($("combo_quantizer").value),
    BTMatrix:        $("combo_colorspace").value,
    Overwrite:       true,
    IgnoreRes:       $("chk_ignore_res").checked,
    BothFormats:     $("chk_both_formats").checked,
    AllowNormalCase: $("chk_allow_normal").checked,
    FullPalette:     $("chk_full_palette").checked,
    Overlap:         $("chk_overlap").checked,
  };

  setEncodingState(true);
  $("progress_fill").style.width = "0%";
  $("progress_fill").classList.remove("failed");
  $("lbl_progress_text").textContent = t("starting");
  $("lbl_eta").textContent = "—";

  // Remove log empty-state
  const area = $("txt_log");
  if (area) area.removeAttribute("data-empty");

  app.StartEncode(cfg).catch((err) => {
    console.error("StartEncode:", err);
    setEncodingState(false);
    $("lbl_progress_text").textContent = t("failed");
    $("progress_fill").classList.add("failed");
    updateReadyState();
  });
}

function abortEncode() {
  const app = go();
  if (!app?.AbortEncode) return;
  app.AbortEncode();
}

// ── Copy log to clipboard ──
async function copyLog() {
  const text = $("txt_log").innerText;
  if (!text) return;
  try {
    await navigator.clipboard.writeText(text);
    const btn = $("btn_copy_log");
    const original = btn.textContent;
    btn.textContent = t("copied");
    setTimeout(() => { btn.textContent = original; }, 1500);
  } catch (err) {
    console.error("clipboard:", err);
  }
}

// ── Settings persistence (via Go) ──
async function loadSettings() {
  const app = go();
  if (app?.LoadSettings) {
    try {
      const s = await app.LoadSettings();
      if (s) {
        setLang(s.Language || 0);
        $("cmb_language").value = String(s.Language || 0);
        syncSeg("cmb_language");
        applyTheme(s.Theme || 0);
        $("cmb_theme").value = String(s.Theme || 0);
        syncSeg("cmb_theme");
        if (s.LastBDNDir)  state.lastBDNDir = s.LastBDNDir;
        if (s.LastOutDir)  state.lastOutDir = s.LastOutDir;
      }
    } catch (err) {
      console.error("LoadSettings:", err);
    }
  }
  applyI18n();
}

async function saveSettings() {
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
    if (data && data.success) {
      fill.style.width = "100%";
      $("lbl_progress_text").textContent = t("done");
    } else if (data && data.Cancelled) {
      $("lbl_progress_text").textContent = t("abortedShort");
      fill.classList.add("failed");
    } else {
      $("lbl_progress_text").textContent = t("failed");
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
  $("cmb_language").addEventListener("change", () => {
    setLang(parseInt($("cmb_language").value));
    applyI18n();
    saveSettings();
  });

  $("cmb_theme").addEventListener("change", () => {
    applyTheme(parseInt($("cmb_theme").value));
    saveSettings();
  });

  // Segmented controls drive the hidden selects so the rest of the app stays unchanged.
  document.querySelectorAll(".seg .seg-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      const seg = btn.closest(".seg");
      const sel = $(seg.dataset.target);
      if (!sel) return;
      sel.value = btn.dataset.val;
      sel.dispatchEvent(new Event("change"));
      syncSeg(seg.dataset.target);
    });
  });

  $("btn_select_bdn").addEventListener("click", selectBDN);
  $("btn_set_output").addEventListener("click", setOutput);
  $("btn_encode").addEventListener("click", startEncode);
  $("btn_abort").addEventListener("click", abortEncode);
  $("btn_copy_log").addEventListener("click", copyLog);
  $("btn_clear_log").addEventListener("click", clearLog);

  // PES output requires full palette (mirrors Qt logic)
  $("chk_both_formats").addEventListener("change", (e) => {
    $("chk_full_palette").checked = e.target.checked;
    $("chk_full_palette").disabled = e.target.checked;
  });
}

// ── Init ──
async function init() {
  wireDomEvents();
  wireEngineEvents();
  wireDragAndDrop();
  await loadSettings();
  updateReadyState();
  document.title = t("windowTitle");
}

init();