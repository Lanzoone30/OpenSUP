// btns-actions.js — Encode/abort button state, readiness gate and start/abort.

import { $ } from "../../shared/dom.js";
import { state } from "../../shared/state.js";
import { numOr } from "../../shared/dom.js";
import { go } from "../../shared/bridge.js";
import { setProgressClip, setStatus } from "../progress/progress.js";
import { appendLog } from "../log-panel/log-panel.js";
import { selectBDN, setOutput, revealOutputFolder, updateOutputActions } from "../left-control-panel/left-control-panel.js";

export function setEncodingState(encoding) {
  state.isEncoding = encoding;
  $("btn_encode").disabled = encoding;
  $("btn_abort").disabled = !encoding;
  $("btn_select_bdn").disabled = encoding;
  $("btn_set_output").disabled = encoding;
  updateOutputActions();
}

// Attribute `state.isEncoding || !state.inputPath || !state.outputPath` gates the encode button.
export function updateReadyState() {
  $("btn_encode").disabled =
    state.isEncoding || !state.inputPath || !state.outputPath;
}

export function startEncode() {
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
    AlternateOids:   $("chk_alternate_oids").checked,
    RedrawPeriod:    numOr($("input_redraw"), 0),
    MaxKbps:         numOr($("input_max_kbps"), 0),
    Threads:         numOr($("combo_threads"), 0),
    Compression:     numOr($("input_compression"), 80),
    Acqrate:         numOr($("input_acqrate"), 100),
    SsimTol:         numOr($("input_ssim_tol"), 0),
    ExtraAcq:        numOr($("input_extra_acq"), 2),
  };

  setEncodingState(true);
  setProgressClip(0);
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

export function abortEncode() {
  const app = go();
  if (!app?.AbortEncode) return;
  app.AbortEncode();
  // Immediate feedback (red bar + message). "error" (string) so
  // levelNameToCss maps to the red log style.
  appendLog("error", "Encoding aborted by user.");
  $("progress_fill").classList.add("failed");
  $("progress_fill").classList.remove("active");
  setStatus("aborted");
}

// Wire the action buttons.
export function wireButtons() {
  $("btn_select_bdn").addEventListener("click", selectBDN);
  $("btn_set_output").addEventListener("click", setOutput);
  $("btn_reveal_output").addEventListener("click", revealOutputFolder);
  $("btn_encode").addEventListener("click", startEncode);
  $("btn_abort").addEventListener("click", abortEncode);
}

// Wire the option checkboxes that gate encode readiness.
export function wireOptions() {
  // Both-formats output requires full palette; unchecking only re-enables
  // Full Palette without touching its own value.
  $("chk_both_formats").addEventListener("change", (e) => {
    if (e.target.checked) {
      $("chk_full_palette").checked = true;
      $("chk_full_palette").disabled = true;
    } else if (!state.outputPath.toLowerCase().endsWith(".pes")) {
      $("chk_full_palette").disabled = false;
    }
    updateReadyState();
  });

  // Re-evaluate encode readiness if options change (button gate: paths only).
  ["chk_allow_normal", "chk_full_palette",
   "chk_both_formats", "chk_overlap", "chk_alternate_oids", "chk_ignore_res"]
    .forEach((id) => {
      $(id).addEventListener("change", updateReadyState);
    });
}
