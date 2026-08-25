#!/usr/bin/env python3
"""Generate CTU fixture: two spatially separated bands, top identical, bottom with noise delta=40.

Run from tests/fixtures/: python3 gen_ctu_fixture.py
Produces: ctu_bands_delta40.xml + ctu_band1.png + ctu_band2.png
"""
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 600, 120  # Same as synth_ssimband
X, Y = 660, 480
BAND_H = H // 2  # Two equal bands: top 60px, bottom 60px

# Deterministic noise for bottom band
np.random.seed(0xC7)  # fixed seed

def make_band_texture(w, h, base_gray, noise_amp=0):
    """Generate a band: flat gray + optional noise."""
    img = np.full((h, w), base_gray, dtype=np.uint8)
    if noise_amp > 0:
        noise = np.random.randint(-noise_amp, noise_amp + 1, size=(h, w), dtype=np.int16)
        img = np.clip(img.astype(np.int16) + noise, 0, 255).astype(np.uint8)
    return img

def save_rgba(arr, name):
    """Save grayscale array as RGBA PNG with full opacity."""
    h, w = arr.shape
    rgba = np.zeros((h, w, 4), dtype=np.uint8)
    rgba[..., 0] = arr
    rgba[..., 1] = arr
    rgba[..., 2] = arr
    rgba[..., 3] = 255
    Image.fromarray(rgba, 'RGBA').save(os.path.join(HERE, name))
    print(f"wrote {name} ({w}x{h})")

def main():
    # Event 1: top band gray=128, bottom band gray=128 + noise
    top1 = make_band_texture(W, BAND_H, 128, 0)
    bot1 = make_band_texture(W, BAND_H, 128, 30)  # noise amplitude 30
    img1 = np.vstack([top1, bot1])
    save_rgba(img1, "ctu_band1.png")

    # Event 2: top band IDENTICAL (gray=128), bottom band gray=168 + SAME noise pattern (delta=40)
    top2 = make_band_texture(W, BAND_H, 128, 0)
    bot2 = make_band_texture(W, BAND_H, 168, 30)  # same noise, shifted by +40
    img2 = np.vstack([top2, bot2])
    save_rgba(img2, "ctu_band2.png")

    # XML with 2 events
    xml = f'''<?xml version="1.0" encoding="UTF-8"?>
<BDN Version="0.9"
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:noNamespaceSchemaLocation="BD-03-006-006-009.xsd">
  <Description>
    <Name Title="opensup-ctu-separated-bands-delta40"/>
    <Language Code="eng"/>
    <Format VideoFormat="1080p" FrameRate="25" DropFrame="False"/>
    <Events Count="2"/>
  </Description>
  <Events>
    <Event InTC="00:00:01:00" OutTC="00:00:05:00" Forced="False">
      <Graphic Width="{W}" Height="{H}" X="{X}" Y="{Y}">ctu_band1.png</Graphic>
    </Event>
    <Event InTC="00:00:05:00" OutTC="00:00:09:00" Forced="False">
      <Graphic Width="{W}" Height="{H}" X="{X}" Y="{Y}">ctu_band2.png</Graphic>
    </Event>
  </Events>
</BDN>
'''
    with open(os.path.join(HERE, "ctu_bands_delta40.xml"), "w") as f:
        f.write(xml)
    print("wrote ctu_bands_delta40.xml")

if __name__ == "__main__":
    main()