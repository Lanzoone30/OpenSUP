// progress.js — Status LED/label and progress bar + ETA.

import { $ } from "../../shared/dom.js";
import { state } from "../../shared/state.js";
import { t } from "../../shared/i18n.js";

export function setStatus(mode) {
  const led = $("status_led");
  const label = $("lbl_progress_text");
  const map = {
    standing: { cls: "led-standing", key: "standingBy" },
    working:  { cls: "led-working",  key: "working" },
    finished: { cls: "led-finished", key: "finished" },
    failed:   { cls: "led-failed",   key: "failed" },
    aborted:  { cls: "led-aborted",  key: "aborted" },
  };
  const s = map[mode] || map.standing;
  state.statusMode = mode;
  if (led) {
    const pop = mode === "finished" && !led.classList.contains("led-finished") ? " led-pop" : "";
    led.className = "led " + s.cls + pop;
    clearTimeout(state.ledBurstTimer);
  }
  if (mode === "working") state.lastProgressAt = 0;
  if (label) label.textContent = t(s.key);
}

export function setProgressClip(frac) {
  const f = Math.max(0, Math.min(1, frac));
  const reveal = $("progress_reveal");
  if (reveal) {
    reveal.style.clipPath = "inset(0 " + Math.round((1 - f) * 1000) / 10 + "% 0 0)";
  }
  const track = document.querySelector(".progress-track");
  if (track) track.setAttribute("aria-valuenow", String(Math.round(f * 100)));
}

export function updateProgress(percent, epoch, total) {
  const fill = $("progress_fill");
  setProgressClip(percent / 100);
  fill.classList.remove("failed");
  fill.classList.toggle("active", percent > 0 && percent < 100);
  $("lbl_pct").textContent = percent + "%";

  // Glow tick per epoch; min 0.4 s (max 2.5 ticks/s) and never restart while
  // one is still running, so fast epochs cannot flicker.
  const led = $("status_led");
  if (led && state.isEncoding && !led.classList.contains("led-tick")) {
    const now = Date.now();
    const dt = state.lastProgressAt ? now - state.lastProgressAt : 0;
    state.lastProgressAt = now;
    const dur = Math.max(0.4, Math.min(0.8, dt * 0.6));
    led.style.setProperty("--beat", dur + "s");
    led.classList.add("led-tick");
    clearTimeout(state.ledBurstTimer);
    state.ledBurstTimer = setTimeout(() => {
      led.classList.remove("led-tick");
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
      // "Epoch" stays in English in both UI languages (parity with the original).
      $("lbl_eta").textContent = "Epoch " + epoch + "/" + total + " · ETA " + eta;
    } else {
      $("lbl_eta").textContent = "Epoch " + epoch + "/" + total;
    }
  }
}
