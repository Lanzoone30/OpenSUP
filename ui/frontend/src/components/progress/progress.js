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

export function setProgressClip(frac) {
  const reveal = $("progress_reveal");
  if (reveal) {
    const f = Math.max(0, Math.min(1, frac));
    reveal.style.clipPath = "inset(0 " + Math.round((1 - f) * 1000) / 10 + "% 0 0)";
  }
}

export function updateProgress(percent, epoch, total) {
  const fill = $("progress_fill");
  setProgressClip(percent / 100);
  fill.classList.remove("failed");
  fill.classList.toggle("active", percent > 0 && percent < 100);
  $("lbl_pct").textContent = percent + "%";

  // Green burst when an epoch completes: duration = clamp(dt*0.6, 0.12s,
  // 0.8s), proportional to the gap between progress events.
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
      // "Epoch" stays in English in both UI languages (parity with the original).
      $("lbl_eta").textContent = "Epoch " + epoch + "/" + total + " · ETA " + eta;
    } else {
      $("lbl_eta").textContent = "Epoch " + epoch + "/" + total;
    }
  }
}
