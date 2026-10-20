// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.
// Layout engine ported from brule (MIT License, cubicibo 2024-2025).

#include "opensup/core/layout_engine.h"
#include "opensup/common/logger.h"
#include "opensup/media/pgraphics.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <utility>

namespace opensup {
namespace core {

using media::pg_decoder_t;

namespace {
double decode_base(std::initializer_list<int> areas, int container_area);
common::box_t pad_min8(const common::box_t& b, int cw, int ch);
int pad_vertical(common::box_t& b, bool upper, const common::box_t& region);
}

layout_engine_c::layout_engine_c(int width, int height)
    : m_screen(static_cast<size_t>(std::max(0, width)) * static_cast<size_t>(std::max(0, height)), 0)
    , m_shape{std::max(0, width), std::max(0, height)} {
    m_container = common::box_t{0, std::max(0, height), 0, std::max(0, width)};
}

void layout_engine_c::add(int x, int y, const std::vector<uint8_t>& alpha, int w, int h) {
    if (w <= 0 || h <= 0 || alpha.size() < static_cast<size_t>(w) * static_cast<size_t>(h)) return;

    // Accumulate alpha > 0 into the screen buffer
    for (int row = 0; row < h; ++row) {
        int screen_y = y + row;
        if (screen_y < 0 || screen_y >= m_shape.h) continue;
        int screen_x = x;
        if (screen_x < 0) screen_x = 0;
        int copy_w = std::min(w, m_shape.w - screen_x);
        if (copy_w <= 0) continue;

        size_t src_offset = static_cast<size_t>(row) * static_cast<size_t>(w);
        size_t dst_offset = static_cast<size_t>(screen_y) * static_cast<size_t>(m_shape.w) + static_cast<size_t>(screen_x);

        for (int col = 0; col < copy_w; ++col) {
            if (alpha[src_offset + static_cast<size_t>(col)] > 0) {
                m_screen[dst_offset + static_cast<size_t>(col)] = 1;
            }
        }
    }
    m_first_add = false;
    m_has_layout_changed = true;
}

std::tuple<common::box_t, common::box_t, common::box_t, bool>
layout_engine_c::get_layout() const {
    if (m_has_layout_changed) {
        const_cast<layout_engine_c*>(this)->brute_force_windows();
    }
    return std::make_tuple(m_container, m_window0, m_window1, m_is_vertical);
}

void layout_engine_c::reset() {
    std::fill(m_screen.begin(), m_screen.end(), 0);
    m_container = common::box_t{0, m_shape.h, 0, m_shape.w};
    m_window0 = m_container;
    m_window1 = m_container;
    m_is_vertical = false;
    m_has_layout_changed = true;
    m_first_add = true;
}

void layout_engine_c::brute_force_windows() {
    // First, find the current container (bounding box of non-zero pixels)
    common::box_t current_container = m_container;
    bool found = false;

    for (int y = 0; y < m_shape.h; ++y) {
        bool row_has_pixel = false;
        for (int x = 0; x < m_shape.w; ++x) {
            if (m_screen[static_cast<size_t>(y) * static_cast<size_t>(m_shape.w) + static_cast<size_t>(x)]) {
                row_has_pixel = true;
                break;
            }
        }
        if (row_has_pixel) {
            current_container.y = y;
            found = true;
            break;
        }
    }
    if (!found) {
        // No pixels - single window (full container)
        m_window0 = m_container;
        m_window1 = m_container;
        m_is_vertical = false;
        m_has_layout_changed = false;
        return;
    }

    for (int y = m_shape.h - 1; y >= current_container.y; --y) {
        bool row_has_pixel = false;
        for (int x = 0; x < m_shape.w; ++x) {
            if (m_screen[static_cast<size_t>(y) * static_cast<size_t>(m_shape.w) + static_cast<size_t>(x)]) {
                row_has_pixel = true;
                break;
            }
        }
        if (row_has_pixel) {
            current_container.dy = (y + 1) - current_container.y;
            break;
        }
    }

    int min_x = m_shape.w, max_x = 0;
    for (int y = current_container.y; y < current_container.y + current_container.dy; ++y) {
        for (int x = 0; x < m_shape.w; ++x) {
            if (m_screen[static_cast<size_t>(y) * static_cast<size_t>(m_shape.w) + static_cast<size_t>(x)]) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
            }
        }
    }
    const int cmin = min_x;
    const int cmax = max_x + 1;
    const int rmin = current_container.y;
    const int rmax = current_container.y + current_container.dy;

    // Raw container (brule LayoutEngine.find -> ocbox): content bounding box, grown so
    // each axis spans at least 8px. This is the single-window fallback SUPer uses when
    // a split is rejected.
    common::box_t ocbox;
    ocbox.x = cmin; ocbox.y = rmin; ocbox.dx = cmax - cmin; ocbox.dy = rmax - rmin;
    const int diffx = std::max(0, 8 - ocbox.dx);
    if (ocbox.x > diffx) ocbox.x = cmin - diffx;
    else if (diffx > 0) ocbox.dx = cmax + diffx - ocbox.x;
    const int diffy = std::max(0, 8 - ocbox.dy);
    if (ocbox.y > diffy) ocbox.y = rmin - diffy;
    else if (diffy > 0) ocbox.dy = rmax + diffy - ocbox.y;
    m_container = ocbox;

    // Split search region: content bbox padded by up to 7px, clamped to the screen.
    const int s_x0 = cmin - std::min(7, cmin);
    const int s_y0 = rmin - std::min(7, rmin);
    const int s_x1 = cmax + std::min(7, m_shape.w - cmax);
    const int s_y1 = rmax + std::min(7, m_shape.h - rmax);
    const common::box_t region{s_y0, s_y1 - s_y0, s_x0, s_x1 - s_x0};

    const int margin = MARGIN;
    uint32_t best_score = static_cast<uint32_t>(ocbox.area());
    common::box_t best0 = ocbox;
    common::box_t best1 = ocbox;
    bool has_split = false;
    bool best_is_vertical = false;

    // Cut along x -> left/right windows (brule scans this axis first, so ties favour it).
    for (int xk = s_x0 + margin; xk < s_x1 - margin; ++xk) {
        common::box_t b0 = region;
        common::box_t b1 = region;
        b0.dx = xk - s_x0;
        b1.x = xk;
        b1.dx = s_x1 - xk;

        b0 = cut_horizontal(b0, m_screen, m_shape.w, m_shape.h);
        b1 = cut_horizontal(b1, m_screen, m_shape.w, m_shape.h);

        const uint32_t surface = static_cast<uint32_t>(b0.area() + b1.area());
        if (surface < best_score) {
            best_score = surface;
            best0 = b0;
            best1 = b1;
            has_split = true;
            best_is_vertical = false;
        }
    }

    // Cut along y -> top/bottom windows.
    for (int yk = s_y0 + margin; yk < s_y1 - margin; ++yk) {
        common::box_t b0 = region;
        common::box_t b1 = region;
        b0.dy = yk - s_y0;
        b1.y = yk;
        b1.dy = s_y1 - yk;

        b0 = cut_vertical(b0, m_screen, m_shape.w, m_shape.h);
        b1 = cut_vertical(b1, m_screen, m_shape.w, m_shape.h);

        const uint32_t surface = static_cast<uint32_t>(b0.area() + b1.area());
        if (surface < best_score) {
            best_score = surface;
            best0 = b0;
            best1 = b1;
            has_split = true;
            best_is_vertical = true;
        }
    }

    if (!has_split) {
        m_window0 = ocbox;
        m_window1 = ocbox;
        m_is_vertical = false;
        m_has_layout_changed = false;
        return;
    }

    // PaddingEngine.directional_pad. Thin windows of a vertical split grow
    // toward the raw-region edges; leftover pixels pad inward (merge below).
    if (best_is_vertical) {
        const common::box_t raw0 = best0, raw1 = best1;
        int inter_margin = std::abs(raw0.y2() - raw1.y);
        inter_margin -= pad_vertical(best0, true, region);
        inter_margin -= pad_vertical(best1, false, region);
        if (inter_margin < 0) {
            const int y = std::min(raw0.y, raw1.y);
            const int x = std::min(raw0.x, raw1.x);
            best0 = common::box_t{y, std::max(raw0.y2(), raw1.y2()) - y,
                                  x, std::max(raw0.x2(), raw1.x2()) - x};
            best1 = best0;
            best_is_vertical = false;
        }
    } else {
        best0 = pad_min8(best0, m_shape.w, m_shape.h);
        best1 = pad_min8(best1, m_shape.w, m_shape.h);
    }

    // NORMAL preset orders the pair so the faster-to-decode window comes first.
    const int screen_area = m_shape.w * m_shape.h;
    const double score_ab = decode_base({best0.area(), best1.area()}, screen_area);
    const double score_ba = decode_base({best1.area(), best0.area()}, screen_area);
    const double score_split = std::min(score_ab, score_ba);
    if (score_ba < score_ab) std::swap(best0, best1);

    // Reject the split unless it saves more than ~10 ticks over the whole container: a
    // marginal cut only shaves a rounded corner yet pays for a second window (SUPer
    // _find_modify_layout is_bad_split).
    const double score_container = decode_base({ocbox.area()}, screen_area);
    if (score_split >= std::max(1.0 / pg_decoder_t::FREQ, score_container - 10.0 / pg_decoder_t::FREQ)) {
        m_window0 = ocbox;
        m_window1 = ocbox;
        m_is_vertical = false;
    } else {
        m_window0 = best0;
        m_window1 = best1;
        m_is_vertical = best_is_vertical;
    }
    m_has_layout_changed = false;
}

namespace {
// Effective decode duration of a 1- or 2-window layout (SUPer _decode_duration_base).
double decode_base(std::initializer_list<int> areas, int container_area) {
    double dd = std::ceil(pg_decoder_t::FREQ * container_area / pg_decoder_t::RC) / pg_decoder_t::FREQ;
    double t_dec = 0.0;
    for (int a : areas) {
        t_dec += std::ceil(pg_decoder_t::FREQ * a / pg_decoder_t::RD) / pg_decoder_t::FREQ;
        dd = std::max(t_dec, dd) + std::ceil(pg_decoder_t::FREQ * a / pg_decoder_t::RC) / pg_decoder_t::FREQ;
    }
    return dd;
}

// PaddingEngine.directional_pad vertical branch. A thin window first reaches
// the raw-region edge; a residual deficit pads inward and is returned so the
// caller can detect the merge case.
int pad_vertical(common::box_t& b, bool upper, const common::box_t& region) {
    const int diff_x = std::max(0, 8 - b.dx);
    const int left_pad = std::max(0, std::min(diff_x, b.x2() - region.x2() + diff_x));
    const int right_pad = diff_x - left_pad;

    const int diff_y = std::max(0, 8 - b.dy);
    int top_pad = 0, bot_pad = 0, missing = 0;
    if (diff_y > 0) {
        if (upper) {
            top_pad = b.y - region.y;
            missing = std::max(0, diff_y - top_pad);
            bot_pad = missing;
        } else {
            bot_pad = region.y2() - b.y2();
            missing = std::max(0, diff_y - bot_pad);
            top_pad = missing;
        }
    }
    b.x -= left_pad;
    b.dx += left_pad + right_pad;
    b.y -= top_pad;
    b.dy += top_pad + bot_pad;
    return missing;
}

// Grow a window to at least 8px per axis, keeping it inside the screen (SUPer
// PaddingEngine._pad_any_box with a full-screen container).
common::box_t pad_min8(const common::box_t& b, int cw, int ch) {
    const int min_d = 8;
    if (b.dx >= min_d && b.dy >= min_d) return b;

    const int diff_y = std::max(0, min_d - b.dy);
    const int diff_x = std::max(0, min_d - b.dx);
    const double dvy = static_cast<double>(diff_y) * (2.0 * b.y + (b.dy - ch)) / ch;
    const double dvx = static_cast<double>(diff_x) * (2.0 * b.x + (b.dx - cw)) / cw;

    auto minmax = [](int diff, double dv) {
        double lo = std::numeric_limits<double>::infinity();
        double hi = -std::numeric_limits<double>::infinity();
        for (int i = 0; i < diff; ++i) {
            const double v = -diff / 2.0 + dv + i;
            lo = std::min(lo, std::floor(v));
            hi = std::max(hi, std::ceil(v));
        }
        return std::pair<double, double>{lo, hi};
    };
    const auto [pu, pd] = minmax(diff_y, dvy);
    const auto [pl, pr] = minmax(diff_x, dvx);

    int new_x1 = std::max(0, static_cast<int>(b.x + (pl < pr ? pl : 0.0)));
    int new_x2 = std::min(cw, static_cast<int>(b.x + b.dx + (pl < pr ? pr : 0.0)));
    int new_y1 = std::max(0, static_cast<int>(b.y + (pu < pd ? pu : 0.0)));
    int new_y2 = std::min(ch, static_cast<int>(b.y + b.dy + (pu < pd ? pd : 0.0)));

    // Pad right/bottom first (rounding-friendly), else left/top.
    int missing = diff_x - ((b.x - new_x1) + (new_x2 - (b.x + b.dx)));
    if (missing > 0) {
        if (new_x2 + missing <= cw) new_x2 += missing; else new_x1 -= missing;
    }
    missing = diff_y - ((b.y - new_y1) + (new_y2 - (b.y + b.dy)));
    if (missing > 0) {
        if (new_y2 + missing <= ch) new_y2 += missing; else new_y1 -= missing;
    }

    common::box_t out;
    out.x = new_x1; out.y = new_y1; out.dx = new_x2 - new_x1; out.dy = new_y2 - new_y1;
    return out;
}

// Tighten a window to its non-zero content (bounding box), clamped to screen bounds.
// Matches brule's cut_vertical/cut_horizontal (_layouteng.cc): windows are tightened to
// content WITHOUT padding — brule pads only the container (pad_container, +7px), never the
// windows. Padding windows would swallow transparent rows and break region-edge parity.
common::box_t tighten_to_content(const common::box_t& window,
                                 const std::vector<uint8_t>& screen,
                                 int screen_w, int screen_h) {
    int x0 = window.x + window.dx;
    int y0 = window.y + window.dy;
    int x1 = window.x - 1;
    int y1 = window.y - 1;
    for (int y = window.y; y < window.y + window.dy; ++y) {
        if (y < 0 || y >= screen_h) continue;
        const size_t row_off = static_cast<size_t>(y) * static_cast<size_t>(screen_w);
        for (int x = window.x; x < window.x + window.dx; ++x) {
            if (x < 0 || x >= screen_w) continue;
            if (screen[row_off + static_cast<size_t>(x)]) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
        }
    }
    if (x0 > x1 || y0 > y1) {
        // No content in this window — keep it unchanged.
        return window;
    }
    return common::box_t{y0, y1 - y0 + 1, x0, x1 - x0 + 1};
}
}

common::box_t layout_engine_c::cut_vertical(const common::box_t& window,
                                             const std::vector<uint8_t>& screen,
                                             int screen_w, int screen_h) {
    if (window.dy <= 0 || window.dx <= 0) return window;
    return tighten_to_content(window, screen, screen_w, screen_h);
}

common::box_t layout_engine_c::cut_horizontal(const common::box_t& window,
                                               const std::vector<uint8_t>& screen,
                                               int screen_w, int screen_h) {
    if (window.dy <= 0 || window.dx <= 0) return window;
    return tighten_to_content(window, screen, screen_w, screen_h);
}

}
}