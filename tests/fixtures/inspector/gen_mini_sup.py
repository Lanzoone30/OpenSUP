#!/usr/bin/env python3
# Copyright (C) 2024-2026 Lanzoone30
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Spec 034 fixture: a tiny, deterministic .sup (1 epoch, 2 display sets) built
# with the PGS byte layout the port's segment structs read/write. Regenerate:
#   python3 tests/fixtures/inspector/gen_mini_sup.py
import struct
from pathlib import Path

W, H, FPS = 1920, 1080, 25
WIN = (100, 100, 64, 32)          # x, y, w, h
RLE_W, RLE_H = 64, 32
OUT = Path(__file__).with_name("mini.sup")


def seg(stype, pts, dts, payload):
    return (b"PG" + struct.pack(">II", pts, dts) + bytes([stype])
            + struct.pack(">H", len(payload)) + payload)


def pcs(comp_n, state, pal_flag, objs):
    p = struct.pack(">HHBHB", W, H, 0x10, comp_n, state)
    p += bytes([0x80 if pal_flag else 0, 0, len(objs)])
    for oid, wid, flags, x, y in objs:
        p += struct.pack(">HBBHH", oid, wid, flags, x, y)
    return p


def wds(windows):
    p = bytes([len(windows)])
    for wid, x, y, w, h in windows:
        p += struct.pack(">BHHHH", wid, x, y, w, h)
    return p


def pds(pid, vn, entries):
    p = bytes([pid, vn])
    for idx, y, cr, cb, t in entries:
        p += bytes([idx, y, cr, cb, t])
    return p


def ods(oid, flags, w, h, rle):
    return (struct.pack(">HBB", oid, 0, flags)
            + (4 + len(rle)).to_bytes(3, "big")
            + struct.pack(">HH", w, h) + rle)


def rle_band():
    # 64x32: rows 8..23 are color 1, the rest transparent (0). Extended runs:
    # 0x00 0x40 hi lo = zero run, 0x00 0xC0 hi lo color = non-zero run.
    rows = []
    for y in range(RLE_H):
        if 8 <= y < 24:
            line = bytes([0x00, 0xC0 | (64 >> 8), 64 & 0xFF, 0x01])
        else:
            line = bytes([0x00, 0x40 | (64 >> 8), 64 & 0xFF])
        rows.append(line + b"\x00\x00")
    return b"".join(rows)


def main():
    rle = rle_band()
    t0, wds_delta, wipe = 900000, 6, 5832
    buf = bytearray()

    # DS0: epoch start (EPOCH_START, full palette, one object).
    buf += seg(0x16, t0, t0 - 6000, pcs(0, 0x80, False, [(1, 0, 0, WIN[0], WIN[1])]))
    buf += seg(0x17, t0 - wds_delta, t0 - 6000, wds([(0, *WIN)]))
    buf += seg(0x14, t0 - 5, t0 - 5, pds(0, 0, [
        (0x00, 16, 128, 128, 0),
        (0x01, 235, 128, 128, 255),
        (0xFF, 128, 128, 128, 0),
    ]))
    buf += seg(0x15, t0 - 2, t0 - 4, ods(1, 0xC0, RLE_W, RLE_H, rle))
    buf += seg(0x80, t0, t0 - 1, b"")

    # DS1: acquisition at t0 + one frame, same window, new object version.
    t1 = t0 + 3600
    buf += seg(0x16, t1, t1 - 3600, pcs(1, 0x40, False, [(1, 0, 0, WIN[0], WIN[1])]))
    buf += seg(0x17, t1 - wds_delta, t1 - 3600, wds([(0, *WIN)]))
    buf += seg(0x15, t1 - 2, t1 - 3600, ods(1, 0xC0, RLE_W, RLE_H, rle))
    buf += seg(0x80, t1, t1 - 1, b"")

    OUT.write_bytes(bytes(buf))
    print(f"wrote {OUT} ({len(buf)} bytes, {wipe=})")


if __name__ == "__main__":
    main()
