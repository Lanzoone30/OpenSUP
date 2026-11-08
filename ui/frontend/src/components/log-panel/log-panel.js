// log-panel.js — Log rendering, formatting, follow and clipboard.

import { $ } from "../../shared/dom.js";
import { state } from "../../shared/state.js";
import { t, fmt, getLang } from "../../shared/i18n.js";
import { setProgressClip, setStatus } from "../progress/progress.js";

const LEVEL_NAMES = { debug: "DEBUG", info: "INFO", warn: "WARN", pass: "PASS", error: "ERROR", fail: "FAIL", fatal: "FATAL" };

const RESOLUTION_SUFFIX = "Use --ignore-resolution or the checkbox in the GUI.";

export function appendLog(level, msg, extraClass) {
  const area = $("txt_log");
  const entry = document.createElement("span");
  entry.className = "log-entry " + levelNameToCss(level) + (extraClass ? " " + extraClass : "");
  entry.textContent = formatLogLine(level, msg) + "\n";
  area.appendChild(entry);
  if (followLog) area.scrollTop = area.scrollHeight;
  state.logEntries++;
  updateLogCount();
  // Remove empty-state once we have content
  if (area.dataset.empty && area.children.length > 0) {
    area.removeAttribute("data-empty");
  }
}

// Plain-text divider between runs. No level or timestamp so it stays a
// readable marker when the log is pasted elsewhere.
export function appendLogSeparator() {
  if (state.logEntries === 0) return;
  const area = $("txt_log");
  const entry = document.createElement("span");
  entry.className = "log-entry log-sep";
  entry.textContent = "-".repeat(60) + "\n";
  area.appendChild(entry);
  if (followLog) area.scrollTop = area.scrollHeight;
  state.logEntries++;
  updateLogCount();
}

// Formats as "HH:mm:ss │ LEVEL │ text"; the engine prefixes
// "HH:MM:SS LEVEL: ", which is dropped and rebuilt here.
function formatLogLine(level, msg) {
  const css = levelNameToCss(level);
  const name = LEVEL_NAMES[css] || "INFO";
  let text = msg.replace(/^\d{2}:\d{2}:\d{2}\s+[A-Z]+:\s+/, "");
  if (text.includes(RESOLUTION_SUFFIX)) text = text.replaceAll(RESOLUTION_SUFFIX, t("resolutionHint"));
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

function clearLog() {
  const area = $("txt_log");
  // replaceChildren is cheaper and does not disturb the WebView2 DOM tree.
  area.replaceChildren();
  state.logEntries = 0;
  followLog = true;
  const fb = $("btn_log_follow");
  if (fb) fb.hidden = true;
  updateLogCount();
  updateLogPlaceholder();
  // Keep "failed" so a red (aborted) bar stays red while it resets; a new
  // encode clears it in startEncode().
  setProgressClip(0);
  $("progress_fill").classList.remove("active");
  setStatus("standing");
  $("lbl_pct").textContent = "0%";
  $("lbl_eta").textContent = "—";
  updateLogButtons();
}

export function updateLogCount() {
  const el = $("lbl_log_lines");
  if (el) el.textContent = fmt(t(state.logEntries === 1 ? "logLine" : "logLines"), state.logEntries);
  updateLogButtons();
}

function hasProgressState() {
  return state.isEncoding ||
    state.statusMode !== "standing" ||
    $("lbl_pct").textContent.trim() !== "0%" ||
    $("lbl_eta").textContent.trim() !== "—";
}

export function updateLogButtons() {
  const hasLog = state.logEntries > 0;
  const copy = $("btn_copy_log");
  const clear = $("btn_clear_log");
  if (copy) copy.disabled = !hasLog;
  if (clear) clear.disabled = !hasLog && !hasProgressState();
}

export function updateLogPlaceholder() {
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

const LOG_FOLLOW_THRESHOLD = 40;
let followLog = true;

function isLogNearBottom() {
  const area = $("txt_log");
  if (!area) return true;
  return area.scrollHeight - area.scrollTop - area.clientHeight < LOG_FOLLOW_THRESHOLD;
}

function updateLogFollowBtn() {
  const btn = $("btn_log_follow");
  if (!btn) return;
  followLog = isLogNearBottom();
  btn.hidden = followLog;
}

function wireLogFollow() {
  const area = $("txt_log");
  const btn = $("btn_log_follow");
  if (!area || !btn) return;
  area.addEventListener("scroll", () => {
    followLog = isLogNearBottom();
    updateLogFollowBtn();
  });
  btn.addEventListener("click", () => {
    area.scrollTop = area.scrollHeight;
    followLog = true;
    updateLogFollowBtn();
  });
}

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

// Wire the log panel's own buttons and auto-scroll.
export function wireLogPanel() {
  $("btn_copy_log").addEventListener("click", copyLog);
  $("btn_clear_log").addEventListener("click", clearLog);
  wireLogFollow();
}
