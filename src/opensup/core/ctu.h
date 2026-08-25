// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "opensup/common/geometry.h"

namespace opensup {
namespace core {

/// Recursive area-weighted SSIM comparison (SUPer render2.py:1186-1245, CTU).
/// Splits spatially separated alpha regions with the layout engine and
/// aggregates per-region scores, discounting identical regions by 0.325 so a
/// diverging region is not diluted by unchanged ones.
class ctu_c {
public:
    /// Compare an accumulated composite against the current event bitmap.
    /// @param composite  RGBA buffer (w * h * 4), accumulated composite
    /// @param frame      RGBA buffer (w * h * 4), current event bitmap
    /// @return (score, cross_percentage), area-weighted over split regions
    [[nodiscard]] static std::pair<double, double> evaluate(
        const std::vector<uint8_t>& composite,
        const std::vector<uint8_t>& frame, int w, int h);

private:
    struct cost_t {
        double score;
        double cross;
        int64_t area;
    };

    static void get_costs(const std::vector<uint8_t>& composite,
                          const std::vector<uint8_t>& frame,
                          int w, int h, int depth, std::vector<cost_t>& out);

    static std::vector<uint8_t> crop(const std::vector<uint8_t>& rgba,
                                     int w, int h, const common::box_t& box);
};

}  // namespace core
}  // namespace opensup
