// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Quantization backends: libimagequant (GPL-3.0-or-later) and the
// HexTree quantizer from cubicibo/brule (MIT, ported in hextree_impl.cpp).

#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <utility>

#include "opensup/media/palette.h"

namespace opensup {
namespace media {

struct quantize_result_t {
    media::palette_t palette;
    std::vector<uint8_t> indexed;  // palette index per pixel
};

/**
 * @brief Contract for color quantization backends.
 *
 * Turns an RGBA bitmap into a palette plus indexed pixels. OpenSUP needs
 * this for PGS encoding, where each subtitle bitmap is reduced to at most
 * 256 palette entries. Implementations differ in quality and speed.
 */
class quantizer_base_c {
public:
    virtual ~quantizer_base_c() = default;
    /// @param dither Apply error diffusion (off for sequence co-quantization).
    virtual quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                                       int width, int height, int max_colors,
                                       bool dither = true) = 0;
    virtual std::string name() const = 0;
};

/// Quantizer backed by libimagequant (high quality, dithering).
class libimagequant_t : public quantizer_base_c {
public:
    quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                               int width, int height, int max_colors,
                               bool dither = true) override;
    std::string name() const override { return "libimagequant"; }
};

/// Quantizer backed by the ported HexTree algorithm (no external deps).
class hextree_t : public quantizer_base_c {
public:
    quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                               int width, int height, int max_colors,
                               bool dither = true) override;
    std::string name() const override { return "HexTree"; }
};

/// One frame of a subtitle animation group (raw trimmed RGBA).
struct group_frame_t {
    std::vector<uint8_t> rgba;  ///< width*height*4, straight alpha.
    int width = 0;
    int height = 0;
};

/// Co-quantization result: one union bitmap + per-frame palette updates.
struct group_solution_t {
    std::vector<uint8_t> bitmap;            ///< width*height; 0xFF = transparent.
    std::vector<media::palette_t> palettes; ///< Per frame: frame 0 full, rest diffs.
};

/**
 * @brief Co-quantize a group of frames into one union bitmap plus a chain of
 *        palette updates (SUPer Optimise.solve_and_remap).
 *
 * Each pixel is assigned a single bitmap index for its whole color sequence;
 * per-frame palettes map that index to the frame's color. Frames must share
 * dimensions. Runs deterministically (ties broken by byte order).
 *
 * @param frames     Frames of the group, in display order.
 * @param max_colors Maximum number of non-transparent sequences (<=255).
 * @param out        Bitmap + per-frame palette diffs (frame 0 carries all).
 * @return true on success.
 */
[[nodiscard]] bool solve_group(const std::vector<group_frame_t>& frames,
                               int max_colors, group_solution_t& out);

/**
 * @brief Factory of quantizers available in this build.
 *
 * Returns all compiled-in backends so callers can pick one by name.
 */
class optimiser_c {
public:
    static std::vector<std::unique_ptr<quantizer_base_c>> get_available();
};

} // namespace media
} // namespace opensup
