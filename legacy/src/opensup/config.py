"""
Configuration loading for OpenSUP.

Reads ``config.ini`` from the application directory and merges
user-defined options with the encode pipeline defaults. Keeps
configuration external so users can tweak settings without
modifying source code.
"""
import configparser
import os
from pathlib import Path
from typing import Optional

from opensup.utils.logging import LogFacility


def load_config_file(app_path: Path, quantizer_index: int, logger) -> dict:
    """Load config.ini and return merged options."""
    ini_opts: dict = {'super_cfg': {}}
    cfg_file = app_path.joinpath('config.ini')
    if not cfg_file.exists():
        logger.error("config.ini not found!")
        return ini_opts

    cfg = configparser.ConfigParser()
    cfg.read(cfg_file)

    if (sc := _get_section(cfg, 'SUPer')) is not None:
        ini_opts['super_cfg'] |= dict(sc)
        if int(ini_opts['super_cfg'].pop('abort_on_error', 0)):
            LogFacility.exit_on_error(logger)

    if quantizer_index >= 3:
        exepath = None
        piq_values = {}
        if (piq := _get_section(cfg, 'PILIQ')) is not None:
            if (exepath := piq.pop('quantizer', None)) is not None and not os.path.isabs(exepath):
                exepath = str(app_path.joinpath(exepath))
            piq_values |= {k: int(v) for k, v in piq.items()}
        ini_opts['quant'] = {'qpath': exepath} | piq_values
    return ini_opts


def get_application_path() -> Path:
    import sys
    try:
        return Path(sys.argv[0]).resolve().parent
    except Exception:
        return Path(sys.argv[0]).absolute().parent


def _get_section(cfg: configparser.ConfigParser, key: str) -> Optional[dict]:
    try:
        return dict(cfg[key])
    except KeyError:
        return None
