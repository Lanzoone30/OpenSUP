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

#include <cstdint>
#include <vector>

namespace opensup {
namespace common {

/// A point in image coordinates.
struct pos_t {
    int32_t x = 0;
    int32_t y = 0;
};

/// Width/height pair (no position).
struct shape_t {
    int32_t w = 0;
    int32_t h = 0;

    [[nodiscard]] constexpr int32_t area() const noexcept { return w * h; }
    [[nodiscard]] constexpr int32_t width() const noexcept { return w; }
    [[nodiscard]] constexpr int32_t height() const noexcept { return h; }

    /// Derive a shape from the extents of a box.
    static shape_t from_box(const struct box_t& box);
    /// Smallest shape that fully contains all given shapes.
    static shape_t union_shape(const std::vector<shape_t>& shapes);
};

/// Axis-aligned box defined by origin (x, y) and size (dx, dy).
struct box_t {
    int32_t y  = 0;
    int32_t dy = 0;
    int32_t x  = 0;
    int32_t dx = 0;

    [[nodiscard]] constexpr int32_t x2() const noexcept { return x + dx; }
    [[nodiscard]] constexpr int32_t y2() const noexcept { return y + dy; }
    [[nodiscard]] constexpr int32_t area() const noexcept { return dx * dy; }

    /// Size of this box as a shape.
    [[nodiscard]] shape_t dims() const noexcept;
    /// Top-left corner as a position.
    [[nodiscard]] pos_t origin() const noexcept;

    /// Fraction of this box's area covered by another box [0..1].
    [[nodiscard]] double overlap_with(const box_t& other) const noexcept;

    /// Box covering the region common to both inputs (empty if disjoint).
    static box_t intersect(const box_t& a, const box_t& b) noexcept;
    /// Smallest box fully containing both inputs.
    static box_t union_box(const box_t& a, const box_t& b) noexcept;
    static box_t from_coords(int32_t x1, int32_t y1, int32_t x2, int32_t y2) noexcept;
};

bool operator==(const pos_t& a, const pos_t& b) noexcept;
bool operator!=(const pos_t& a, const pos_t& b) noexcept;
bool operator==(const shape_t& a, const shape_t& b) noexcept;
bool operator!=(const shape_t& a, const shape_t& b) noexcept;
bool operator==(const box_t& a, const box_t& b) noexcept;
bool operator!=(const box_t& a, const box_t& b) noexcept;

} // namespace common
} // namespace opensup
