#!/usr/bin/env python3
"""Generate CTU fixture: two spatially separated bands, top identical, bottom with noise delta=40.

Run from tests/fixtures/: python3 gen_ctu_fixture.py
Produces: ctu_bands_delta40.xml + ctu_band1.png + ctu_band2.png

Key: add transparent strip (alpha=0) between bands so layout_engine_c can
split them into two regions. Without it, alpha=255 everywhere → one region
→ flat SSIM instead of CTU area-weighted SSIM.

The split must pass the gain check: (area_w0 + area_w1) / area_region < 0.9
Layout engine includes part of gap in Window0, so we need larger gap.
With G=30, B=35: total=100, gap ratio=0.3
"""
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
W = 600
X, Y = 660, 480
BAND_H = 35      # Each band height
GAP_H = 30       # Transparent gap between bands (alpha=0)
TOTAL_H = BAND_H + GAP_H + BAND_H  # 100

# Deterministic noise for bottom band
np.random.seed(0xC7)  # fixed seed


def make_band_texture(w, h, base_gray, noise_amp=0):
    """Generate a band: flat gray + optional noise."""
    img = np.full((h, w), base_gray, dtype=np.uint8)
    if noise_amp > 0:
        noise = np.random.randint(-noise_amp, noise_amp + 1, size=(h, w), dtype=np.int16)
        img = np.clip(img.astype(np.int16) + noise, 0, 255).astype(np.uint8)
    return img


def save_rgba_png(rgb_arr, alpha_arr, name):
    """Save RGB array with separate alpha array as RGBA PNG."""
    h, w = rgb_arr.shape[:2]
    rgba = np.zeros((h, w, 4), dtype=np.uint8)
    rgba[..., 0] = rgb_arr[..., 0]
    rgba[..., 1] = rgb_arr[..., 1]
    rgba[..., 2] = rgb_arr[..., 2]
    rgba[..., 3] = alpha_arr
    img = Image.fromarray(rgba, 'RGBA')
    img.save(os.path.join(HERE, name))
    print(f"wrote {name} ({w}x{h}) mode={img.mode}")


def main():
    # Event 1: top band gray=128, gap (transparent), bottom band gray=128 + noise
    top1 = make_band_texture(W, BAND_H, 128, 0)
    gap = np.zeros((GAP_H, W), dtype=np.uint8)
    bot1 = make_band_texture(W, BAND_H, 128, 30)
    
    gray1 = np.vstack([top1, gap, bot1])
    rgb1 = np.repeat(gray1[..., None], 3, axis=2)
    
    alpha1 = np.full((TOTAL_H, W), 255, dtype=np.uint8)
    alpha1[BAND_H:BAND_H + GAP_H, :] = 0
    
    save_rgba_png(rgb1, alpha1, "ctu_band1.png")

    # Event 2: top band IDENTICAL (gray=128), gap (transparent), bottom band gray=168 + SAME noise pattern
    top2 = make_band_texture(W, BAND_H, 128, 0)
    gap2 = np.zeros((GAP_H, W), dtype=np.uint8)
    np.random.seed(0xC7)
    bot2 = make_band_texture(W, BAND_H, 168, 30)  # delta = 40 (168-128)
    
    gray2 = np.vstack([top2, gap2, bot2])
    rgb2 = np.repeat(gray2[..., None], 3, axis=2)
    
    alpha2 = np.full((TOTAL_H, W), 255, dtype=np.uint8)
    alpha2[BAND_H:BAND_H + GAP_H, :] = 0
    
    save_rgba_png(rgb2, alpha2, "ctu_band2.png")

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
      <Graphic Width="{W}" Height="{TOTAL_H}" X="{X}" Y="{Y}">ctu_band1.png</Graphic>
    </Event>
    <Event InTC="00:00:05:00" OutTC="00:00:09:00" Forced="False">
      <Graphic Width="{W}" Height="{TOTAL_H}" X="{X}" Y="{Y}">ctu_band2.png</Graphic>
    </Event>
  </Events>
</BDN>
'''
    xml_path = os.path.join(HERE, "ctu_bands_delta40.xml")
    with open(xml_path, "w") as f:
        f.write(xml)
    print(f"wrote ctu_bands_delta40.xml (Height={TOTAL_H})")


if __name__ == "__main__":
    main()
