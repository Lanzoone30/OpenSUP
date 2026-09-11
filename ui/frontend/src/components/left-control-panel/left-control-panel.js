// left-control-panel.js — BDN/output picking, output actions and sidebar fade.

import { $ } from "../../shared/dom.js";
import { state } from "../../shared/state.js";
import { go } from "../../shared/bridge.js";
import { updateReadyState } from "../btns-actions/btns-actions.js";

export function applyBDNPath(path) {
  if (!path) return;
  state.inputPath = path;
  state.lastBDNDir = path.substring(0, Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\")));
  $("lbl_bdn_file").textContent = path.split(/[\\/]/).pop();
  updateReadyState();
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
    const path = await app.SetOutput();
    if (path) {
      state.outputPath = path;
      state.lastOutDir = path.substring(0, Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\")));
      $("lbl_output_file").textContent = path.split(/[\\/]/).pop();
      // .pes destinations require full palette.
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
