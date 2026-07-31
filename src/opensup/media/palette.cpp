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
#include "opensup/media/palette.h"
#include "opensup/common/color_matrix.h"

#include <cmath>
#include <algorithm>
#include <cstring>

namespace opensup {
namespace media {

using common::color_matrix_e;
using common::get_matrix;
using common::matrix_from_name;

uint8_t
clip_y(uint32_t v) noexcept
{
    if (v < 16) return 16;
    if (v > 235) return 235;
    return static_cast<uint8_t>(v);
}

uint8_t
clip_cbcr(uint32_t v) noexcept
{
    if (v < 16) return 16;
    if (v > 240) return 240;
    return static_cast<uint8_t>(v);
}

static uint8_t clip_rgba(int v) noexcept
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<uint8_t>(v);
}

palette_entry_t
palette_entry_t::to_rgba(std::string_view matrix) const
{
    auto mat_opt = matrix_from_name(matrix);
    auto mat = get_matrix(mat_opt.value_or(color_matrix_e::bt709));

    // YCbCr limited range → RGBA
    double yy  = static_cast<double>(y) - 16;
    double cb_ = static_cast<double>(cb) - 128;
    double cr_ = static_cast<double>(cr) - 128;

    auto& m = mat.y2r;
    int r = static_cast<int>(std::round(m[0][0] * yy + m[0][1] * cb_ + m[0][2] * cr_ + m[0][3]));
    int g = static_cast<int>(std::round(m[1][0] * yy + m[1][1] * cb_ + m[1][2] * cr_ + m[1][3]));
    int b = static_cast<int>(std::round(m[2][0] * yy + m[2][1] * cb_ + m[2][2] * cr_ + m[2][3]));
    int a = static_cast<int>(alpha);

    return palette_entry_t(clip_rgba(b), clip_rgba(g), clip_rgba(r), clip_rgba(a));
    // ponytail: BGR order from matrix math matches Python's RGBA namedtuple
}

palette_entry_t
palette_entry_t::from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                            std::string_view matrix)
{
    auto mat_opt = matrix_from_name(matrix);
    auto mat = get_matrix(mat_opt.value_or(color_matrix_e::bt709));

    auto& m = mat.r2y;
    double rd = r, gd = g, bd = b;
    auto y_  = static_cast<uint32_t>(std::round(m[0][0] * rd + m[0][1] * gd + m[0][2] * bd + m[0][3])) + 16;
    auto cb_ = static_cast<uint32_t>(std::round(m[1][0] * rd + m[1][1] * gd + m[1][2] * bd + m[1][3])) + 128;
    auto cr_ = static_cast<uint32_t>(std::round(m[2][0] * rd + m[2][1] * gd + m[2][2] * bd + m[2][3])) + 128;

    return palette_entry_t(clip_y(y_), clip_cbcr(cb_), clip_cbcr(cr_), a);
}

std::array<uint8_t, 4>
palette_entry_t::to_bytes() const noexcept
{
    // Internal: y=Y, cr=Cb, cb=Cr, alpha=A
    // PGS spec: [Y, Cr, Cb, Alpha]
    return {y, cb, cr, alpha};
}

palette_entry_t
palette_entry_t::from_bytes(const uint8_t* data)
{
    // PGS spec: [Y, Cr, Cb, Alpha]
    // Internal: y=Y, cr=Cb, cb=Cr
    return palette_entry_t(data[0], data[2], data[1], data[3]);
}

bool
palette_entry_t::operator==(const palette_entry_t& other) const noexcept
{
    return y == other.y && cr == other.cr && cb == other.cb && alpha == other.alpha;
}

palette_t::palette_t(const std::map<uint8_t, palette_entry_t>& entries)
    : m_entries(entries)
{
}

void
palette_t::set(uint8_t index, const palette_entry_t& entry)
{
    m_entries[index] = entry;
}

std::optional<palette_entry_t>
palette_t::get(uint8_t index) const
{
    auto it = m_entries.find(index);
    if (it != m_entries.end())
        return it->second;
    return std::nullopt;
}

bool
palette_t::has(uint8_t index) const noexcept
{
    return m_entries.find(index) != m_entries.end();
}

void
palette_t::remove(uint8_t index)
{
    m_entries.erase(index);
}

void
palette_t::offset(int delta)
{
    if (m_entries.empty()) return;
    auto max_idx = m_entries.rbegin()->first;
    auto min_idx = m_entries.begin()->first;
    if (static_cast<int>(max_idx) + delta < 256 &&
        static_cast<int>(min_idx) + delta >= 0) {
        std::map<uint8_t, palette_entry_t> shifted;
        for (auto& [k, v] : m_entries) {
            shifted[static_cast<uint8_t>(static_cast<int>(k) + delta)] = v;
        }
        m_entries = std::move(shifted);
    } else {
        throw std::invalid_argument("Shift outside 8-bit range");
    }
}

palette_t
palette_t::diff(const palette_t& other) const
{
    palette_t result;
    for (auto& [k, v] : other.m_entries) {
        auto it = m_entries.find(k);
        if (it == m_entries.end() || !(it->second == v))
            result.set(k, v);
    }
    return result;
}

std::vector<uint8_t>
palette_t::to_bytes() const
{
    std::vector<uint8_t> out;
    for (auto& [idx, entry] : m_entries) {
        out.push_back(idx);
        auto b = entry.to_bytes();
        out.insert(out.end(), b.begin(), b.end());
    }
    return out;
}

palette_t
palette_t::from_bytes(const uint8_t* data, size_t length)
{
    palette_t pal;
    for (size_t i = 0; i + 5 <= length; i += 5) {
        uint8_t idx = data[i];
        pal.set(idx, palette_entry_t::from_bytes(data + i + 1));
    }
    return pal;
}

bool
palette_t::operator==(const palette_t& other) const noexcept
{
    return m_entries == other.m_entries;
}

} // namespace media
} // namespace opensup
