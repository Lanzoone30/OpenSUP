// left-control-panel.js — BDN/output picking, output actions and sidebar fade.

import { $ } from "../../shared/dom.js";
import { state } from "../../shared/state.js";
import { go } from "../../shared/bridge.js";
import { t } from "../../shared/i18n.js";
import { updateReadyState, syncFullPalette } from "../btns-actions/btns-actions.js";

export function applyBDNPath(path) {
  if (!path) return;
  state.inputPath = path;
  state.lastBDNDir = path.substring(0, Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\")));
  $("lbl_bdn_file").textContent = path.split(/[\\/]/).pop();
  $("lbl_bdn_file").title = path;
  updateReadyState();
  updateScopeHint();
}

export async function selectBDN() {
  const app = go();
  if (!app?.SelectBDN) return;
  try {
    const path = await app.SelectBDN();
    applyBDNPath(path);
  } catch (err) {
    console.error("SelectBDN:", err);
  }
}

export async function setOutput() {
  const app = go();
  if (!app?.SetOutput) return;
  try {
    const path = await app.SetOutput(state.lastBDNDir || "");
    if (path) {
      state.outputPath = path;
      state.lastOutDir = path.substring(0, Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\")));
      $("lbl_output_file").textContent = path.split(/[\\/]/).pop();
      $("lbl_output_file").title = path;
      syncFullPalette();
      updateOutputActions();
    }
  } catch (err) {
    console.error("SetOutput:", err);
  }
}

// Reveal the output file in the OS file manager (Explorer on Windows).
export async function revealOutputFolder() {
  const app = go();
  if (!app?.RevealOutput || !state.outputPath) return;
  try {
    await app.RevealOutput(state.outputPath);
  } catch (err) {
    console.error("RevealOutput:", err);
  }
}

// Enable/disable the reveal-folder icon based on whether an output exists.
export function updateOutputActions() {
  const btn = $("btn_reveal_output");
  if (btn) btn.disabled = !state.outputPath || state.isEncoding;
  updateScopeHint();
}

export function updateScopeHint() {
  const hint = $("encode_hint");
  if (hint) {
    const hasBdn = Boolean(state.inputPath);
    const hasOut = Boolean(state.outputPath);
    hint.hidden = hasBdn === hasOut;
    $("hint_bdn").classList.toggle("done", hasBdn);
    $("hint_output").classList.toggle("done", hasOut);
  }
  const clear = $("btn_clear_scope");
  if (clear) clear.disabled = (!state.inputPath && !state.outputPath) || state.isEncoding;
  const dz = $("drop_bdn");
  if (dz) dz.title = state.inputPath ? "" : t("dropBdn");
}

export function clearScope() {
  state.inputPath = "";
  state.outputPath = "";
  $("lbl_bdn_file").textContent = t("noFile");
  $("lbl_output_file").textContent = t("destNotSet");
  $("lbl_bdn_file").title = "";
  $("lbl_output_file").title = "";
  updateReadyState();
  updateOutputActions();
}

export function updateControlsFade() {
  const sc = document.querySelector(".controls-scroll");
  if (!sc) return;
  const maxSc = sc.scrollHeight - sc.clientHeight;
  sc.style.setProperty("--fade-t", sc.scrollTop > 0 ? "14px" : "0px");
  sc.style.setProperty("--fade-b", sc.scrollTop < maxSc ? "14px" : "0px");
}

// Wire the sidebar scroll fade + collapsible-card toggles.
export function wireControlsFade() {
  const sc = document.querySelector(".controls-scroll");
  if (!sc) return;
  sc.addEventListener("scroll", updateControlsFade, { passive: true });
  window.addEventListener("resize", updateControlsFade);
  updateControlsFade();
}

// Keep the sidebar edge fade in sync when a collapsible card opens/closes.
export function wireCardToggles() {
  ["dd_params", "dd_engine", "dd_advanced"].forEach((id) => {
    $(id)?.addEventListener("toggle", () => updateControlsFade());
  });
}

// Auto (value 0) lets the engine pick the physical-core count; the rest are
// explicit worker counts.
export async function populateThreads() {
  const sel = $("combo_threads");
  if (!sel) return;
  let cores = 0;
  const app = go();
  if (app?.PhysicalCores) {
    try {
      cores = await app.PhysicalCores();
    } catch (err) {
      console.error("PhysicalCores:", err);
    }
  }
  const max = cores > 0 ? cores : 1;
  const current = sel.value;
  const auto = document.createElement("option");
  auto.value = "0";
  auto.textContent = t("auto");
  auto.dataset.i18n = "auto";
  const options = [auto];
  for (let n = 1; n <= max; n++) {
    const opt = document.createElement("option");
    opt.value = String(n);
    opt.textContent = String(n);
    options.push(opt);
  }
  sel.replaceChildren(...options);
  // Keep the previous choice when still valid, otherwise fall back to auto.
  const values = options.map((o) => o.value);
  sel.value = values.includes(current) ? current : "0";
}
