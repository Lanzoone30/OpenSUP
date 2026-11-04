// btns-actions.js — Encode/abort button state, readiness gate and start/abort.

import { $ } from "../../shared/dom.js";
import { state } from "../../shared/state.js";
import { numOr } from "../../shared/dom.js";
import { go } from "../../shared/bridge.js";
import { setProgressClip, setStatus } from "../progress/progress.js";
import { appendLog, updateLogButtons } from "../log-panel/log-panel.js";
import { selectBDN, setOutput, revealOutputFolder, updateOutputActions, clearScope } from "../left-control-panel/left-control-panel.js";
import { saveSettings } from "../../shared/settings.js";

export function setEncodingState(encoding) {
  state.isEncoding = encoding;
  $("btn_encode").disabled = encoding;
  $("btn_abort").disabled = !encoding;
  $("btn_select_bdn").disabled = encoding;
  $("btn_set_output").disabled = encoding;
  const sc = document.querySelector(".controls-scroll");
  if (sc) {
    // Lock the cards, not the scroll container, so the sidebar still scrolls.
    sc.classList.toggle("is-encoding", encoding);
    for (const card of sc.children) card.inert = encoding;
  }
  updateOutputActions();
  updateLogButtons();
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
    BothFormats:     $("chk_both_formats").checked,
    IgnoreResolution:  $("chk_ignore_resolution").checked,
    AllowNormalCase: $("chk_allow_normal").checked,
    PreferNormalCase: $("chk_prefer_normal").checked,
    FullPalette:     $("chk_full_palette").checked,
    Overlap:         $("chk_overlap").checked,
    AlternateOids:   $("chk_alternate_oids").checked,
    TightOds:        $("chk_tight_ods").checked,
    SingleObjectDs:  $("chk_single_object_ds").checked,
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

  // Persist the checkbox/quantizer state on each run: there is no settings dialog.
  saveSettings();
  app.StartEncode(cfg).then(() => {
    // Normal completion: the engine:done event handles the UI state.
  }).catch((err) => {
    console.error("StartEncode:", err);
    appendLog("error", "Failed to start encode: " + err);
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
  if (!app?.AbortEncode || !state.isEncoding) return;
  $("btn_abort").disabled = true;
  app.AbortEncode();
  $("progress_fill").classList.add("failed");
  $("progress_fill").classList.remove("active");
  $("lbl_eta").textContent = "—";
  setStatus("aborted");
}

// Wire the action buttons.
export function wireButtons() {
  $("btn_select_bdn").addEventListener("click", selectBDN);
  $("btn_set_output").addEventListener("click", setOutput);
  $("btn_reveal_output").addEventListener("click", revealOutputFolder);
  $("btn_clear_scope").addEventListener("click", clearScope);
  $("btn_encode").addEventListener("click", startEncode);
  $("btn_abort").addEventListener("click", abortEncode);
}

let allowBeforePrefer = false;

// freePaletteChecked remembers the user's choice while no rule forces the
// palette (both formats, .pes output, single-object), so unforcing restores
// it instead of leaving the box half-changed.
let freePaletteChecked = false;

export function syncFullPalette() {
  const forced =
    $("chk_both_formats").checked ||
    (state.outputPath || "").toLowerCase().endsWith(".pes") ||
    $("chk_single_object_ds").checked;
  const wasFullPaletteForced = $("chk_full_palette").disabled;
  if (!wasFullPaletteForced) freePaletteChecked = $("chk_full_palette").checked;
  $("chk_full_palette").disabled = forced;
  $("chk_full_palette").checked = forced || freePaletteChecked;
  updateReadyState();
}

// Wire the option checkboxes that gate encode readiness.
export function wireOptions() {
  $("chk_prefer_normal").addEventListener("change", (e) => {
    if (e.target.checked) {
      allowBeforePrefer = $("chk_allow_normal").checked;
      $("chk_allow_normal").checked = true;
      $("chk_allow_normal").disabled = true;
    } else {
      $("chk_allow_normal").disabled = false;
      $("chk_allow_normal").checked = allowBeforePrefer;
    }
    updateReadyState();
  });

  // Both-formats and .pes destinations require full palette; the re-enable is
  // re-evaluated on both triggers so no disabled state survives.
  $("chk_both_formats").addEventListener("change", syncFullPalette);

  // Single-object display sets are incompatible with -d/--ahead and inert
  // with normal-case refreshes; full palette and tight ODS boxes are forced
  // engine-side. Pre-state is captured so toggling off restores the user's.
  let singleOnRestore = null;

  $("chk_single_object_ds").addEventListener("change", (e) => {
    const on = e.target.checked;
    if (on) {
      singleOnRestore = {
        overlap: $("chk_overlap").checked,
        allowNormal: $("chk_allow_normal").checked,
        preferNormal: $("chk_prefer_normal").checked,
        tightOds: $("chk_tight_ods").checked,
      };
      $("chk_overlap").checked = false;
      $("chk_allow_normal").checked = false;
      $("chk_prefer_normal").checked = false;
      $("chk_tight_ods").checked = true;
    } else if (singleOnRestore) {
      $("chk_prefer_normal").checked = singleOnRestore.preferNormal;
      $("chk_allow_normal").checked = singleOnRestore.preferNormal || singleOnRestore.allowNormal;
      $("chk_overlap").checked = singleOnRestore.overlap;
      allowBeforePrefer = singleOnRestore.preferNormal;
      $("chk_tight_ods").checked = singleOnRestore.tightOds;
      singleOnRestore = null;
    }
    $("chk_overlap").disabled = on;
    $("chk_prefer_normal").disabled = on;
    $("chk_allow_normal").disabled = on || $("chk_prefer_normal").checked;
    $("chk_tight_ods").disabled = on;
    if ($("chk_prefer_normal").checked) $("chk_allow_normal").checked = true;
    syncFullPalette();
  });

  // Re-evaluate encode readiness if options change (button gate: paths only).
  ["chk_allow_normal", "chk_full_palette",
   "chk_both_formats", "chk_overlap", "chk_alternate_oids"]
    .forEach((id) => {
      $(id).addEventListener("change", updateReadyState);
    });
}
