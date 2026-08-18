// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#pragma once

namespace opensup {
namespace common {

/// Structural similarity between two images in [0..1] (1 = identical).
class ssim_c {
public:
    /// Compare two RGBA images using luminance channel with alpha masking (SUPer-style).
    /// Both images must have same dimensions and 4 channels (RGBA).
    /// Returns SSIM score in [0,1] and cross_percentage (alpha overlap) via out parameter.
    static double compare_with_alpha(const uint8_t* img1, const uint8_t* img2,
                                     int width, int height,
                                     double& cross_percentage);

    /// Generic compare (all channels, no alpha masking) - kept for testing.
    static double compare(const uint8_t* img1, const uint8_t* img2,
                          int width, int height, int channels = 4);
};

} // namespace common
} // namespace opensup
