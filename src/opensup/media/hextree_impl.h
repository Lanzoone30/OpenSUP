// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Port of brule's HexTree quantizer (MIT, (c) 2024 cubicibo).

// The Python/numpy binding was removed; the C++ core is unchanged.

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace opensup {
namespace media {

/// Quantize RGBA pixels with brule's HexTree algorithm (Atkinson dithering).
/// @param rgba     flat RGBA byte array (4 bytes per pixel)
/// @param len      pixel count
/// @param width    image width in pixels (> 0 enables dithering)
/// @param max_colors max palette entries (16..256)
/// @return true on success; fills palette (RGBA bytes, `pal_count` entries)
///         and indexed (palette index per pixel).
bool hextree_quantize_core(const uint8_t* rgba, size_t len, uint32_t width,
                           int max_colors,
                           std::vector<uint8_t>& palette_out,
                           std::vector<uint8_t>& indexed_out,
                           int& pal_count_out);

} // namespace media
} // namespace opensup
