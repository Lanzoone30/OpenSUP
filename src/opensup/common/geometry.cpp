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

#include "opensup/pch.h"
#include "opensup/common/geometry.h"

#include <algorithm>

namespace opensup {
namespace common {

shape_t
shape_t::from_box(const box_t& box)
{
    return {box.dx, box.dy};
}

shape_t
shape_t::union_shape(const std::vector<shape_t>& shapes)
{
    int32_t w = 0, h = 0;
    for (const auto& s : shapes) {
        w = std::max(w, s.w);
        h = std::max(h, s.h);
    }
    return {w, h};
}

shape_t
box_t::dims() const noexcept
{
    return {dx, dy};
}

pos_t
box_t::origin() const noexcept
{
    return {x, y};
}

double
box_t::overlap_with(const box_t& other) const noexcept
{
    auto isect = intersect(*this, other);
    int32_t min_area = std::min(area(), other.area());
    if (min_area == 0) return 0.0;
    return static_cast<double>(isect.area()) / static_cast<double>(min_area);
}

box_t
box_t::intersect(const box_t& a, const box_t& b) noexcept
{
    int32_t x2_a = a.x2(), y2_a = a.y2();
    int32_t x2_b = b.x2(), y2_b = b.y2();

    int32_t nx = std::max(a.x, b.x);
    int32_t ny = std::max(a.y, b.y);
    int32_t ndx = std::max(0, std::min(x2_a, x2_b) - nx);
    int32_t ndy = std::max(0, std::min(y2_a, y2_b) - ny);

    return {ny, ndy, nx, ndx};
}

box_t
box_t::union_box(const box_t& a, const box_t& b) noexcept
{
    int32_t nx = std::min(a.x, b.x);
    int32_t ny = std::min(a.y, b.y);
    int32_t ndx = std::max(a.x2(), b.x2()) - nx;
    int32_t ndy = std::max(a.y2(), b.y2()) - ny;

    return {ny, ndy, nx, ndx};
}

box_t
box_t::from_coords(int32_t x1, int32_t y1, int32_t x2, int32_t y2) noexcept
{
    int32_t nx = std::min(x1, x2);
    int32_t ny = std::min(y1, y2);
    int32_t ndx = std::abs(x2 - x1);
    int32_t ndy = std::abs(y2 - y1);
    return {ny, ndy, nx, ndx};
}

bool operator==(const pos_t& a, const pos_t& b) noexcept
{
    return a.x == b.x && a.y == b.y;
}

bool operator!=(const pos_t& a, const pos_t& b) noexcept
{
    return !(a == b);
}

bool operator==(const shape_t& a, const shape_t& b) noexcept
{
    return a.w == b.w && a.h == b.h;
}

bool operator!=(const shape_t& a, const shape_t& b) noexcept
{
    return !(a == b);
}

bool operator==(const box_t& a, const box_t& b) noexcept
{
    return a.x == b.x && a.y == b.y && a.dx == b.dx && a.dy == b.dy;
}

bool operator!=(const box_t& a, const box_t& b) noexcept
{
    return !(a == b);
}

} // namespace common
} // namespace opensup
