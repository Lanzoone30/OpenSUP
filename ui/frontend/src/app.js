// app.js — Main entry point. Vanilla JS, no framework.
// Handles i18n, theming, state, and Wails event wiring.

import './app.css';
import { t, fmt, setLang, getLang } from './i18n.js';
import { EventsOn, OnFileDrop } from '../wailsjs/runtime/runtime.js';
import folderIcon from './assets/icons/folder.svg?raw';

// ── State ──
const state = {
  inputPath: "",
  outputPath: "",
  isEncoding: false,
  logEntries: 0,
  encodingStart: 0,
  lastBDNDir: "",
  lastOutDir: "",
  statusMode: "standing",
  lastProgressAt: 0,
  ledBurstTimer: 0,
};

// ── DOM shortcuts ──
const $ = (id) => document.getElementById(id);

// ── i18n: translate all [data-i18n] elements ──
function applyI18n() {
  document.querySelectorAll("[data-i18n]").forEach((el) => {
    // Keep the selected file names when switching language (Qt pattern:
    // only translate the placeholder, never overwrite a chosen path).
    if (el.id === "lbl_bdn_file" && state.inputPath) return;
    if (el.id === "lbl_output_file" && state.outputPath) return;
    // The status LED label is dynamic (Working/Finished/etc): keep its
    // current state, it is re-applied at the end of this function.
    if (el.id === "lbl_progress_text") return;
    if (el.classList.contains("help-tip")) {
      // Help tips keep a "?" glyph; the text goes into the tooltip.
      el.dataset.tooltip = t(el.dataset.i18n);
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
  const rd = $("input_redraw");
  if (rd) rd.title = t("redrawPeriodTip");
  const rv = $("btn_reveal_output");
  if (rv) rv.title = t("openOutputFolder");
  rv?.setAttribute("aria-label", t("openOutputFolder"));
  // Update log count + empty-state text
  updateLogCount();
  updateLogPlaceholder();
  // Re-translate the status LED label in the new language.
  if (state.statusMode) setStatus(state.statusMode);
}

// ── Theme: 0=System, 1=Light, 2=Dark ──
function applyTheme(theme) {
  const themes = ["system", "light", "dark"];
  document.documentElement.setAttribute("data-theme", themes[theme] || "system");
}

// ── Log area ──
const QT_LEVEL_NAMES = { debug: "DEBUG", info: "INFO", warn: "WARN", pass: "PASS", error: "ERROR", fail: "FAIL", fatal: "FATAL" };

function appendLog(level, msg) {
  const area = $("txt_log");
  const entry = document.createElement("span");
  entry.className = "log-entry " + levelNameToCss(level);
  entry.textContent = formatQtLogLine(level, msg) + "\n";
  area.appendChild(entry);
  area.scrollTop = area.scrollHeight;
  state.logEntries++;
  updateLogCount();
  // Remove empty-state once we have content
  if (area.dataset.empty && area.children.length > 0) {
    area.removeAttribute("data-empty");
  }
}

// Replicates the Qt GUI line format: "HH:mm:ss │ LEVEL │ text".
// The C++ logger prefixes "HH:MM:SS LEVEL: text", so the prefix is
// stripped and rebuilt with the same separators and colors the old UI used.
function formatQtLogLine(level, msg) {
  const css = levelNameToCss(level);
  const name = QT_LEVEL_NAMES[css] || "INFO";
  const text = msg.replace(/^\d{2}:\d{2}:\d{2}\s+[A-Z]+:\s+/, "");
  const now = new Date();
  const pad = (n) => String(n).padStart(2, "0");
  const ts = `${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
  return `${ts} \u2502 ${name} \u2502 ${text}`;
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

// Custom dropdown: the <select> stays the source of truth; the dropdown
// button shows the full option text (quantizer keeps its parenthesized
// descriptor), and the list shows the same options with hover-marquee.
function syncDropdown(ddId) {
  const dd = $(ddId);
  if (!dd) return;
  const sel = dd.querySelector("select");
  const btn = dd.querySelector(".dd-btn");
  const list = dd.querySelector(".dd-list");
  if (!sel || !btn || !list) return;
  list.querySelectorAll("li").forEach((li) => {
    const active = li.dataset.val === sel.value;
    li.classList.toggle("active", active);
    if (active) {
      // Buttons show the full option text; the quantizer includes the
      // parenthesized descriptor ("libimagequant (best, fast)") so the
      // closed control is as informative as the open list.
      const label = btn.querySelector(".dd-label");
      const iconOnly = btn.querySelector(".seg-icon");
      const full = li.textContent.trim();
      if (label) label.textContent = full;
      else if (!iconOnly) btn.textContent = full;
      // Language button shows the flag of the active locale.
      const flag = btn.querySelector(".dd-flag-current");
      if (flag) {
        const host = li.querySelector("img.dd-flag");
        if (host) flag.src = host.src;
      }
    }
    // Mark options whose text overflows the list width; they scroll on hover.
    const span = li.querySelector("span");
    if (span) {
      span.classList.toggle("overflow", span.scrollWidth > li.clientWidth);
    }
  });
  list.classList.remove("open");
  btn.classList.remove("open");
}

function closeDropdowns() {
  document.querySelectorAll(".dropdown").forEach((dd) => {
    dd.querySelector(".dd-list")?.classList.remove("open");
    dd.querySelector(".dd-btn")?.classList.remove("open");
  });
}

// Wire a custom dropdown; the hidden select drives the value.
function wireDropdown(ddId) {
  const dd = $(ddId);
  if (!dd) return;
  const sel = dd.querySelector("select");
  const btn = dd.querySelector(".dd-btn");
  const list = dd.querySelector(".dd-list");
  if (!sel || !btn || !list) return;

  btn.addEventListener("click", (e) => {
    e.stopPropagation();
    const willOpen = !list.classList.contains("open");
    // Only one dropdown open at a time (theme vs language, sidebar too).
    closeDropdowns();
    if (willOpen) {
      list.classList.add("open");
      btn.classList.add("open");
    }
  });

  list.addEventListener("click", (e) => {
    const li = e.target.closest("li[data-val]");
    if (!li) return;
    sel.value = li.dataset.val;
    sel.dispatchEvent(new Event("change"));
    syncDropdown(ddId);
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

// ── Status LED + label (standing / working / finished / failed / aborted) ──
function setStatus(mode) {
  const led = $("status_led");
  const label = $("lbl_progress_text");
  const map = {
    standing: { cls: "led-standing", key: "standingBy" },
    working:  { cls: "led-working",  key: "working" },
    finished: { cls: "led-finished", key: "finished" },
    failed:   { cls: "led-failed",   key: "failed" },
    aborted:  { cls: "led-failed",   key: "aborted" },
  };
  const s = map[mode] || map.standing;
  state.statusMode = mode;
  if (led) {
    led.className = "led " + s.cls;
    // led-done is only driven by progress events; clear it on any status
    // change so a finished/failed/standing LED is static.
    led.classList.remove("led-done");
    clearTimeout(state.ledBurstTimer);
  }
  if (label) label.textContent = t(s.key);
}

function clearLog() {
  const area = $("txt_log");
  // replaceChildren is cheaper and does not disturb the WebView2 DOM tree.
  area.replaceChildren();
  state.logEntries = 0;
  updateLogCount();
  updateLogPlaceholder();
  // Qt pattern: also reset the progress bar and ETA.
  // Keep the "failed" class so an aborted (red) bar stays red while it
  // animates back; a new encode clears it in startEncode().
  $("progress_fill").style.width = "0%";
  $("progress_fill").classList.remove("active");
  setStatus("standing");
  $("lbl_pct").textContent = "0%";
  $("lbl_eta").textContent = "—";
}

// ── Progress + ETA ──
function updateProgress(percent, epoch, total) {
  const fill = $("progress_fill");
  fill.style.width = percent + "%";
  fill.classList.remove("failed");
  fill.classList.toggle("active", percent > 0 && percent < 100);
  $("lbl_pct").textContent = percent + "%";

  // Epoch completed: green HDD-like burst on the blue working LED.
  // duration = clamp(dt*0.6, 0.12s, 0.8s) where dt is the gap to the
  // previous progress event; the burst restarts per event and the LED
  // returns to the blue base once it ends.
  const led = $("status_led");
  if (led && state.isEncoding) {
    const now = Date.now();
    const dt = state.lastProgressAt ? now - state.lastProgressAt : 0;
    state.lastProgressAt = now;
    const dur = dt > 0 ? Math.max(0.12, Math.min(0.8, dt * 0.6)) : 0.3;
    led.style.setProperty("--beat", dur + "s");
    led.classList.remove("led-done");
    void led.offsetWidth;
    led.classList.add("led-done");
    // Burst runs twice (led-blink x2); drop back to blue when it finishes.
    clearTimeout(state.ledBurstTimer);
    state.ledBurstTimer = setTimeout(() => {
      led.classList.remove("led-done");
    }, dur * 1000 + 50);
  }

  if (total > 0 && epoch > 0) {
    const elapsed = Date.now() - state.encodingStart;
    if (epoch < total) {
      const remainingMs = elapsed * (total - epoch) / epoch;
      const remainingS = Math.floor(remainingMs / 1000);
      const eta = remainingS >= 60
        ? `${Math.floor(remainingS / 60)}m ${remainingS % 60}s`
        : `${remainingS}s`;
      // "Epoch" stays in English in both UI languages (SUPer parity).
      $("lbl_eta").textContent = "Epoch " + epoch + "/" + total + " · ETA " + eta;
    } else {
      $("lbl_eta").textContent = "Epoch " + epoch + "/" + total;
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
  updateOutputActions();
}

// Attribute `state.isEncoding || !state.inputPath || !state.outputPath` gates the encode button.
function updateReadyState() {
  $("btn_encode").disabled =
    state.isEncoding || !state.inputPath || !state.outputPath;
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
      // .pes destinations require full palette (SUPer set_outputsup parity).
      if (path.toLowerCase().endsWith(".pes")) {
        $("chk_full_palette").checked = true;
        $("chk_full_palette").disabled = true;
      } else if (!$("chk_both_formats").checked) {
        $("chk_full_palette").disabled = false;
      }
      updateReadyState();
      updateOutputActions();
    }
  } catch (err) {
    console.error("SetOutput:", err);
  }
}

// Reveal the output file in the OS file manager (Explorer on Windows).
async function revealOutputFolder() {
  const app = go();
  if (!app?.RevealOutput || !state.outputPath) return;
  try {
    await app.RevealOutput(state.outputPath);
  } catch (err) {
    console.error("RevealOutput:", err);
  }
}

// Enable/disable the reveal-folder icon based on whether an output exists.
function updateOutputActions() {
  const btn = $("btn_reveal_output");
  if (btn) btn.disabled = !state.outputPath || state.isEncoding;
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
    PreferNormalCase: $("chk_prefer_normal").checked,
    FullPalette:     $("chk_full_palette").checked,
    Overlap:         $("chk_overlap").checked,
    RedrawPeriod:    parseFloat($("input_redraw").value) || 0,
    MaxKbps:         parseInt($("input_max_kbps").value) || 0,
  };

  setEncodingState(true);
  $("progress_fill").style.width = "0%";
  $("progress_fill").classList.remove("failed", "active");
  setStatus("working");
  $("lbl_pct").textContent = "0%";
  $("lbl_eta").textContent = "—";

  // Remove log empty-state
  const area = $("txt_log");
  if (area) area.removeAttribute("data-empty");

  app.StartEncode(cfg).then(() => {
    // Normal completion: the engine:done event handles the UI state.
  }).catch((err) => {
    console.error("StartEncode:", err);
    setStatus("failed");
    $("progress_fill").classList.add("failed");
  }).finally(() => {
    // Always re-enable buttons, even on abort/cancel where engine:done
    // may never fire (prevents stuck-disabled Select BDN / Set Output).
    setEncodingState(false);
    updateReadyState();
  });
}

function abortEncode() {
  const app = go();
  if (!app?.AbortEncode) return;
  app.AbortEncode();
  // Qt pattern: immediate UI feedback (bar turns red + log message).
  // "error" (not the numeric level) is used so levelNameToCss maps to the
  // red .log-entry.error style; the log stays English regardless of UI language.
  appendLog("error", "Encoding aborted by user.");
  $("progress_fill").classList.add("failed");
  $("progress_fill").classList.remove("active");
  setStatus("aborted");
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
    fill.classList.remove("active");
    if (data && data.success) {
      fill.style.width = "100%";
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

  $("btn_select_bdn").addEventListener("click", selectBDN);
  $("btn_set_output").addEventListener("click", setOutput);
  $("btn_reveal_output").addEventListener("click", revealOutputFolder);
  $("btn_encode").addEventListener("click", startEncode);
  $("btn_abort").addEventListener("click", abortEncode);
  $("btn_copy_log").addEventListener("click", copyLog);
  $("btn_clear_log").addEventListener("click", clearLog);

  // Both-formats output requires full palette; unchecking only re-enables
// Full Palette without touching its own value (SUPer hide_chkbox parity).
  $("chk_both_formats").addEventListener("change", (e) => {
    if (e.target.checked) {
      $("chk_full_palette").checked = true;
      $("chk_full_palette").disabled = true;
    } else if (!state.outputPath.toLowerCase().endsWith(".pes")) {
      $("chk_full_palette").disabled = false;
    }
    updateReadyState();
  });

  // Prefer normal case forces Allow Normal Case ON and disables it; unchecking
  // only re-enables Allow preserving its own value (SUPer hide_chkbox parity).
  $("chk_prefer_normal").addEventListener("change", (e) => {
    if (e.target.checked) {
      $("chk_allow_normal").checked = true;
      $("chk_allow_normal").disabled = true;
    } else {
      $("chk_allow_normal").disabled = false;
    }
    updateReadyState();
  });

  // Re-evaluate encode readiness if options change (button gate: paths only).
  ["chk_allow_normal", "chk_prefer_normal", "chk_full_palette",
   "chk_both_formats", "chk_overlap", "chk_ignore_res"]
    .forEach((id) => {
      $(id).addEventListener("change", updateReadyState);
    });
}

// Sidebar edge fade: mask the scroll ends only while content is cut off
// there (idle lists stay crisp; scrolling looks soft instead of abrupt).
function updateControlsFade() {
  const sc = document.querySelector(".controls-scroll");
  if (!sc) return;
  const maxSc = sc.scrollHeight - sc.clientHeight;
  sc.style.setProperty("--fade-t", sc.scrollTop > 0 ? "14px" : "0px");
  sc.style.setProperty("--fade-b", sc.scrollTop < maxSc ? "14px" : "0px");
}

// ── Init ──

// Inyect SVG icons (loaded ?raw from src/assets/icons/) into their hosts.
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
  updateReadyState();
  updateOutputActions();
  syncDropdown("dd_quantizer");
  syncDropdown("dd_language");
  syncDropdown("dd_theme");
  document.title = t("windowTitle");
  const sc = document.querySelector(".controls-scroll");
  if (sc) {
    sc.addEventListener("scroll", updateControlsFade, { passive: true });
    window.addEventListener("resize", updateControlsFade);
    updateControlsFade();
  }
}

init();