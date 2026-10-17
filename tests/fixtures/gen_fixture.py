#!/usr/bin/env python3
"""Generate the synthetic BDN XML + PNG fixtures for OpenSUP integration checks.

Run from tests/fixtures/:  python3 gen_fixture.py
Produces synth_bdn.xml, synth_similar.xml, 00001..00003.png, sim00001..sim00003.png
and large_wipe.png in this directory. (large_wipe.xml and the other fixture XMLs
are versioned; only the gitignored PNGs are emitted here.)
"""
import os
import sys
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


def generate_long_run():
    """Six-event single run for the extra_acq one-per-run check (SC-003).

    A wide gray fade keeps every event byte-reusable, so with a low acqrate
    (no drought growth) the whole sequence is one NORMAL run and the oracle
    inserts a single mid-run acquisition on the last event (+1 frame).
    """
    colors = [(20, 20, 20), (32, 32, 32), (44, 44, 44),
              (56, 56, 56), (68, 68, 68), (80, 80, 80)]
    for i, rgb in enumerate(colors, start=1):
        img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        try:
            font = ImageFont.truetype("DejaVuSans-Bold.ttf", 44)
        except OSError:
            font = ImageFont.load_default()
        d.rounded_rectangle([8, 8, W - 8, H - 8], radius=16, fill=(240, 200, 255, 230))
        d.text((30, 32), "Subtitle", fill=rgb + (255,), font=font)
        img.save(os.path.join(HERE, f"lr{i:05d}.png"))
        print(f"wrote lr{i:05d}.png (text color {rgb})")

    xml = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<BDN Version="0.9"\n'
        '     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"\n'
        '     xsi:noNamespaceSchemaLocation="BD-03-006-006-009.xsd">\n'
        '  <Description>\n'
        '    <Name Title="opensup-synth-long-run"/>\n'
        '    <Language Code="eng"/>\n'
        '    <Format VideoFormat="1080p" FrameRate="25" DropFrame="False"/>\n'
        '    <Events Count="6"/>\n'
        '  </Description>\n'
        '  <Events>\n'
    )
    for i in range(len(colors)):
        a, b = 1 + i * 4, 5 + i * 4
        xml += (
            f'    <Event InTC="00:00:{a:02d}:00" OutTC="00:00:{b:02d}:00" Forced="False">\n'
            f'      <Graphic Width="{W}" Height="{H}" X="{X}" Y="{Y}">lr{i + 1:05d}.png</Graphic>\n'
            '    </Event>\n'
        )
    xml += '  </Events>\n</BDN>\n'

    out = os.path.join(HERE, "synth_long_run.xml")
    with open(out, "w", encoding="utf-8") as f:
        f.write(xml)
    print(f"wrote {out}")


def generate_semi_alpha():
    """Single event with alpha==0, alpha==64 and alpha==255 blocks (R2).

    The alpha==0 block is green (non-black RGB) and sits inside the opaque
    frame so it survives the padding trim; the semi-transparent block is a
    distinct blue so the quantizer keeps it as its own entry.
    """
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rectangle([10, 10, W - 11, H - 11], fill=(200, 60, 60, 255))
    d.rectangle([70, 40, 229, 79], fill=(40, 120, 240, 64))
    d.rectangle([330, 40, 449, 79], fill=(60, 220, 60, 0))
    img.save(os.path.join(HERE, "semi_alpha.png"))
    print(f"wrote semi_alpha.png ({W}x{H})")

    xml = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<BDN Version="0.9"\n'
        '     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"\n'
        '     xsi:noNamespaceSchemaLocation="BD-03-006-006-009.xsd">\n'
        '  <Description>\n'
        '    <Name Title="opensup-semi-alpha"/>\n'
        '    <Language Code="eng"/>\n'
        '    <Format VideoFormat="1080p" FrameRate="25" DropFrame="False"/>\n'
        '    <Events Count="1"/>\n'
        '  </Description>\n'
        '  <Events>\n'
        '    <Event InTC="00:00:01:00" OutTC="00:00:03:00" Forced="False">\n'
        f'      <Graphic Width="{W}" Height="{H}" X="{X}" Y="{Y}">semi_alpha.png</Graphic>\n'
        '    </Event>\n'
        '  </Events>\n'
        "</BDN>\n"
    )
    out = os.path.join(HERE, "semi_alpha.xml")
    with open(out, "w", encoding="utf-8") as f:
        f.write(xml)
    print(f"wrote {out}")


def generate_rgba_collision():
    """Single event, 6 opaque color blocks (H1: RGBA vs YCrCbA solver domain).

    Blocks 0/1 collide to the same bt709 YCrCbA after palette_entry_t::from_rgba
    rounding (11,21,30) and (11,22,31) -> Y=33 Cr=133 Cb=123. Block 2 (10,20,30)
    is a near collision (dY=1); 3-5 are contrast colors so the palette is not
    trivial. Run: python3 gen_fixture.py rgba_collision
    """
    blocks = [
        (11, 21, 30),    # A1  -> Y=33 Cr=133 Cb=123
        (11, 22, 31),    # A2  -> Y=33 Cr=133 Cb=123 (same YCrCbA as A1)
        (10, 20, 30),    # B1  -> Y=32 Cr=133 Cb=123 (near twin, dY=1)
        (200, 60, 40),
        (40, 120, 200),
        (240, 240, 240),
    ]
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    bw = W // len(blocks)
    for i, rgb in enumerate(blocks):
        x0 = i * bw
        d.rectangle([x0, 0, x0 + bw - 1 if i < len(blocks) - 1 else W - 1, H - 1],
                    fill=rgb + (255,))
    img.save(os.path.join(HERE, "rgba_collision.png"))
    print(f"wrote rgba_collision.png ({W}x{H}, {len(blocks)} blocks)")

    xml = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<BDN Version="0.9"\n'
        '     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"\n'
        '     xsi:noNamespaceSchemaLocation="BD-03-006-006-009.xsd">\n'
        '  <Description>\n'
        '    <Name Title="opensup-rgba-collision"/>\n'
        '    <Language Code="eng"/>\n'
        '    <Format VideoFormat="1080p" FrameRate="25" DropFrame="False"/>\n'
        '    <Events Count="1"/>\n'
        '  </Description>\n'
        '  <Events>\n'
        '    <Event InTC="00:00:01:00" OutTC="00:00:03:00" Forced="False">\n'
        f'      <Graphic Width="{W}" Height="{H}" X="{X}" Y="{Y}">rgba_collision.png</Graphic>\n'
        '    </Event>\n'
        '  </Events>\n'
        "</BDN>\n"
    )
    out = os.path.join(HERE, "rgba_collision.xml")
    with open(out, "w", encoding="utf-8") as f:
        f.write(xml)
    print(f"wrote {out}")


def generate_large_wipe():
    """740x740 window for the FR-006 palette-wipe path.

    At 60fps the frame budget is ~531K px, so this window is too large to redraw
    within a frame; a clear therefore swaps to a palette wipe. The pixels only
    need to be a real opaque graphic (the clear type is driven by the window
    size, not the content). large_wipe.xml (versioned) references this file.
    """
    size, half = 740, 370
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    quads = [(0, 0, (220, 60, 60)), (half, 0, (220, 220, 60)),
             (0, half, (220, 60, 220)), (half, half, (230, 230, 230))]
    for x, y, rgb in quads:
        d.rectangle([x, y, x + half - 1, y + half - 1], fill=rgb + (255,))
    d.rectangle([0, 0, size - 1, size - 1], outline=(40, 40, 40, 255), width=40)
    img.save(os.path.join(HERE, "large_wipe.png"))
    print("wrote large_wipe.png (740x740)")


def generate_flags_parity():
    """Three-event fixture for boolean-flags parity.

    Event 1 forces a 2-window layout (two opaque regions with a wide gap).
    Event 2 starts a new palette id. Event 3 reuses that palette id so the
    encoder must emit a delta PDS.
    """
    w2, h2 = W, H
    img = Image.new("RGBA", (w2, h2), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # Two compact opaque blocks far apart so the epoch layout keeps two windows.
    # Each block is >8 px on both axes and the gap is >8 px.
    left_block = (10, 80, 20, 100)
    right_block = (520, 590, 20, 100)
    d.rectangle([left_block[0], left_block[2], left_block[1], left_block[3]],
                fill=(220, 60, 60, 255))
    d.rectangle([right_block[0], right_block[2], right_block[1], right_block[3]],
                fill=(60, 220, 120, 255))
    img.save(os.path.join(HERE, "flags_parity_2win.png"))
    print(f"wrote flags_parity_2win.png ({w2}x{h2})")

    # Single-window frames for palette redefinition/delta tests.
    # They reuse the left block so the epoch does not merge back to one window.
    for name, color in (
        ("flags_parity_swin.png", (80, 120, 220, 255)),
        ("flags_parity_swin2.png", (220, 180, 40, 255)),
    ):
        img = Image.new("RGBA", (w2, h2), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        d.rectangle([left_block[0], left_block[2], left_block[1], left_block[3]],
                    fill=color)
        img.save(os.path.join(HERE, name))
        print(f"wrote {name} ({w2}x{h2})")

    xml = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<BDN Version="0.9"\n'
        '     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"\n'
        '     xsi:noNamespaceSchemaLocation="BD-03-006-006-009.xsd">\n'
        '  <Description>\n'
        '    <Name Title="opensup-flags-parity"/>\n'
        '    <Language Code="eng"/>\n'
        '    <Format VideoFormat="1080p" FrameRate="25" DropFrame="False"/>\n'
        '    <Events Count="3"/>\n'
        '  </Description>\n'
        '  <Events>\n'
        '    <Event InTC="00:00:01:00" OutTC="00:00:03:00" Forced="False">\n'
        f'      <Graphic Width="{w2}" Height="{h2}" X="{X}" Y="{Y}">flags_parity_2win.png</Graphic>\n'
        '    </Event>\n'
        '    <Event InTC="00:00:03:00" OutTC="00:00:05:00" Forced="False">\n'
        f'      <Graphic Width="{w2}" Height="{h2}" X="{X}" Y="{Y}">flags_parity_swin.png</Graphic>\n'
        '    </Event>\n'
        '    <Event InTC="00:00:05:00" OutTC="00:00:07:00" Forced="False">\n'
        f'      <Graphic Width="{w2}" Height="{h2}" X="{X}" Y="{Y}">flags_parity_swin2.png</Graphic>\n'
        '    </Event>\n'
        '  </Events>\n'
        '</BDN>\n'
    )
    out = os.path.join(HERE, "flags_parity.xml")
    with open(out, "w", encoding="utf-8") as f:
        f.write(xml)
    print(f"wrote {out}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "rgba_collision":
        generate_rgba_collision()
    elif len(sys.argv) > 1 and sys.argv[1] == "flags_parity":
        generate_flags_parity()
    else:
        main()
        generate_similar_series()
        generate_long_run()
        generate_semi_alpha()
        generate_rgba_collision()
        generate_large_wipe()