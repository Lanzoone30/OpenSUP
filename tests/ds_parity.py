#!/usr/bin/env python3
"""DS-level parity harness: compare composition_state per display set.

Aligns two .sup streams by PTS and reports every mismatch. Exit code is
non-zero when the streams disagree.

State byte values (fields at PCS payload offset 7):
  NORMAL=0x00, ACQUISITION=0x40, EPOCH_START=0x80

Usage:
  ds_parity.py <motor.sup> <oracle.sup>
"""
import struct
import sys

STATES = {0x00: "NORMAL", 0x40: "ACQUISITION", 0x80: "EPOCH_START"}


def pcs_states(path):
    with open(path, "rb") as f:
        data = f.read()
    out = []
    i = 0
    while i + 13 <= len(data) and data[i:i + 2] == b"PG":
        pts = struct.unpack_from(">I", data, i + 2)[0]
        stype = data[i + 10]
        slen = struct.unpack_from(">H", data, i + 11)[0]
        if stype == 0x16:
            out.append((pts, STATES.get(data[i + 20], "?")))
        i += 13 + slen
    return out


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2

    motor = pcs_states(argv[1])
    oracle = pcs_states(argv[2])
    by_pts_oracle = {pts: state for pts, state in oracle}

    mismatches = 0
    for pts, state in motor:
        ref = by_pts_oracle.get(pts, "—")
        mark = "ok" if state == ref else "MISMATCH"
        if state != ref:
            mismatches += 1
        print(f"  PTS {pts:>8}  motor={state:<11} oracle={ref:<11} {mark}")

    for pts in by_pts_oracle:
        if pts not in {p for p, _ in motor}:
            print(f"  PTS {pts:>8}  motor={'—':<11} oracle={by_pts_oracle[pts]:<11} MISMATCH")
            mismatches += 1

    verdict = "PASS" if mismatches == 0 else f"FAIL ({mismatches} mismatch)"
    print(f"{verdict}: {argv[1]} vs {argv[2]}")
    return 0 if mismatches == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
