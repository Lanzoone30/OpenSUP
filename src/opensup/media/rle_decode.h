// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace opensup {
namespace media {

/// Decode a PGS RLE object payload (US 7912305 B1, same format as encode_rle)
/// into a flat indexed bitmap of width*height bytes.
///
/// @param data   RLE byte stream (an ODS object payload).
/// @param width  expected line width; lines are aligned to it on EOL.
/// @param height expected number of lines (used to bound the output).
/// @return decoded pixels, or an empty vector on malformed input.
std::vector<uint8_t> decode_rle(const std::vector<uint8_t>& data, int width, int height);

/// Pixel census of a decoded object, without materializing the bitmap.
struct rle_stats_t {
    size_t pixels = 0;     ///< decoded pixels (width*height for a well-formed object)
    size_t non_empty = 0;  ///< pixels with a non-zero palette index
};

/// Decode @p data and count pixels; @return zeroed stats on malformed input.
rle_stats_t rle_stats(const std::vector<uint8_t>& data, int width, int height);

/// Strict decoder used by validate: reports whether the RLE matches the line
/// width exactly (no short/overlong lines) and returns the decoded bitmap.
/// @param ok set to false on any structural violation (must not be null).
std::vector<uint8_t> decode_rle_checked(const std::vector<uint8_t>& data,
                                        int width, int height, bool& ok);

}
}
