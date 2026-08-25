// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.
// CTU: recursive area-weighted SSIM (render2.py:1186-1245).

#include "opensup/core/ctu.h"

#include <algorithm>

#include "opensup/common/ssim.h"
#include "opensup/core/layout_engine.h"

namespace opensup {
namespace core {
namespace {

constexpr int CTU_MAX_DEPTH = 4;
constexpr double IDENTICAL_AREA_COEFF = 0.325;
constexpr int MIN_REGION_SIZE = 8;

bool same_box(const common::box_t& a, const common::box_t& b) {
    return a.x == b.x && a.y == b.y && a.dx == b.dx && a.dy == b.dy;
}

}  // namespace

std::vector<uint8_t> ctu_c::crop(const std::vector<uint8_t>& rgba, int w, int h,
                                 const common::box_t& box) {
    const int x0 = std::max(0, box.x);
    const int y0 = std::max(0, box.y);
    const int x1 = std::min(w, box.x2());
    const int y1 = std::min(h, box.y2());
    if (x1 <= x0 || y1 <= y0) return {};

    const int crop_w = x1 - x0;
    const int crop_h = y1 - y0;
    const size_t out_size = static_cast<size_t>(crop_w) * static_cast<size_t>(crop_h) * 4u;
    std::vector<uint8_t> out(out_size);
    for (int y = y0; y < y1; ++y) {
        const size_t src_off = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x0)) * 4;
        const size_t dst_off = (static_cast<size_t>(y - y0) * static_cast<size_t>(crop_w)) * 4;
        std::copy_n(rgba.begin() + static_cast<ptrdiff_t>(src_off),
                    static_cast<size_t>(crop_w) * 4,
                    out.begin() + static_cast<ptrdiff_t>(dst_off));
    }
    return out;
}

void ctu_c::get_costs(const std::vector<uint8_t>& composite,
                      const std::vector<uint8_t>& frame,
                      int w, int h, int depth, std::vector<cost_t>& out) {
    bool split_valid = depth < CTU_MAX_DEPTH;
    common::box_t w0{}, w1{};

    if (split_valid) {
        layout_engine_c eng(w, h);
        const size_t sw = static_cast<size_t>(w);
        const size_t sh = static_cast<size_t>(h);
        std::vector<uint8_t> alpha_comp(sw * sh);
        std::vector<uint8_t> alpha_frame(sw * sh);
        for (size_t i = 0; i < alpha_comp.size(); ++i) {
            alpha_comp[i] = composite[i * 4 + 3];
            alpha_frame[i] = frame[i * 4 + 3];
        }
        eng.add(0, 0, alpha_comp, w, h);
        eng.add(0, 0, alpha_frame, w, h);

        common::box_t cbox{};
        bool is_vertical = false;
        std::tie(cbox, w0, w1, is_vertical) = eng.get_layout();
        (void)cbox;
        (void)is_vertical;
        split_valid = !same_box(w0, w1);
    }

    if (split_valid) {
        // ponytail: raw window boxes — SUPer first pads to >=8px via
        // PaddingEngine.directional_pad; add it if region-edge parity matters.
        const bool sizes_ok = w0.dx >= MIN_REGION_SIZE && w0.dy >= MIN_REGION_SIZE &&
                              w1.dx >= MIN_REGION_SIZE && w1.dy >= MIN_REGION_SIZE;
        const bool gain_ok = static_cast<double>(w0.area() + w1.area()) <
                             static_cast<double>(w) * h * (0.9 - depth / 13.0);
        if (sizes_ok && gain_ok) {
            get_costs(crop(composite, w, h, w0), crop(frame, w, h, w0),
                      w0.dx, w0.dy, depth + 1, out);
            get_costs(crop(composite, w, h, w1), crop(frame, w, h, w1),
                      w1.dx, w1.dy, depth + 1, out);
            return;
        }
    }

    double cross = 0.0;
    const double score = common::ssim_c::compare_with_alpha(
        composite.data(), frame.data(), w, h, cross);
    const bool identical = (score == 1.0 && cross == 1.0);
    const int64_t area = static_cast<int64_t>(w) * static_cast<int64_t>(h);
    out.push_back({score, cross, identical
        ? static_cast<int64_t>(static_cast<double>(area) * IDENTICAL_AREA_COEFF) : area});
}

std::pair<double, double> ctu_c::evaluate(const std::vector<uint8_t>& composite,
                                          const std::vector<uint8_t>& frame,
                                          int w, int h) {
    std::vector<cost_t> costs;
    get_costs(composite, frame, w, h, 0, costs);

    int64_t sum_area = 0;
    for (const auto& c : costs) sum_area += c.area;
    if (sum_area == 0) return {1.0, 1.0};

    double score = 0.0, cross = 0.0;
    for (const auto& c : costs) {
        score += static_cast<double>(c.area) * c.score;
        cross += static_cast<double>(c.area) * c.cross;
    }
    return {score / static_cast<double>(sum_area), cross / static_cast<double>(sum_area)};
}

}  // namespace core
}  // namespace opensup
