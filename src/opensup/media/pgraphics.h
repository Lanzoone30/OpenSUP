// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#pragma once

#include <cstdint>
#include <vector>
#include <cstddef>

namespace opensup {
namespace media {

// ── RLE Codec ──
/// RLE-compress an indexed-color bitmap (PGS ODS payload format).
/// @param bitmap flat indexed pixels, width*height bytes
/// @param width  image width in pixels
/// @param height image height in pixels
/// @return compressed byte stream ready for the ODS segment
std::vector<uint8_t> encode_rle(const std::vector<uint8_t>& bitmap, int width, int height);

// ── PGDecoder Timing Constants ──
/**
 * @brief Timing constants of a nominal PGS decoder.
 * Used by the renderer to predict whether a display set can be decoded in time.
 */
struct pg_decoder_t {
    static constexpr double RX = 2e6;
    static constexpr double RD = 16e6;
    static constexpr double RC = 32e6;
    static constexpr double FREQ = 90000.0;
    static constexpr size_t DECODED_BUF_SIZE = 4 * 1024 * 1024;
    static constexpr size_t CODED_BUF_SIZE   = 1 * 1024 * 1024;
};

} // namespace media
} // namespace opensup