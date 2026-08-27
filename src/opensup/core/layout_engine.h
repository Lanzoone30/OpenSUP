// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.
// Layout engine ported from brule (MIT License, cubicibo 2024-2025).

#pragma once

#include <cstdint>
#include <vector>
#include <tuple>

#include "opensup/common/geometry.h"

namespace opensup {
namespace core {

/// Layout engine for determining window splits (1 or 2 windows).
/// Ported from brule's LayoutEngine (_layouteng.cc).
/// Determines whether a group of events should use a single full-screen window
/// or be split into two windows (vertical or horizontal) to minimize total area.
class layout_engine_c {
public:
    explicit layout_engine_c(int width, int height);

    /// Add an event's alpha mask to the layout accumulator.
    /// @param x        X position of the event on screen
    /// @param y        Y position of the event on screen
    /// @param alpha    Alpha channel data (width * height bytes, 0-255)
    /// @param w        Width of the alpha mask
    /// @param h        Height of the alpha mask
    void add(int x, int y, const std::vector<uint8_t>& alpha, int w, int h);

    /// Get the computed layout.
    /// @return tuple (container, window0, window1, is_vertical)
    ///         If window0 == window1, the layout is single-window.
    ///         is_vertical: true = horizontal split (top/bottom), false = vertical split (left/right)
    [[nodiscard]] std::tuple<common::box_t, common::box_t, common::box_t, bool>
    get_layout() const;

    /// Reset the engine for a new epoch.
    void reset();

private:
    // Crop a window to its non-zero content, with optional directional padding.
    static common::box_t cut_vertical(const common::box_t& window,
                                       const std::vector<uint8_t>& screen,
                                       int screen_w, int screen_h);
    static common::box_t cut_horizontal(const common::box_t& window,
                                         const std::vector<uint8_t>& screen,
                                         int screen_w, int screen_h);

    // Brute-force all possible splits to find the one minimizing total area.
    void brute_force_windows();

    // Current container (bounding box of all non-zero pixels)
    common::box_t m_container{};
    // The two windows (identical if no split)
    common::box_t m_window0{};
    common::box_t m_window1{};
    // Accumulated alpha screen (RGBA-aware: only alpha > 0 counts)
    std::vector<uint8_t> m_screen;
    common::shape_t m_shape{};
    bool m_is_vertical = false;
    bool m_has_layout_changed = true;
    bool m_first_add = true;

    static constexpr int MARGIN = 8;
};

} // namespace core
} // namespace opensup