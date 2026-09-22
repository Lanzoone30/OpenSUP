// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.
// Layout engine ported from brule (MIT License, cubicibo 2024-2025).

#include "opensup/core/layout_engine.h"
#include "opensup/common/logger.h"

#include <algorithm>
#include <cstring>

namespace opensup {
namespace core {

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

    // Find top
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

    // Find bottom
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

    // Find left/right within y-range
    int min_x = m_shape.w, max_x = 0;
    for (int y = current_container.y; y < current_container.y + current_container.dy; ++y) {
        for (int x = 0; x < m_shape.w; ++x) {
            if (m_screen[static_cast<size_t>(y) * static_cast<size_t>(m_shape.w) + static_cast<size_t>(x)]) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
            }
        }
    }
    current_container.x = min_x;
    current_container.dx = (max_x + 1) - current_container.x;

    // brule parity (brute_force_windows @ _layouteng.cc:228/267): a split only
    // pays if the tightened window pair is smaller than the EXACT content
    // bounding box. Comparing against the margin-inflated container makes any
    // solid single blob look "worth splitting" (it saves the 8px margin).
    const uint32_t exact_area = static_cast<uint32_t>(current_container.area());

    // Ensure minimum margin
    const int margin = MARGIN;
    if (current_container.x > margin) current_container.x -= margin;
    else current_container.x = 0;
    if (current_container.x + current_container.dx + margin < m_shape.w) current_container.dx += margin;
    else current_container.dx = m_shape.w - current_container.x;
    if (current_container.y > margin) current_container.y -= margin;
    else current_container.y = 0;
    if (current_container.y + current_container.dy + margin < m_shape.h) current_container.dy += margin;
    else current_container.dy = m_shape.h - current_container.y;

    m_container = current_container;

    // Now brute-force all possible splits
    uint32_t best_score = exact_area;
    common::box_t best0 = m_container;
    common::box_t best1 = m_container;
    bool best_is_vertical = false;

    // Try horizontal splits (cut along y - top/bottom windows)
    for (int yk = current_container.y + margin; yk <= current_container.y + current_container.dy - margin; ++yk) {
        common::box_t eval0 = current_container;
        common::box_t eval1 = current_container;
        eval0.dy = yk - eval0.y;
        eval1.y = yk;
        eval1.dy = current_container.y + current_container.dy - yk;

        // Cut vertical edges (left/right) for each window
        eval0 = cut_vertical(eval0, m_screen, m_shape.w, m_shape.h);
        eval1 = cut_vertical(eval1, m_screen, m_shape.w, m_shape.h);

        uint32_t surface = static_cast<uint32_t>(eval0.area() + eval1.area());
        if (surface < best_score) {
            best_score = surface;
            best0 = eval0;
            best1 = eval1;
            best_is_vertical = true;
        }
    }

    // Try vertical splits (cut along x - left/right windows)
    for (int xk = current_container.x + margin; xk <= current_container.x + current_container.dx - margin; ++xk) {
        common::box_t eval0 = current_container;
        common::box_t eval1 = current_container;
        eval0.dx = xk - eval0.x;
        eval1.x = xk;
        eval1.dx = current_container.x + current_container.dx - xk;

        // Cut horizontal edges (top/bottom) for each window
        eval0 = cut_horizontal(eval0, m_screen, m_shape.w, m_shape.h);
        eval1 = cut_horizontal(eval1, m_screen, m_shape.w, m_shape.h);

        uint32_t surface = static_cast<uint32_t>(eval0.area() + eval1.area());
        if (surface < best_score) {
            best_score = surface;
            best0 = eval0;
            best1 = eval1;
            best_is_vertical = false;
        }
    }

    if (best_score >= exact_area) {
        // No split worthwhile
        m_window0 = m_container;
        m_window1 = m_container;
        m_is_vertical = false;
    } else {
        m_window0 = best0;
        m_window1 = best1;
        m_is_vertical = best_is_vertical;
    }
    m_has_layout_changed = false;
}

namespace {
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
        // No content in this window — keep it unchanged (matches the reference fallback).
        return window;
    }
    return common::box_t{y0, y1 - y0 + 1, x0, x1 - x0 + 1};
}
}  // namespace

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

} // namespace core
} // namespace opensup