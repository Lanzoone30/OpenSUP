#!/usr/bin/env python3
"""
gui.py

OpenSUP graphical user interface built with CustomTkinter.

Provides the main application window for PGS subtitle encoding:
file/directory selection, palette preview, quantizer configuration,
checkbox mode toggles, encoding progress display, and abort control.

Why this design: GUI offers a visual workflow for users who prefer
not to use the command line. CustomTkinter was chosen for its modern
look, cross-platform consistency, and dark/light theme support.
"""

# Copyright (C) 2023-2026 cibo
# This file is part of OpenSUP, based on SUPer <https://github.com/cubicibo/SUPer>.
#
# OpenSUP is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

import logging
import os
import re
import signal
import subprocess
import sys
import threading
import time
from datetime import timedelta
from pathlib import Path

import customtkinter as ctk

from opensup.__metadata__ import __version__

ctk.set_appearance_mode("light")
ctk.set_default_color_theme("green")

# Colour scheme: white background, black text
WHITE = "#FFFFFF"
BLACK = "#000000"

# Separator line: use ASCII on Windows (code page issues), UTF-8 elsewhere
if os.name == 'nt':
    SEPARATOR = "=" * 50
else:
    SEPARATOR = "\u2550" * 50  # "═"
GRAY_LIGHT = "#F0F0F0"
GRAY_MED = "#CCCCCC"
GRAY_DARK = "#666666"
BLUE_ACCENT = "#2563eb"
GREEN_BTN = "#166534"
RED_BTN = "#7f1d1d"


# Log level colours for the textbox
LOG_COLORS = {
    logging.DEBUG:    "#888888",  # gray
    logging.INFO:     "#000000",  # black
    logging.WARNING:  "#B8860B",  # dark goldenrod
    logging.ERROR:    "#CC0000",  # red
    logging.CRITICAL: "#AA0000",  # dark red
}
PASS_LEVEL = 35
FAIL_LEVEL = 45
LOG_COLORS[PASS_LEVEL] = "#166534"  # green for PASS
LOG_COLORS[FAIL_LEVEL] = "#CC0000"  # red for FAIL

# Custom tag colours for the structured log view
COLOR_SECTION = "#2563eb"  # blue for section headers
COLOR_FIELD   = "#555555"  # dark gray for field labels (Input:, Params:)
COLOR_SUCCESS = "#166534"  # green for SUCCESS status


def _icon_path() -> Path | None:
    """Resolve path to icon.ico in frozen and dev modes."""
    if getattr(sys, 'frozen', False):
        exe_dir = Path(sys.argv[0]).resolve().parent
        ico = exe_dir / 'assets' / 'OpenSup.ico'
        if ico.exists():
            return ico
        if hasattr(sys, '_MEIPASS'):
            ico = Path(sys._MEIPASS) / 'assets' / 'OpenSup.ico'
            if ico.exists():
                return ico
        return None
    ico = Path(__file__).resolve().parent.parent.parent / 'assets' / 'OpenSup.ico'
    return ico if ico.exists() else None


def make_header(title: str = "", width: int = 60, char: str = "\u2550") -> str:
    """Build a dynamically centred section header with double-line characters."""
    if not title:
        return char * (width + 2)
    return f" {title} ".center(width, char)


# Phase-detection patterns: when a log line matches a key, insert a section header.
# Each entry: (trigger_keyword, header_text, level)
SECTION_PATTERNS = [
    ("Parameters:",        None),  # suppress raw params line
    ("Finding all epochs", lambda: make_header("EPOCH ANALYSIS")),
    ("Starting workers",   lambda: make_header("WORKERS")),
    ("All jobs finished",  lambda: make_header()),  # bare separator
    ("Checking stream",    lambda: make_header("VERIFICATION")),
    ("Writing output",     None),  # no banner — RESULT header from _on_encode_done is sufficient
]

class TextboxHandler(logging.Handler):
    """Logging handler that writes to a CTkTextbox with level-based colours
    and structured section headers."""

    def __init__(self, textbox: ctk.CTkTextbox, max_lines: int = 5000,
                 on_line_update: callable | None = None):
        super().__init__()
        self.textbox = textbox
        self.max_lines = max_lines
        self._line_count = 0
        self.buffer: list[str] = []
        self._on_line_update = on_line_update
        self._phase_open = False  # True if we've emitted the header but not the blank line
        self._has_logged_empty_line = False  # track if we've ever inserted a bare newline
        self._triggered_phases: set[str] = set()  # track which section headers have been inserted
        # Configure colour tags on the textbox
        for level, colour in LOG_COLORS.items():
            textbox.tag_config(f"LOG_{level}", foreground=colour)
        textbox.tag_config("SECTION", foreground=COLOR_SECTION)
        textbox.tag_config("FIELD", foreground=COLOR_FIELD)
        textbox.tag_config("SUCCESS", foreground=COLOR_SUCCESS)
        textbox.tag_config("PASS", foreground=COLOR_SUCCESS)
        textbox.tag_config("FAIL", foreground=LOG_COLORS[logging.ERROR])

    def emit(self, record: logging.LogRecord) -> None:
        try:
            msg = self.format(record)
            self.textbox.after(0, self._write, msg, record.levelno)
        except Exception:
            self.handleError(record)

    def emit_raw(self, msg: str, level: int = logging.INFO) -> None:
        """Write a message with full formatting (timestamp + level prefix).

        Unlike emit(), this accepts a pre-formatted message string and creates
        a synthetic LogRecord to pass through the formatter. Used for messages
        that arrive via the pipe reader (worker stdout/stderr) or from the
        exception handler.
        """
        record = logging.LogRecord(
            name="OpenSUP", level=level, pathname="", lineno=0,
            msg=msg, args=(), exc_info=None,
        )
        formatted = self.format(record)
        self.textbox.after(0, self._write, formatted, level)

    def _write_phase_header(self, header: str) -> None:
        """Insert a section header line with timestamp + level prefix and SECTION color."""
        record = logging.LogRecord(
            name="OpenSUP", level=logging.INFO, pathname="", lineno=0,
            msg=header, args=(), exc_info=None,
        )
        formatted = self.format(record)
        tb = self.textbox
        self.buffer.append(formatted)
        tb.insert("end", formatted + "\n", "SECTION")
        self._line_count += 1
        self._phase_open = True

    def _close_phase(self) -> None:
        """Insert a blank line separator after a phase (no decorated timestamp)."""
        if self._phase_open:
            tb = self.textbox
            self.buffer.append("")
            # Insert bare newline without timestamp/level prefix — cleaner separation
            tb.insert("end", "\n")
            self._line_count += 1
            self._phase_open = False

    def _write(self, msg: str, levelno: int) -> None:
        tb = self.textbox
        self.buffer.append(msg)
        # Map levelno to nearest defined colour key
        tag = "LOG_INFO"
        for lvl in sorted(LOG_COLORS, reverse=True):
            if levelno >= lvl:
                tag = f"LOG_{lvl}"
                break
        # Check for phase transitions (deduplicated via _triggered_phases)
        for pattern, header in SECTION_PATTERNS:
            if pattern in msg and pattern not in self._triggered_phases:
                self._triggered_phases.add(pattern)
                self._close_phase()
                if header is not None:
                    header_text = header() if callable(header) else header
                    self._write_phase_header(header_text)
                elif pattern == "Parameters:":
                    # Suppress the raw params line — we show structured header instead
                    self.buffer.pop()  # remove the suppressed line from buffer
                    return
                break
        # ═══ header detection — SECTION (blue) tag for centred double-line headers
        if msg.strip().startswith("\u2550"):
            tag = "SECTION"
        # PASS/FAIL/SUCCESS tag override for results
        if "SUCCESS" in msg or levelno == PASS_LEVEL:
            tag = "SUCCESS"
        elif "FAILED" in msg or levelno == FAIL_LEVEL:
            tag = "FAIL"
        # Apply FIELD colour for indented field lines (start with "  ")
        if msg.startswith("  ") and "SUCCESS" not in msg:
            tag = "FIELD"
        # Insert message into textbox
        tb.insert("end", msg + "\n", tag)
        self._line_count += 1
        if self._line_count > self.max_lines:
            first = tb.index("1.0")
            second = tb.index(f"{self._line_count - self.max_lines + 1}.0")
            tb.delete(first, second)
            n_prune = self._line_count - self.max_lines
            self.buffer = self.buffer[n_prune:]
            self._line_count = self.max_lines
        tb.see("end")
        # Notify line count update
        if self._on_line_update:
            try:
                self._on_line_update(self._line_count)
            except Exception:
                pass


def _copy_to_clipboard(tk_root, text: str) -> bool:
    """Copy text to clipboard. Uses tkinter (Unicode-safe) first, external tools as fallback."""
    # Primary: tkinter clipboard (handles Unicode correctly on all platforms)
    try:
        tk_root.clipboard_clear()
        tk_root.clipboard_append(text)
        tk_root.update()
        return True
    except Exception:
        pass
    # Fallback: external tools (clip.exe, wl-copy, xclip)
    # On Windows, encode as UTF-16-LE with BOM for clip.exe
    for cmd in (
        ["wl-copy"],
        ["xclip", "-selection", "c"],
        ["clip.exe"],
    ):
        try:
            if cmd[-1] == "clip.exe":
                data = text.encode("utf-16-le")
            else:
                data = text.encode("utf-8")
            subprocess.run(cmd, input=data, capture_output=True, timeout=5)
            return True
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue
    return False


def _encode_worker(input_file: str, params: dict, output_file: str,
                   abort_event: threading.Event | None = None,
                   error_event: threading.Event | None = None) -> None:
    """Run encoding in a background thread with stdout/stderr capture."""
    # Track cleanup state
    r_fd = w_fd = None
    old_out = old_err = None
    reader = None
    tb_handler = None
    error_tb = None

    try:
        # Logger y TextboxHandler se necesitan antes que nada (incluso si pipe falla)
        logger_gui = logging.getLogger('OpenSUP')
        tb_handler = next((h for h in logger_gui.handlers if isinstance(h, TextboxHandler)), None)

        # Setup pipe para capturar stdout/stderr de los workers
        # En frozen --onefile + console=False en Windows, stdout/stderr son None
        # y os.dup/dup2 pueden fallar. Se captura OSError y se salta la pipe.
        r_fd = w_fd = None
        old_out = old_err = None
        try:
            r_fd, w_fd = os.pipe()
            if sys.stdout is not None:
                sys.stdout.flush()
                old_out = os.dup(1)
            if sys.stderr is not None:
                sys.stderr.flush()
                old_err = os.dup(2)
            if old_out is not None:
                os.dup2(w_fd, 1)
            if old_err is not None:
                os.dup2(w_fd, 2)
            os.close(w_fd)
            w_fd = None
        except OSError:
            # Fallback: frozen + console=False, no hay consola para capturar
            if r_fd is not None:
                try: os.close(r_fd)
                except: pass
                r_fd = None
            if w_fd is not None:
                try: os.close(w_fd)
                except: pass
                w_fd = None
            old_out = old_err = None

        # Silenciar LogFacility StreamHandler para evitar duplicados
        _stream_handlers = [
            h for h in logger_gui.handlers
            if isinstance(h, logging.StreamHandler) and h is not tb_handler
        ]
        _orig_levels = [h.level for h in _stream_handlers]
        for sh in _stream_handlers:
            sh.setLevel(logging.CRITICAL + 1)

        # Desactivar tqdm cuando stdout/stderr van a pipe (tqdm escribe \r
        # que produce WinError 1 en pipes de Windows).
        if r_fd is not None:
            from opensup.utils.logging import LogFacility
            LogFacility.disable_tqdm()

        if r_fd is not None:
            def _pipe_reader():
                buf = b""
                while True:
                    try:
                        chunk = os.read(r_fd, 4096)
                        if not chunk:
                            break
                        buf += chunk
                        while b"\n" in buf:
                            line, buf = buf.split(b"\n", 1)
                            text = line.decode("utf-8", errors="replace").strip()
                            if text and tb_handler is not None:
                                text = re.sub(r"^\S+\s+\S+\s+:\s+", "", text)
                                if re.search(r"\b(Error|ERROR|error|Failed|FAILED)\b", text):
                                    _level = logging.ERROR
                                elif re.search(r"\b(Warn|WARN|warning|WARNING)\b", text):
                                    _level = logging.WARNING
                                elif "No module" in text:
                                    _level = logging.WARNING
                                else:
                                    _level = logging.INFO
                                tb_handler.emit_raw(text, _level)
                    except OSError:
                        break
            reader = threading.Thread(target=_pipe_reader, daemon=True)
            reader.start()

        from opensup.core.interface import BDNRender
        bdnr = BDNRender(input_file, params, output_file)
        if abort_event is not None:
            bdnr._abort_event = abort_event
        bdnr.encode_input()
        if abort_event and abort_event.is_set():
            logger_gui.warning("Encoding aborted by user. Skipping output.")
            return
        bdnr.write_output()
    except BaseException as exc:
        import traceback
        is_system_exit = isinstance(exc, SystemExit)
        if not is_system_exit:
            error_tb = "".join(traceback.format_exception(type(exc), exc, exc.__traceback__))
        else:
            error_tb = str(exc) if str(exc) else "SystemExit"
        # Show error summary in GUI, full traceback to console only
        if tb_handler is not None:
            logger_gui.error(f"Encoding {'aborted' if is_system_exit else 'failed'}: {exc}")
            if error_tb and old_err is not None:
                # Escribir traceback al fd stderr ORIGINAL (duplicado antes del pipe redirect).
                # old_err no está afectado por os.dup2() a la pipe, siempre apunta a la consola.
                try:
                    _data = (f"\n{'='*60}\n  OpenSUP Error Traceback\n{'='*60}\n"
                             f"{error_tb}")
                    os.write(old_err, _data.encode('utf-8', errors='replace'))
                except OSError:
                    pass
        elif r_fd is not None:
            # No textbox handler — try to write via pipe
            try:
                os.write(r_fd, f"ERROR: {exc}\n".encode())
            except OSError:
                pass
        # Fallback: escribir error a archivo si no hay textbox ni pipe (ej: frozen)
        if tb_handler is None:
            try:
                import tempfile
                log_path = os.path.join(tempfile.gettempdir(), "opensup_error.log")
                with open(log_path, "w") as f:
                    f.write(f"OpenSUP ERROR: {exc}\n")
                    if error_tb:
                        f.write(error_tb + "\n")
            except Exception:
                pass
        # Signal error to the GUI
        if error_event is not None:
            error_event.set()
    finally:
        # Restore original stdout/stderr safely
        if old_out is not None:
            os.dup2(old_out, 1)
            os.close(old_out)
        if old_err is not None:
            os.dup2(old_err, 2)
            os.close(old_err)
        # Close pipe read end if open
        if r_fd is not None:
            try:
                os.close(r_fd)
            except OSError:
                pass
        # Join pipe reader thread
        if reader is not None:
            reader.join(timeout=2)
        # Restore StreamHandler levels
        for sh, lvl in zip(_stream_handlers, _orig_levels):
            try:
                sh.setLevel(lvl)
            except Exception:
                pass
class OpenSUPGUI:
    """Main application window - minimalist, white background, black text."""

    def __init__(self):
        self.app = ctk.CTk()
        # Set window icon (non-critical: fallback silently to default)
        if (icon_path := _icon_path()) is not None:
            try:
                self.app.iconbitmap(str(icon_path))
            except Exception:
                pass
        self.app.title(f"OpenSUP v{__version__}")
        self.app.geometry("640x800")
        self.app.minsize(640, 700)
        self.app.configure(fg_color=WHITE)

        self.input_file: str | None = None
        self.output_file: str | None = None
        self.process: threading.Thread | None = None
        self.monitor_id: str | None = None
        self.start_time: float = 0.0
        self.eta_var = ctk.StringVar(value="")
        self._abort_event = threading.Event()
        self._user_aborted = False

        # Logger setup antes de build_widgets para capturar logs tempranos
        self._setup_logging()
        self._build_widgets()

    def _setup_logging(self, textbox: ctk.CTkTextbox | None = None) -> None:
        log_logger = logging.getLogger('OpenSUP')
        # Preservar handlers existentes si solo se pasa el textbox
        if textbox is None:
            log_logger.setLevel(logging.DEBUG)
            # Añadir handler para stdout (solo una vez)
            if not any(isinstance(h, logging.StreamHandler) for h in log_logger.handlers):
                stdout_handler = logging.StreamHandler()
                stdout_handler.setFormatter(logging.Formatter(
                    "%(asctime)s \u2502 %(levelname).4s \u2502 %(message)s", datefmt="%H:%M:%S"
                ))
                stdout_handler.setLevel(logging.DEBUG)
                log_logger.addHandler(stdout_handler)
        else:
            # Eliminar solo TextboxHandlers previos, conservar StreamHandler de stdout
            for h in list(log_logger.handlers):
                if isinstance(h, TextboxHandler):
                    log_logger.removeHandler(h)
            handler = TextboxHandler(textbox, on_line_update=self._on_log_line_update)
            handler.setFormatter(logging.Formatter(
                "%(asctime)s \u2502 %(levelname).4s \u2502 %(message)s", datefmt="%H:%M:%S"
            ))
            handler.setLevel(logging.DEBUG)
            log_logger.addHandler(handler)

    def _get_textbox_handler(self) -> TextboxHandler | None:
        log_logger = logging.getLogger('OpenSUP')
        for h in log_logger.handlers:
            if isinstance(h, TextboxHandler):
                return h
        return None

    def _on_log_line_update(self, count: int) -> None:
        """Update the log line count label."""
        try:
            self.log_line_label.configure(text=f"[{count} lines]")
        except Exception:
            pass

    def _build_widgets(self):
        app = self.app
        row = 0
        pad = 14
        font_label = ("Segoe UI", 11)
        font_header = ("Segoe UI", 18, "bold")
        font_mono = ("Cascadia Code", 11) if os.name == 'nt' else ("Liberation Mono", 11)

        # Header
        ctk.CTkLabel(app, text=f"OpenSUP v{__version__} - PGS Subtitle Encoder",
                      font=font_header, text_color=BLACK,
                      anchor="w").grid(row=row, column=0, columnspan=2,
                                       padx=pad, pady=(12, 0), sticky="w")
        row += 1

        # Separator
        ctk.CTkFrame(app, height=1, fg_color=GRAY_MED).grid(
            row=row, column=0, columnspan=2, sticky="ew", padx=pad, pady=6)
        row += 1

        # Input row
        ctk.CTkButton(app, text="Select BDN XML", command=self._get_bdnxml,
                       width=130, fg_color=BLUE_ACCENT, text_color=WHITE,
                       hover_color="#1d4ed8"
                       ).grid(row=row, column=0, padx=pad, pady=3, sticky="w")
        self.bdn_label = ctk.CTkLabel(app, text="(no file selected)", anchor="w",
                                       fg_color=GRAY_LIGHT, text_color=BLACK,
                                       corner_radius=4, padx=8, height=30)
        self.bdn_label.grid(row=row, column=1, padx=(0, pad), pady=3, sticky="ew")
        row += 1

        # Output row
        ctk.CTkButton(app, text="Set Output", command=self._set_output,
                       width=130, fg_color=BLUE_ACCENT, text_color=WHITE,
                       hover_color="#1d4ed8"
                       ).grid(row=row, column=0, padx=pad, pady=3, sticky="w")
        self.out_label = ctk.CTkLabel(app, text="(not set)", anchor="w",
                                       fg_color=GRAY_LIGHT, text_color=BLACK,
                                       corner_radius=4, padx=8, height=30)
        self.out_label.grid(row=row, column=1, padx=(0, pad), pady=3, sticky="ew")
        row += 1

        # Separator
        ctk.CTkFrame(app, height=1, fg_color=GRAY_MED).grid(
            row=row, column=0, columnspan=2, sticky="ew", padx=pad, pady=4)
        row += 1

        # Compression & Acquisition
        opts = ctk.CTkFrame(app, fg_color="transparent")
        opts.grid(row=row, column=0, columnspan=2, padx=pad, pady=2, sticky="ew")

        ctk.CTkLabel(opts, text="Compression [int]%:", font=font_label,
                      text_color=BLACK).pack(side="left")
        self.comp_entry = ctk.CTkEntry(opts, width=50, justify="center",
                                        fg_color=WHITE, text_color=BLACK,
                                        border_color=GRAY_MED)
        self.comp_entry.insert(0, "85")
        self.comp_entry.pack(side="left", padx=4)

        ctk.CTkLabel(opts, text="   Acquisition rate [int]%:", font=font_label,
                      text_color=BLACK).pack(side="left")
        self.acq_entry = ctk.CTkEntry(opts, width=50, justify="center",
                                       fg_color=WHITE, text_color=BLACK,
                                       border_color=GRAY_MED)
        self.acq_entry.insert(0, "100")
        self.acq_entry.pack(side="left", padx=4)
        row += 1

        # Color space, Quantizer, Threads
        sel = ctk.CTkFrame(app, fg_color="transparent")
        sel.grid(row=row, column=0, columnspan=2, padx=pad, pady=2, sticky="ew")

        ctk.CTkLabel(sel, text="Color space:", font=font_label,
                      text_color=BLACK).pack(side="left")
        self.cs_combo = ctk.CTkComboBox(sel, values=["bt709", "bt601", "bt2020"],
                                         width=100, state="readonly",
                                         fg_color=WHITE, text_color=BLACK,
                                         border_color=GRAY_MED,
                                         button_color=GRAY_MED,
                                         button_hover_color=GRAY_DARK)
        self.cs_combo.set("bt709")
        self.cs_combo.pack(side="left", padx=4)

        ctk.CTkLabel(sel, text="  Quantizer:", font=font_label,
                      text_color=BLACK).pack(side="left")
        from opensup.media.optimizer import Quantizer
        Quantizer.init_piliq()
        q_raw = Quantizer.get_options()
        self._quant_map = {}
        q_labels = []
        for key, (name, desc) in q_raw.items():
            label = f"{name} {desc}"
            self._quant_map[label] = key
            q_labels.append(label)
        # Reorder: advanced quantizers (libimagequant, pngquant) first
        q_labels.sort(key=lambda x: (0 if x.startswith(('libimagequant', 'pngquant')) else 1, x))
        self.quant_combo = ctk.CTkComboBox(sel, values=q_labels, width=140,
                                            state="readonly",
                                            fg_color=WHITE, text_color=BLACK,
                                            border_color=GRAY_MED,
                                            button_color=GRAY_MED,
                                            button_hover_color=GRAY_DARK)
        # Default to first option (libimagequant/pngquant after reorder)
        default_label = q_labels[0] if q_labels else ""
        self.quant_combo.set(default_label)
        self.quant_combo.pack(side="left", padx=4)

        ctk.CTkLabel(sel, text="  Threads:", font=font_label,
                      text_color=BLACK).pack(side="left")
        t_opts = ['auto'] + [str(i) for i in range(1, 9)]
        self.thread_combo = ctk.CTkComboBox(sel, values=t_opts, width=80,
                                             state="readonly",
                                             fg_color=WHITE, text_color=BLACK,
                                             border_color=GRAY_MED,
                                             button_color=GRAY_MED,
                                             button_hover_color=GRAY_DARK)
        self.thread_combo.set("auto")
        self.thread_combo.pack(side="left", padx=4)
        row += 1

        # Checkboxes
        chk = ctk.CTkFrame(app, fg_color="transparent")
        chk.grid(row=row, column=0, columnspan=2, padx=pad, pady=2, sticky="w")

        self.normal_var = ctk.BooleanVar(value=False)
        self.pref_var = ctk.BooleanVar(value=False)
        self.over_var = ctk.BooleanVar(value=False)
        self.pal_var = ctk.BooleanVar(value=False)
        self.allf_var = ctk.BooleanVar(value=False)
        self.ignore_res_var = ctk.BooleanVar(value=False)

        ctk.CTkCheckBox(chk, text="Allow normal case", variable=self.normal_var,
                         text_color=BLACK).grid(row=0, column=0, sticky="w", padx=4, pady=1)
        ctk.CTkCheckBox(chk, text="Prefer normal case", variable=self.pref_var,
                         text_color=BLACK).grid(row=1, column=0, sticky="w", padx=4, pady=1)
        ctk.CTkCheckBox(chk, text="Allow overlap buffering", variable=self.over_var,
                         text_color=BLACK).grid(row=2, column=0, sticky="w", padx=4, pady=1)
        ctk.CTkCheckBox(chk, text="Write full palette", variable=self.pal_var,
                         text_color=BLACK).grid(row=0, column=1, sticky="w", padx=20, pady=1)
        ctk.CTkCheckBox(chk, text="Both SUP + PES/MUI", variable=self.allf_var,
                         text_color=BLACK).grid(row=1, column=1, sticky="w", padx=20, pady=1)
        ctk.CTkCheckBox(chk, text="Ignore resolution validation", variable=self.ignore_res_var,
                         text_color=BLACK).grid(row=2, column=1, sticky="w", padx=20, pady=1)
        row += 1

        # Extra acq, anchor, SSIM
        adv = ctk.CTkFrame(app, fg_color="transparent")
        adv.grid(row=row, column=0, columnspan=2, padx=pad, pady=2, sticky="ew")
        ctk.CTkLabel(adv, text="Extra acq after N updates:", font=font_label,
                      text_color=BLACK).pack(side="left")
        self.extra_acq_entry = ctk.CTkEntry(adv, width=40, justify="center",
                                             fg_color=WHITE, text_color=BLACK,
                                             border_color=GRAY_MED)
        self.extra_acq_entry.insert(0, "3")
        self.extra_acq_entry.pack(side="left", padx=4)

        ctk.CTkLabel(adv, text="  Max bitrate [Kbps]:", font=font_label,
                      text_color=BLACK).pack(side="left")
        self.max_kbps_entry = ctk.CTkEntry(adv, width=60, justify="center",
                                            fg_color=WHITE, text_color=BLACK,
                                            border_color=GRAY_MED)
        self.max_kbps_entry.insert(0, "16000")
        self.max_kbps_entry.pack(side="left", padx=4)
        row += 1

        # Separator
        ctk.CTkFrame(app, height=1, fg_color=GRAY_MED).grid(
            row=row, column=0, columnspan=2, sticky="ew", padx=pad, pady=4)
        row += 1

        # Progress bar
        self.progress = ctk.CTkProgressBar(app, height=14, corner_radius=7,
                                            fg_color=GRAY_LIGHT,
                                            progress_color=GREEN_BTN)
        self.progress.grid(row=row, column=0, columnspan=2, padx=pad, pady=(0, 2), sticky="ew")
        self.progress.set(0)
        row += 1

        # Progress info
        prog_info = ctk.CTkFrame(app, fg_color="transparent")
        prog_info.grid(row=row, column=0, columnspan=2, padx=pad, sticky="ew")
        self.progress_pct = ctk.CTkLabel(prog_info, text="0%",
                                          font=("Segoe UI", 12, "bold"),
                                          text_color=BLACK)
        self.progress_pct.pack(side="left", padx=4)
        self.progress_eta = ctk.CTkLabel(prog_info, textvariable=self.eta_var,
                                          font=("Segoe UI", 12),
                                          text_color=GRAY_DARK)
        self.eta_var.set("")
        self.progress_eta.pack(side="left", padx=4)
        row += 1

        # Log panel header + copy + clear buttons
        log_hdr = ctk.CTkFrame(app, fg_color="transparent")
        log_hdr.grid(row=row, column=0, columnspan=2, padx=pad, pady=(4, 0), sticky="ew")
        log_hdr.grid_columnconfigure(0, weight=1)
        log_title_frame = ctk.CTkFrame(log_hdr, fg_color="transparent")
        log_title_frame.grid(row=0, column=0, sticky="w")
        ctk.CTkLabel(log_title_frame, text="Log output", font=("Segoe UI", 11, "bold"),
                      text_color=BLACK, anchor="w").pack(side="left")
        self.log_line_label = ctk.CTkLabel(log_title_frame, text="[0 lines]",
                                            font=("Segoe UI", 10),
                                            text_color=GRAY_DARK, anchor="w")
        self.log_line_label.pack(side="left", padx=(6, 0))

        btn_row = ctk.CTkFrame(log_hdr, fg_color="transparent")
        btn_row.grid(row=0, column=1, sticky="e")
        self.clear_btn = ctk.CTkButton(btn_row, text="Clear", width=60, height=24,
                                        command=self._clear_log,
                                        fg_color=GRAY_LIGHT, text_color=BLACK,
                                        hover_color=GRAY_MED, font=("Segoe UI", 10),
                                        corner_radius=4)
        self.clear_btn.pack(side="right", padx=(4, 0))
        self.copy_btn = ctk.CTkButton(btn_row, text="Copy log", width=80, height=24,
                                       command=self._copy_log,
                                       fg_color=GRAY_LIGHT, text_color=BLACK,
                                       hover_color=GRAY_MED, font=("Segoe UI", 10),
                                       corner_radius=4)
        self.copy_btn.pack(side="right")
        row += 1

        # Log textbox (normal state + intercept typing = selectable + Ctrl+C)
        self.log_textbox = ctk.CTkTextbox(app, wrap="word", state="normal",
                                            fg_color=WHITE, text_color=BLACK,
                                            border_width=1, border_color=GRAY_MED,
                                            font=font_mono)
        self.log_textbox.bind("<Key>", lambda e: "break")  # prevent typing
        self.log_textbox.grid(row=row, column=0, columnspan=2, padx=pad, pady=(2, 6),
                               sticky="nsew")
        app.grid_rowconfigure(row, weight=1)
        row += 1
        # Conectar el TextboxHandler (reemplaza el StreamHandler temporal)
        self._setup_logging(textbox=self.log_textbox)

        # Action buttons
        btn_frame = ctk.CTkFrame(app, fg_color="transparent")
        btn_frame.grid(row=row, column=0, columnspan=2, pady=(4, 10))
        self.go_btn = ctk.CTkButton(btn_frame, text="Make it SUP!",
                                     command=self._start_encode, state="disabled",
                                     fg_color=GREEN_BTN, text_color=WHITE,
                                     hover_color="#14532d",
                                     width=160, height=36,
                                     font=("Segoe UI", 13, "bold"), corner_radius=6)
        self.go_btn.pack(side="left", padx=8)
        self.abort_btn = ctk.CTkButton(btn_frame, text="Abort",
                                        command=self._abort, state="disabled",
                                        fg_color=RED_BTN, text_color=WHITE,
                                        hover_color="#450a0a",
                                        width=160, height=36, corner_radius=6)
        self.abort_btn.pack(side="left", padx=8)
        row += 1

        # Column weights
        app.grid_columnconfigure(1, weight=1)

    # ── File dialogs ───────────────────────────────────────────────
    def _get_bdnxml(self):
        from tkinter import filedialog
        fp = filedialog.askopenfilename(title="Select BDN XML file",
                                         filetypes=[("BDN XML files", "*.xml"), ("All files", "*.*")])
        if fp:
            self.input_file = fp
            self.bdn_label.configure(text=os.path.basename(fp))
            self._check_ready()

    def _set_output(self):
        from tkinter import filedialog
        fp = filedialog.asksaveasfilename(title="Set output SUP file",
                                           defaultextension=".sup",
                                           filetypes=[("SUP files", "*.sup"), ("All files", "*.*")])
        if fp:
            self.output_file = fp
            self.out_label.configure(text=os.path.basename(fp))
            self._check_ready()

    def _check_ready(self):
        if self.input_file and self.output_file:
            self.go_btn.configure(state="normal")

    # ── Clear log ──────────────────────────────────────────────────
    def _clear_log(self) -> None:
        self.log_textbox.configure(state="normal")
        self.log_textbox.delete("1.0", "end")
        self.log_textbox.configure(state="normal")
        handler = self._get_textbox_handler()
        if handler is not None:
            handler.buffer.clear()
            handler._line_count = 0

    # ── Copy log ───────────────────────────────────────────────────
    def _copy_log(self) -> None:
        try:
            handler = self._get_textbox_handler()
            if handler is not None:
                content = "\n".join(handler.buffer)
            else:
                content = self.log_textbox.get("1.0", "end-1c")
            # Sanitize: replace UTF-8 box-drawing with ASCII for Windows clipboard compat
            safe_content = content.replace("\u2550", "=").replace("\u2500", "=")
            if safe_content.strip() and _copy_to_clipboard(self.app, safe_content):
                old = self.copy_btn.cget("text")
                self.copy_btn.configure(text="Copied", fg_color=GREEN_BTN, text_color=WHITE)
                self.app.after(2000, lambda: self.copy_btn.configure(text="Copy log",
                                fg_color=GRAY_LIGHT, text_color=BLACK))
        except Exception:
            pass

    # ── Encode ─────────────────────────────────────────────────────
    def _collect_params(self) -> dict:
        q_label = self.quant_combo.get()
        q_key = self._quant_map.get(q_label, None)
        q_idx = q_key.value if hasattr(q_key, 'value') else 3
        return {
            'quality_factor': max(1, min(100, int(self.comp_entry.get() or "85"))) / 100,
            'refresh_rate': max(1, min(100, int(self.acq_entry.get() or "100"))) / 100,
            'quantize_lib': q_idx,
            'bt_colorspace': self.cs_combo.get(),
            'allow_overlaps': self.over_var.get(),
            'full_palette': self.pal_var.get(),
            'output_all_formats': self.allf_var.get(),
            'allow_normal_case': self.normal_var.get(),
            'prefer_normal_case': self.pref_var.get(),
            'max_kbps': int(self.max_kbps_entry.get() or "16000"),
            'log_to_file': 0,
            'insert_acquisitions': int(self.extra_acq_entry.get() or "3"),
            'ssim_tol': 0.0,
            'redraw_period': 0.0,
            'threads': self.thread_combo.get(),
            'ignore_resolution': self.ignore_res_var.get(),
        }

    def _start_encode(self):
        self.go_btn.configure(state="disabled")
        self.abort_btn.configure(state="normal")
        self.progress.set(0)
        self.progress_pct.configure(text="0%")
        self.eta_var.set("Starting...")

        params = self._collect_params()
        params['ini_opts'] = {'super_cfg': {}}

        # Reset section phase tracking for a new encoding run
        for handler in logging.getLogger('OpenSUP').handlers:
            if isinstance(handler, TextboxHandler):
                handler._triggered_phases.clear()
                break

        # Encoding summary header (Model A)
        logger_gui = logging.getLogger('OpenSUP')
        logger_gui.iinfo(make_header("ENCODING START"))
        logger_gui.iinfo(f"  Input:  {Path(self.input_file).name}")
        logger_gui.iinfo(f"  Output: {Path(self.output_file).name}")
        logger_gui.iinfo(f"  Params: {params['quality_factor']*100:.0f}% | "
                         f"{self.quant_combo.get()} | {params['bt_colorspace']} | "
                         f"{params['threads']} threads | "
                         f"{params['max_kbps']} Kbps")

        self._user_aborted = False
        self._abort_event.clear()
        self._encode_error_event = threading.Event()
        self.start_time = time.monotonic()
        self.process = threading.Thread(
            target=_encode_worker,
            args=(self.input_file, params, self.output_file, self._abort_event,
                  self._encode_error_event),
            daemon=True,
        )
        self.process.start()
        self.monitor_id = self.app.after(500, self._monitor_progress)

    def _monitor_progress(self):
        if self.process is None:
            self._on_encode_done()
            return
        if not self.process.is_alive():
            self.process.join(timeout=2)
            self._on_encode_done()
            return
        elapsed = time.monotonic() - self.start_time
        mins, secs = divmod(int(elapsed), 60)
        self.eta_var.set(f"Elapsed: {mins:02d}:{secs:02d}")
        self.monitor_id = self.app.after(500, self._monitor_progress)

    def _on_encode_done(self):
        self.process = None
        if self.monitor_id:
            self.app.after_cancel(self.monitor_id)
            self.monitor_id = None
        had_error = (hasattr(self, '_encode_error_event') and
                     self._encode_error_event is not None and
                     self._encode_error_event.is_set())
        logger_gui = logging.getLogger('OpenSUP')
        if self._user_aborted:
            self._user_aborted = False
            self.progress.set(0)
            self.progress_pct.configure(text="ABORTED", text_color=RED_BTN)
            self.eta_var.set("Encoding aborted by user")
            logger_gui.iinfo(make_header("RESULT"))
            logger_gui.warning("  Status: ABORTED")
            logger_gui.info(make_header("END"))
        elif had_error:
            self.progress.set(0)
            self.progress_pct.configure(text="FAILED", text_color=RED_BTN)
            self.eta_var.set("Encoding FAILED - see log for details")
            logger_gui.iinfo(make_header("RESULT"))
            logger_gui.fail_("  Status: FAILED")
            logger_gui.info("  >> See error messages above for details")
            logger_gui.info(make_header("END"))
        else:
            self.progress.set(1.0)
            self.progress_pct.configure(text="100%")
            elapsed = time.monotonic() - self.start_time
            self.eta_var.set(f"Done in {timedelta(seconds=int(elapsed))}")
            logger_gui.iinfo(make_header("RESULT"))
            logger_gui.pass_(f"  File:   {self.output_file}")
            logger_gui.pass_(f"  Time:   {timedelta(seconds=int(elapsed))}")
            logger_gui.pass_("  Status: SUCCESS")
            logger_gui.iinfo(make_header("END"))
        self.go_btn.configure(state="normal")
        self.abort_btn.configure(state="disabled")

    def _kill_child_processes(self):
        """Kill all child processes of the current process (mp.Pool workers)."""
        log = logging.getLogger('OpenSUP')
        pid = os.getpid()
        try:
            if os.name == 'nt':
                # Use wmic to get child PIDs, then taskkill each
                try:
                    result = subprocess.run(
                        ["wmic", "process", "where", f"ParentProcessId={pid}",
                         "get", "ProcessId"],
                        capture_output=True, text=True, timeout=5,
                    )
                except subprocess.TimeoutExpired:
                    log.warning("wmic timed out enumerating child processes.")
                    return
                except Exception:
                    return
                for line in result.stdout.strip().splitlines():
                    line = line.strip()
                    if line.isdigit():
                        child_pid = int(line)
                        if child_pid == pid:
                            continue  # Never attempt to kill ourselves
                        try:
                            os.kill(child_pid, signal.SIGTERM)
                        except (OSError, PermissionError):
                            try:
                                subprocess.run(
                                    ["taskkill", "/F", "/PID", str(child_pid)],
                                    capture_output=True, timeout=3,
                                )
                            except Exception:
                                pass
            else:
                # Linux: use pgrep -P to find children (fast, single call)
                try:
                    result = subprocess.run(
                        ["pgrep", "-P", str(pid)],
                        capture_output=True, text=True, timeout=3,
                    )
                    for line in result.stdout.strip().splitlines():
                        line = line.strip()
                        if line.isdigit() and int(line) != pid:
                            try:
                                os.kill(int(line), signal.SIGKILL)
                            except OSError:
                                pass
                except (subprocess.TimeoutExpired, FileNotFoundError):
                    pass
                except Exception:
                    pass
        except Exception as exc:
            log.warning(f"_kill_child_processes: {exc}")

    def _abort(self):
        log = logging.getLogger('OpenSUP')
        try:
            if self.process is not None and self.process.is_alive():
                self._abort_event.set()
                self._kill_child_processes()
                self.process = None
            if self.monitor_id:
                self.app.after_cancel(self.monitor_id)
                self.monitor_id = None
            self._user_aborted = True
        except Exception as exc:
            log.error(f"Error during abort: {exc}")
        finally:
            self.go_btn.configure(state="normal")
            self.abort_btn.configure(state="disabled")
            log.warning("Encoding aborted by user.")

    def run(self):
        self.app.mainloop()


def main():
    gui = OpenSUPGUI()
    gui.run()


if __name__ == '__main__':
    main()
