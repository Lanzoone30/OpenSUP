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
  clearOutputFlash();
  try {
    await app.RevealOutput(state.outputPath);
  } catch (err) {
    console.error("RevealOutput:", err);
  }
}

// Mark the reveal-folder button as ready; the CSS blinks its frame for 3 s.
export function flashOutputReady() {
  const btn = $("btn_reveal_output");
  if (!btn || btn.disabled) return;
  clearOutputFlash();
  btn.classList.add("is-ready");
  const msg = t("outputReady");
  btn.title = msg;
  btn.setAttribute("aria-label", msg);
}

export function clearOutputFlash() {
  const btn = $("btn_reveal_output");
  if (!btn) return;
  btn.classList.remove("is-ready");
  btn.title = t("openOutputFolder");
  btn.setAttribute("aria-label", t("openOutputFolder"));
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
  clearOutputFlash();
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

const ACC = { duration: 180, easing: "cubic-bezier(0.2, 0, 0, 1)" }; // = var(--ease)
const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
const accAnim = new WeakMap(); // <details> -> running Animation
const accClosing = new WeakSet(); // <details> held open while collapsing

// An opening ancestor locked to a stale height would clip the growing child,
// so drop its height lock and let it settle at its natural height.
function accUnlockAncestors(d) {
  for (let p = d.parentElement; p; p = p.parentElement) {
    if (p.tagName !== "DETAILS" || !p.open || accClosing.has(p)) continue;
    const anim = accAnim.get(p);
    if (!anim) continue;
    anim.cancel();
    accAnim.delete(p);
    p.classList.remove("acc-anim");
  }
}

function accAnimate(d, from, to, fill, done) {
  d.classList.add("acc-anim");
  const anim = d.animate({ height: [`${from}px`, `${to}px`] }, { ...ACC, fill });
  accAnim.set(d, anim);
  anim.onfinish = () => {
    if (accAnim.get(d) !== anim) return;
    accAnim.delete(d);
    done();
    updateControlsFade();
  };
  return anim;
}

function accOpen(d) {
  accUnlockAncestors(d);
  const from = d.getBoundingClientRect().height;
  d.open = true;
  const to = d.getBoundingClientRect().height;
  accAnimate(d, from, to, "none", () => d.classList.remove("acc-anim"));
}

function accCollapse(d) {
  const from = d.getBoundingClientRect().height;
  accAnim.get(d)?.cancel();
  accAnim.delete(d);
  d.classList.add("is-measuring");
  const to = d.getBoundingClientRect().height;
  d.classList.remove("is-measuring");
  accClosing.add(d);
  d.classList.add("acc-closing");
  const anim = accAnimate(d, from, to, "forwards", () => {
    accClosing.delete(d);
    d.open = false;
    anim.cancel();
    d.classList.remove("acc-closing");
    d.classList.remove("acc-anim");
  });
}

// Reopen during a collapse: the <details> is still open, so its natural height
// is the target and no intermediate closed frame is ever painted.
function accExpand(d) {
  const from = d.getBoundingClientRect().height;
  accAnim.get(d)?.cancel();
  accClosing.delete(d);
  d.classList.remove("acc-closing");
  const to = d.getBoundingClientRect().height;
  accAnimate(d, from, to, "none", () => d.classList.remove("acc-anim"));
}

function wireAccordion(d) {
  const summary = d.querySelector(":scope > summary");
  if (!summary) return;
  summary.addEventListener("click", (e) => {
    if (reduceMotion.matches) return; // native, instant
    e.preventDefault();
    if (accClosing.has(d)) return accExpand(d);
    if (d.open) accCollapse(d);
    else accOpen(d);
  });
}

// Keep the sidebar edge fade in sync when a collapsible card opens/closes.
export function wireCardToggles() {
  const canAnimate =
    typeof Element.prototype.animate === "function" && CSS.supports("overflow", "clip");
  ["dd_params", "dd_engine", "dd_advanced"].forEach((id) => {
    const d = $(id);
    if (!d) return;
    d.addEventListener("toggle", () => updateControlsFade());
    if (canAnimate) wireAccordion(d);
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
