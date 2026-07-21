#!/usr/bin/env python3
"""
logging.py

Centralised logging infrastructure for OpenSUP.

Provides LogFacility — a singleton-style logger factory that
attaches console, file, and GUI textbox handlers. Also wraps
tqdm progress bars so they integrate cleanly with the logging
output without corrupting the progress display.

Why this design: a single logging entry point avoids scattered
logger configuration across the codebase and ensures consistent
formatting whether running in CLI, GUI, or frozen (PyInstaller).
"""

# Copyright (C) 2024-2026 cibo
# This file is part of OpenSUP, based on SUPer <https://github.com/cubicibo/SUPer>.
#
# OpenSUP is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# OpenSUP is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with OpenSUP.  If not, see <http://www.gnu.org/licenses/>.

import os
import sys
import logging
from contextlib import nullcontext
from logging.handlers import BufferingHandler

try:
    from tqdm import tqdm
except ModuleNotFoundError:
    tqdm = nullcontext


class LogFacility:
    _logger = dict()
    _logpbar = dict()
    _tqdm_off = False

    @classmethod
    def set_file_log(cls, logger: logging.Logger, fp: str, level: int | None = None, simple_format: bool = False) -> None:
        if level is None:
            level = logger.level
        lfh = logging.FileHandler(fp, mode='w')
        formatter = logging.Formatter('%(message)s' if simple_format else '%(levelname).8s: %(message)s')
        lfh.setFormatter(formatter)
        if logger.getEffectiveLevel() > level:
            cls.set_logger_level(logger.name, level)
        lfh.setLevel(level)
        logger.addHandler(lfh)

    @classmethod
    def _init_logger(cls, name: str, with_handler: bool = True) -> None:
        cls._extend_logger()
        logger = cls._logger[name] = logging.getLogger(name)

        if not logger.hasHandlers() and with_handler:
            handler = logging.StreamHandler()
            formatter = logging.Formatter(' %(name)s %(levelname).4s : %(message)s')
            handler.setFormatter(formatter)
            logger.addHandler(handler)

    @classmethod
    def set_logger_level(cls, name: str, level: int) -> None:
        assert cls._logger.get(name, None) is not None
        cls._logger[name].setLevel(level)
        if len(cls._logger[name].handlers):
            cls._logger[name].handlers[0].setLevel(level)

    @classmethod
    def get_buffered_msgs(cls, logger: logging.Logger) -> list[str] | None:
        for hdl in logger.handlers:
            if isinstance(hdl, BufferingHandler):
                fmsgs = [(rec.levelno, rec.getMessage()) for rec in hdl.buffer]
                hdl.flush()
                return fmsgs
        return None

    @classmethod
    def exit_on_error(cls, logger: logging.Logger) -> None:
        class ErrorExit:
            def __init__(self, log_error_f) -> None:
                self.f_log_error = log_error_f
            def __call__(self, *args, **kwargs) -> None:
                self.f_log_error(*args, **kwargs)
                self.f_log_error("Error occured in strict mode. Terminating.")
                import sys
                sys.exit(1)

        if getattr(logger.error.__class__, "__name__", None) != 'ErrorExit':
            logger.error = ErrorExit(logger.error)

    @classmethod
    def set_logger_buffer(cls, logger: logging.Logger) -> None:
        hdl = BufferingHandler(float('inf'))
        hdl.setLevel(logging.INFO)
        logger.addHandler(hdl)

    @classmethod
    def get_logger(cls, name: str, level: int = logging.INFO, with_handler: bool = True) -> logging.Logger:
        if cls._logger.get(name, None) is None:
            cls._init_logger(name, with_handler)
            cls.set_logger_level(name, level)
        return cls._logger[name]

    @staticmethod
    def _extend_logger() -> None:
        if getattr(logging.Logger, 'iinfo', None) is not None:
            return
        INFO_OUT = logging.INFO + 5
        logging.addLevelName(INFO_OUT, "INFO")
        def info_out(self, message, *args, **kws):
            self._log(INFO_OUT, message, args, **kws)
        logging.Logger.iinfo = info_out

        INFO_EXT = logging.INFO + 1
        logging.addLevelName(INFO_EXT, "INFO")
        def einfo_out(self, message, *args, **kws):
            self._log(INFO_EXT, message, args, **kws)
        logging.Logger.einfo = einfo_out

        LOW_DEBUG = logging.DEBUG - 5
        logging.addLevelName(LOW_DEBUG, "LDEBUG")
        def low_debug(self, message, *args, **kws):
            self._log(LOW_DEBUG, message, args, **kws)
        logging.Logger.ldebug = low_debug

        HIGH_DEBUG = logging.DEBUG - 2
        logging.addLevelName(HIGH_DEBUG, "HDEBUG")
        def high_debug(self, message, *args, **kws):
            self._log(HIGH_DEBUG, message, args, **kws)
        logging.Logger.hdebug = high_debug

        PASS = logging.WARNING + 5  # 35
        logging.addLevelName(PASS, "PASS")
        def _pass(self, message, *args, **kws):
            self._log(PASS, message, args, **kws)
        logging.Logger.pass_ = _pass

        FAIL = logging.ERROR + 5  # 45
        logging.addLevelName(FAIL, "FAIL")
        def _fail(self, message, *args, **kws):
            self._log(FAIL, message, args, **kws)
        logging.Logger.fail_ = _fail

    @classmethod
    def disable_tqdm(cls) -> None:
        cls._tqdm_off = True

    @classmethod
    def close_progress_bar(cls, logger: logging.Logger):
        if cls._logger.get(logger.name, None) != None and cls._logpbar.get(logger.name, None) is not None:
            cls._logpbar[logger.name].close()
            cls._logpbar[logger.name] = None

    @classmethod
    def get_progress_bar(cls, logger: logging.Logger, tot: ...) -> tqdm | None:
        if cls._logger.get(logger.name, None) is None:
            return None
        if cls._logpbar.get(logger.name, None) is not None:
            return cls._logpbar[logger.name]
        if logger.getEffectiveLevel() >= logging.INFO and not cls._tqdm_off:
            _tqdm_file = sys.stderr if sys.stderr is not None else open(os.devnull, 'w')
            pbar = tqdm(tot, file=_tqdm_file)
        else:
            pbar = nullcontext()
            pbar.n = 0
        if getattr(pbar, 'update', None) is None:
            pbar.update = pbar.close = pbar.set_description = pbar.reset = pbar.refresh = pbar.clear = lambda *args, **kwargs: None
        cls._logpbar[logger.name] = pbar
        return pbar
