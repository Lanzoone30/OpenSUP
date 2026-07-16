#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
cli.py

Command-line interface for OpenSUP.

Provides headless PGS subtitle encoding from BDN XML sources,
supporting all quantizer backends and checkbox modes. Designed
for batch processing, CI pipelines, and power users who need
full control without a graphical desktop.

Why this exists: CLI enables automation and remote execution
where a GUI is unavailable or impractical.
"""

# Copyright (C) 2023-2026 cibo
# This file is part of OpenSUP, based on SUPer <https://github.com/cubicibo/SUPer>.
#
# OpenSUP is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

import multiprocessing as mp
import os
import sys
import time
import configparser
from pathlib import Path
from argparse import ArgumentParser, BooleanOptionalAction
from typing import NoReturn, Union
from datetime import timedelta

from opensup.core.interface import BDNRender
from opensup.utils.logging import LogFacility
from opensup.__metadata__ import __author__, __version__ as LIB_VERSION

logger = LogFacility.get_logger('OpenSUP')


def exit_msg(msg: str, is_error: bool = True) -> NoReturn:
    if msg:
        if is_error:
            logger.critical(msg)
        else:
            logger.info(msg)
    sys.exit(is_error)


def check_output(fp: Union[Path, str], overwrite: bool) -> tuple[str, str]:
    fp = Path(fp)
    if fp.exists() and not overwrite:
        exit_msg("Output file already exist, not overwriting.")
    if fp.name.find('.') == -1:
        logger.warning("No extension provided, assuming .SUP.")
        fp = str(fp) + '.sup'
        ext = 'sup'
    elif (ext := fp.name.split('.')[-1].lower()) not in ['pes', 'sup']:
        exit_msg("Not a known PG stream extension, aborting.")
    return str(os.path.expandvars(os.path.expanduser(fp))), ext


class BruleCapAction(BooleanOptionalAction):
    def __init__(self, option_strings, dest, nargs=None, **kwargs):
        super().__init__(option_strings, dest, **kwargs)

    def __call__(self, parser, namespace, values=None, option_string=None):
        from brule import LayoutEngine, Brule, HexTree, QtzrUTC
        f_strcap = lambda caps: ', '.join(c for c in caps if c is not None)
        print(f"LayoutEngine: {f_strcap(LayoutEngine.get_capabilities())}")
        print(f"   RLE codec: {f_strcap(Brule.get_capabilities())}")
        print(f"     HexTree: {f_strcap(HexTree.get_capabilities())}")
        print(f"     QtzrUTC: {f_strcap(QtzrUTC.get_capabilities())}")
        exit_msg('', is_error=False)


def build_parser() -> ArgumentParser:
    parser = ArgumentParser(prog='opensup', description='OpenSUP - PGS Subtitle Encoder')
    parser.add_argument("-i", "--input", type=str, required=True, help="Set input BDNXML file.")
    parser.add_argument('-c', '--compression', type=int, default=80, help="Compression rate [0-100] (def: %(default)s)")
    parser.add_argument('-a', '--acqrate', type=int, default=100, help="Acquisition rate [0-100] (def: %(default)s)")
    parser.add_argument('-q', '--quantizer', type=int, default=3, help="Quantizer [0:QtzrUTC, 1:Pillow, 2:HexTree, 3:PNGQ/LIQ]")
    parser.add_argument('-k', '--prefer-normal', action='store_true', default=False)
    parser.add_argument('-n', '--allow-normal', action='store_true', default=False)
    parser.add_argument('-b', '--bt', type=int, default=709, help="BT matrix [601, 709, 2020]")
    parser.add_argument('-p', '--palette', action='store_true', default=False)
    parser.add_argument('-d', '--ahead', action='store_true', default=False)
    parser.add_argument('-y', '--yes', action='store_true', default=False, help="Overwrite existing output.")
    parser.add_argument('-w', '--withsup', action='store_true', default=False)
    parser.add_argument('-e', '--extra-acq', type=int, default=2)
    parser.add_argument('-m', '--max-kbps', type=int, default=0)
    parser.add_argument('-l', '--log-to-file', type=int, default=0)
    parser.add_argument('-t', '--threads', type=int, default=0)
    parser.add_argument('--layout', type=int, default=-1)
    parser.add_argument('--capabilities', action=BruleCapAction)
    parser.add_argument('--redraw-period', type=float, default=0.0)
    parser.add_argument('--ssim-tol', type=int, default=0)
    parser.add_argument('--ignore-resolution', action='store_true', default=False,
                        help="Ignore Blu-ray resolution validation and accept any resolution.")
    parser.add_argument('-v', '--version', action='version', version=f"(c) {__author__}, v{LIB_VERSION}")
    parser.add_argument("output", type=str)
    return parser


def main() -> int:
    mp.freeze_support()
    parser = build_parser()
    args = parser.parse_args()

    print(f"OpenSUP version {LIB_VERSION} - (c) 2025 cubicibo")
    print("HDMV PGS encoder.")

    args.output, ext = check_output(args.output, args.yes)

    # Validate args
    assert abs(args.ssim_tol) <= 100
    assert 0 <= args.compression <= 100
    assert 0 <= args.acqrate <= 100
    if args.quantizer not in range(0, 5):
        logger.warning("Unknown quantization mode, using pngquant/libimagequant.")
        args.quantizer = 3
    if args.bt not in [601, 709, 2020]:
        logger.warning("Unknown transfer matrix, using bt709.")
        args.bt = 709

    parameters = {'ini_opts': {'super_cfg': {}}}
    # Load config.ini
    try:
        app_path = Path(sys.argv[0]).resolve().parent
    except Exception:
        app_path = Path(sys.argv[0]).absolute().parent
    config_file = app_path.joinpath('config.ini')
    if config_file.exists():
        cfg = configparser.ConfigParser()
        cfg.read(config_file)
        if (sc := cfg['SUPer']) if 'SUPer' in cfg else None:
            parameters['ini_opts']['super_cfg'] = dict(sc)
            if int(parameters['ini_opts']['super_cfg'].pop('abort_on_error', 0)):
                LogFacility.exit_on_error(logger)
    else:
        logger.error("config.ini not found!")

    # Propagate ignore_resolution to params
    parameters['ignore_resolution'] = args.ignore_resolution

    parameters |= {
        'quality_factor': args.compression / 100,
        'refresh_rate': args.acqrate / 100,
        'quantize_lib': args.quantizer,
        'bt_colorspace': f"bt{args.bt}",
        'allow_overlaps': args.ahead,
        'full_palette': args.palette,
        'output_all_formats': args.withsup,
        'allow_normal_case': args.allow_normal,
        'prefer_normal_case': args.prefer_normal,
        'max_kbps': args.max_kbps,
        'log_to_file': args.log_to_file,
        'insert_acquisitions': args.extra_acq,
        'ssim_tol': args.ssim_tol / 100,
        'redraw_period': args.redraw_period,
        'threads': 'auto' if args.threads == 0 else args.threads,
    }
    ts_start = time.monotonic()
    bdnr = BDNRender(args.input, parameters, args.output)
    bdnr.encode_input()
    bdnr.write_output()
    exit_msg(f"Success. Duration: {timedelta(seconds=round(time.monotonic() - ts_start, 3))}", False)
    return 0


if __name__ == '__main__':
    sys.exit(main())
