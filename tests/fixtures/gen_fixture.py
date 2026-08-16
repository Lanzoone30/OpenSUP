#!/usr/bin/env python3
"""Generate a synthetic BDN XML + PNG pair for OpenSUP integration checks.

Run from tests/fixtures/:  python3 gen_fixture.py
Produces synth_bdn.xml and 00001.png..00003.png in this directory.
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 600, 120
X, Y = 660, 480
EVENTS = [
    ("00:00:01:00", "00:00:03:00", "00001.png", (240, 200, 255), "First subtitle"),
    ("00:00:03:12", "00:00:05:00", "00002.png", (255, 220, 120), "Second subtitle"),
    ("00:00:05:00", "00:00:07:10", "00003.png", (160, 255, 180), "Third subtitle"),
]


def make_png(name, rgb, text):
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([8, 8, W - 8, H - 8], radius=16, fill=rgb + (230,))
    try:
        font = ImageFont.truetype("DejaVuSans-Bold.ttf", 44)
    except OSError:
        font = ImageFont.load_default()
    d.text((30, 32), text, fill=(20, 20, 20, 255), font=font)
    img.save(os.path.join(HERE, name))
    print(f"wrote {name} ({W}x{H})")


def main():
    for i, (tc_in, tc_out, fname, rgb, text) in enumerate(EVENTS, start=1):
        make_png(fname, rgb, text)

    xml = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<BDN Version="0.9"\n'
        '     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"\n'
        '     xsi:noNamespaceSchemaLocation="BD-03-006-006-009.xsd">\n'
        '  <Description>\n'
        '    <Name Title="opensup-synth-fixture"/>\n'
        '    <Language Code="eng"/>\n'
        '    <Format VideoFormat="1080p" FrameRate="25" DropFrame="False"/>\n'
        f'    <Events Count="{len(EVENTS)}"/>\n'
        '  </Description>\n'
        '  <Events>\n'
    )
    for tc_in, tc_out, fname, _rgb, _text in EVENTS:
        xml += (
            f'    <Event InTC="{tc_in}" OutTC="{tc_out}" Forced="False">\n'
            f'      <Graphic Width="{W}" Height="{H}" X="{X}" Y="{Y}">{fname}</Graphic>\n'
            '    </Event>\n'
        )
    xml += '  </Events>\n</BDN>\n'

    out = os.path.join(HERE, "synth_bdn.xml")
    with open(out, "w", encoding="utf-8") as f:
        f.write(xml)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()