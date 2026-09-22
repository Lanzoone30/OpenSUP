// dropdown.js — Custom dropdown molecule (hidden <select> drives the value).

import { $ } from "./dom.js";

export function syncDropdown(ddId) {
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
      // The button shows the full option text (the quantizer keeps its
      // parenthesized descriptor) so the closed control stays informative.
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

export function closeDropdowns() {
  document.querySelectorAll(".dropdown").forEach((dd) => {
    dd.querySelector(".dd-list")?.classList.remove("open");
    dd.querySelector(".dd-btn")?.classList.remove("open");
  });
}

// Wire a custom dropdown; the hidden select drives the value.
export function wireDropdown(ddId) {
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
