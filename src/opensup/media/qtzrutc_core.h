// SPDX-License-Identifier: GPL-3.0-or-later
// MIT: original algorithm (c) 2025 cubicibo.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace opensup {
namespace media {

/// RGBA -> palette + one index per pixel. width 0 disables dithering;
/// max_colors is capped at 255 (0xFF stays free for the caller). All state
/// is per call, so concurrent calls are safe.
bool qtzrutc_quantize_core(const uint8_t* rgba, size_t len, uint32_t width,
                           int max_colors,
                           std::vector<uint8_t>& palette_out,
                           std::vector<uint8_t>& indexed_out,
                           int& pal_count_out);

}
}
