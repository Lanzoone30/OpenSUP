import { $ } from '../../shared/dom.js';
import { t } from '../../shared/i18n.js';
import { BrowserOpenURL } from '../../../wailsjs/runtime/runtime.js';

export function wireAbout() {
  const dialog = $('about_dialog');
  const trigger = $('btn_about');
  const closeBtn = $('btn_about_close');
  const versionLink = $('about_version');

  if (!dialog || !trigger) return;

  if (versionLink) {
    const version = t('version');
    versionLink.href = `https://github.com/Lanzoone30/OpenSUP/releases/tag/${version}`;
    versionLink.setAttribute('aria-label', `Release notes for ${version}`);
  }

  let isClosing = false;
  let fallbackTimer = null;

  const finishClose = () => {
    dialog.removeEventListener('animationend', finishClose);
    if (fallbackTimer) {
      clearTimeout(fallbackTimer);
      fallbackTimer = null;
    }
    dialog.classList.remove('closing');
    isClosing = false;
    if (dialog.open) {
      dialog.close();
    }
  };

  const closeAbout = () => {
    if (isClosing || !dialog.open) return;

    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
      finishClose();
      return;
    }

    isClosing = true;
    dialog.classList.add('closing');
    dialog.addEventListener('animationend', finishClose, { once: true });
    fallbackTimer = setTimeout(finishClose, 200);
  };

  trigger.addEventListener('click', () => {
    dialog.showModal();
    closeBtn?.focus();
  });

  closeBtn?.addEventListener('click', () => {
    closeAbout();
  });

  dialog.addEventListener('click', (e) => {
    if (e.target === dialog) closeAbout();
  });

  dialog.addEventListener('cancel', (e) => {
    e.preventDefault();
    closeAbout();
  });

  dialog.addEventListener('close', () => {
    if (fallbackTimer) {
      clearTimeout(fallbackTimer);
      fallbackTimer = null;
    }
    dialog.classList.remove('closing');
    isClosing = false;
    trigger.focus();
  });

  dialog.querySelectorAll('a[data-external]').forEach((link) => {
    link.addEventListener('click', (e) => {
      e.preventDefault();
      const url = link.getAttribute('href');
      if (url) BrowserOpenURL(url);
    });
  });
}
