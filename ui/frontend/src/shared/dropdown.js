// dropdown.js — Custom dropdown molecule (hidden <select> drives the value).

import { $ } from "./dom.js";

export function refreshDropdown(ddId) {
  const dd = $(ddId);
  if (!dd) return;
  const sel = dd.querySelector("select");
  const btn = dd.querySelector(".dd-btn");
  const active = sel && dd.querySelector(`.dd-list li[data-val="${sel.value}"]`);
  if (!active || !btn) return;
  // The button shows the full option text (the quantizer keeps its
  // parenthesized descriptor) so the closed control stays informative.
  const label = btn.querySelector(".dd-label");
  const iconOnly = btn.querySelector(".seg-icon");
  const full = active.textContent.trim();
  if (label) label.textContent = full;
  else if (!iconOnly) btn.textContent = full;
}

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
    li.setAttribute("aria-selected", String(active));
    // Mark options whose text overflows the list width; they scroll on hover.
    const span = li.querySelector("span");
    if (span) {
      span.classList.toggle("overflow", span.scrollWidth > li.clientWidth);
    }
  });
  refreshDropdown(ddId);
  list.classList.remove("open");
  btn.classList.remove("open");
  btn.setAttribute("aria-expanded", "false");
  btn.removeAttribute("aria-activedescendant");
}

export function closeDropdowns() {
  document.querySelectorAll(".dropdown").forEach((dd) => {
    dd.querySelector(".dd-list")?.classList.remove("open");
    const btn = dd.querySelector(".dd-btn");
    btn?.classList.remove("open");
    btn?.setAttribute("aria-expanded", "false");
    btn?.removeAttribute("aria-activedescendant");
    dd.querySelectorAll(".dd-list li.focus").forEach((li) => li.classList.remove("focus"));
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

  // The hidden select is a value fallback only; keep it out of the tab order.
  sel.tabIndex = -1;
  btn.setAttribute("aria-haspopup", "listbox");
  btn.setAttribute("aria-expanded", "false");

  const options = () => [...list.querySelectorAll("li[data-val]")];
  options().forEach((li, i) => {
    if (!li.id) li.id = `${ddId}_opt_${i}`;
  });

  let focusIndex = 0;

  const setFocus = (index) => {
    const items = options();
    if (!items.length) return;
    focusIndex = (index + items.length) % items.length;
    items.forEach((li, i) => li.classList.toggle("focus", i === focusIndex));
    btn.setAttribute("aria-activedescendant", items[focusIndex].id);
    items[focusIndex].scrollIntoView({ block: "nearest" });
  };

  const open = () => {
    closeDropdowns();
    list.classList.add("open");
    btn.classList.add("open");
    btn.setAttribute("aria-expanded", "true");
    const index = options().findIndex((li) => li.dataset.val === sel.value);
    setFocus(index < 0 ? 0 : index);
  };

  const choose = (li) => {
    sel.value = li.dataset.val;
    sel.dispatchEvent(new Event("change"));
    syncDropdown(ddId);
    btn.focus();
  };

  btn.addEventListener("click", (e) => {
    e.stopPropagation();
    if (list.classList.contains("open")) closeDropdowns();
    else open();
  });

  btn.addEventListener("keydown", (e) => {
    const items = options();
    if (!items.length) return;
    const isOpen = list.classList.contains("open");
    switch (e.key) {
      case "ArrowDown":
        e.preventDefault();
        if (isOpen) setFocus(focusIndex + 1);
        else open();
        break;
      case "ArrowUp":
        e.preventDefault();
        if (isOpen) setFocus(focusIndex - 1);
        else open();
        break;
      case "Home":
        if (!isOpen) return;
        e.preventDefault();
        setFocus(0);
        break;
      case "End":
        if (!isOpen) return;
        e.preventDefault();
        setFocus(items.length - 1);
        break;
      case "Enter":
      case " ":
        if (!isOpen) return; // Native button activation opens it.
        e.preventDefault();
        choose(items[focusIndex] || items[0]);
        break;
      case "Escape":
        if (!isOpen) return;
        e.preventDefault();
        closeDropdowns();
        btn.focus();
        break;
    }
  });

  list.addEventListener("click", (e) => {
    const li = e.target.closest("li[data-val]");
    if (!li) return;
    choose(li);
  });
}
