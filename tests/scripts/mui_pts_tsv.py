#!/usr/bin/env python3
"""Emit one TSV row per MUI record: type, pts, dts.

Usage: mui_pts_tsv.py <twin.sup> <pes.mui>

Uses the oracle reader (scenaristream.TSPair.from_mui) as the PTS/DTS contract,
so the values are exactly what the reference decoder yields. The <twin.sup>
argument is accepted for call-site symmetry; it is not needed to read the MUI.
"""
import os
import sys
from typing import Any, cast

_REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(_REPO, "temp", "SUPer-main"))

from scenaristream import MUIType, TSPair


def read_mui(path):
    data = open(path, "rb").read()
    if len(data) < 4 or data[3] != int(MUIType.GRAPHICS):
        raise SystemExit("not a graphics MUI: " + path)
    i, rows = 4, []
    # Record: type(1) + block_length(4) + timestamp(9). Tail is 0xFF + 13 zeros.
    while i + 14 <= len(data) and data[i] != 0xFF:
        # The lib annotates a tuple but returns a TSPair; cast keeps both honest.
        ts = cast(Any, TSPair.from_mui(data[i + 5:i + 14]))
        rows.append((data[i], ts.pts, ts.dts))
        i += 14
    return rows


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    mui = sys.argv[2] if len(sys.argv) > 2 else sys.argv[1]
    for seg_type, pts, dts in read_mui(mui):
        print("0x%02X\t%d\t%d" % (seg_type, pts, dts))


if __name__ == "__main__":
    main()
