// segmented.js — segmented control synced with its hidden <select>.

import { $ } from "./dom.js";

export function syncSeg(segId) {
  const seg = $(segId);
  if (!seg) return;
  const sel = seg.querySelector("select");
  if (!sel) return;
  const opts = [...seg.querySelectorAll(".seg-opt")];
  let index = opts.findIndex((o) => o.dataset.val === sel.value);
  if (index < 0) index = 0;
  seg.style.setProperty("--i", index);
  opts.forEach((o, i) => {
    const active = i === index;
    o.classList.toggle("active", active);
    o.setAttribute("aria-pressed", String(active));
  });
}
