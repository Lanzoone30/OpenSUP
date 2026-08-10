// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.
//
// Adapted from the design of SUPer by cubicibo
// (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.
// Independently reimplemented in C++17; the original Python
// implementation is available in Referencias/SUPer-main/.

#pragma once

// ponytail: SSIM wrapper stub — full implementation requires OpenCV/SSIM-PIL dependency.
// Add when SSIM-based scene detection is needed in the encoder pipeline.
namespace opensup {
namespace common {

/// Structural similarity between two images in [0..1] (1 = identical).
class ssim_c {
public:
    static double compare(const uint8_t* img1, const uint8_t* img2,
                          int width, int height, int channels = 4);
};

} // namespace common
} // namespace opensup
