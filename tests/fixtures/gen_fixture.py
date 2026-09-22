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


def generate_similar_series():
    """Same-size events, subtle text-color fade (SSIM NORMAL chain).

    Background box keeps the trimmed bbox identical; the text color shifts a
    few gray levels per event (like a fade), so each event fuses against the
    accumulated composite (no drift) and stays NORMAL. With extra_acq=1, the
    counter builds up and a mid-event acquisition is inserted.
    """
    colors = [(20, 20, 20), (32, 32, 32), (44, 44, 44)]
    for i, rgb in enumerate(colors, start=1):
        fname = f"sim{i:05d}.png"
        img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        try:
            font = ImageFont.truetype("DejaVuSans-Bold.ttf", 44)
        except OSError:
            font = ImageFont.load_default()
        # Draw background box so trimmed bbox is the box (same for all events)
        d.rounded_rectangle([8, 8, W - 8, H - 8], radius=16, fill=(240, 200, 255, 230))
        d.text((30, 32), "Subtitle", fill=rgb + (255,), font=font)
        img.save(os.path.join(HERE, fname))
        print(f"wrote {fname} (text color {rgb})")

    xml = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<BDN Version="0.9"\n'
        '     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"\n'
        '     xsi:noNamespaceSchemaLocation="BD-03-006-006-009.xsd">\n'
        '  <Description>\n'
        '    <Name Title="opensup-synth-similar"/>\n'
        '    <Language Code="eng"/>\n'
        '    <Format VideoFormat="1080p" FrameRate="25" DropFrame="False"/>\n'
        '    <Events Count="3"/>\n'
        '  </Description>\n'
        '  <Events>\n'
        '    <Event InTC="00:00:01:00" OutTC="00:00:05:00" Forced="False">\n'
        '      <Graphic Width="600" Height="120" X="660" Y="480">sim00001.png</Graphic>\n'
        '    </Event>\n'
        '    <Event InTC="00:00:05:00" OutTC="00:00:09:00" Forced="False">\n'
        '      <Graphic Width="600" Height="120" X="660" Y="480">sim00002.png</Graphic>\n'
        '    </Event>\n'
        '    <Event InTC="00:00:09:00" OutTC="00:00:13:00" Forced="False">\n'
        '      <Graphic Width="600" Height="120" X="660" Y="480">sim00003.png</Graphic>\n'
        '    </Event>\n'
        '  </Events>\n'
        "</BDN>\n"
    )
    out = os.path.join(HERE, "synth_similar.xml")
    with open(out, "w", encoding="utf-8") as f:
        f.write(xml)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
    generate_similar_series()