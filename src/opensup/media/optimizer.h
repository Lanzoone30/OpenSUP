// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Quantization backends: libimagequant (GPL-3.0-or-later) and the
// HexTree quantizer from cubicibo/brule (MIT, ported in hextree_impl.cpp).

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "opensup/media/palette.h"

namespace opensup {
namespace media {

struct quantize_result_t {
    media::palette_t palette;
    std::vector<uint8_t> indexed;  // palette index per pixel
};

/// libimagequant tuning. Defaults reproduce the legacy hardcoded values.
struct liq_options_t {
    int quality = 95;
    int speed = 4;
    int dither_pct = 95;
};

/// Sequence co-quantization scaling (oracle config.ini [PILIQ] parity).
inline int liq_seq_quality(int base) { return static_cast<int>(std::lround(base * 0.975)); }
inline float liq_seq_dither(int base_pct) { return static_cast<float>(base_pct) * 0.9f / 100.0f; }

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
    /// @param single_bitmap Single image: full quality/dithering. When false
    ///        (sequence co-quantization) quality and dithering are scaled
    ///        down to keep the union palette compact.
    /// @param matrix YCbCr conversion matrix (bt601|bt709|bt2020).
    /// @param opts libimagequant tuning; ignored by backends that do not use it.
    virtual quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                                       int width, int height, int max_colors,
                                       bool single_bitmap = true,
                                       std::string_view matrix = "bt709",
                                       const liq_options_t& opts = {}) = 0;
    virtual std::string name() const = 0;
};

/// Quantizer backed by libimagequant (high quality, dithering).
class libimagequant_t : public quantizer_base_c {
public:
    quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                               int width, int height, int max_colors,
                               bool single_bitmap,
                               std::string_view matrix,
                               const liq_options_t& opts = {}) override;
    std::string name() const override { return "libimagequant"; }
};

/// Quantizer backed by the ported HexTree algorithm (no external deps).
class hextree_t : public quantizer_base_c {
public:
    quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                               int width, int height, int max_colors,
                               bool single_bitmap,
                               std::string_view matrix,
                               const liq_options_t& opts = {}) override;
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
 *        palette updates.
 *
 * Each pixel is assigned a single bitmap index for its whole color sequence;
 * per-frame palettes map that index to the frame's color. Frames must share
 * dimensions. Runs deterministically (ties broken by byte order).
 *
 * @param frames     Frames of the group, in display order.
 * @param max_colors Maximum number of non-transparent sequences (<=255).
 * @param out        Bitmap + per-frame palette diffs (frame 0 carries all).
 * @param matrix     YCbCr conversion matrix (bt601|bt709|bt2020).
 * @param opts       libimagequant tuning forwarded to the backend.
 * @return true on success.
 */
[[nodiscard]] bool solve_group(const std::vector<group_frame_t>& frames,
                               int max_colors, group_solution_t& out,
                               std::string_view matrix = "bt709",
                               const liq_options_t& opts = {});

/**
 * @brief Factory of quantizers available in this build.
 *
 * Returns all compiled-in backends so callers can pick one by name.
 */
class optimiser_c {
public:
    static std::vector<std::unique_ptr<quantizer_base_c>> get_available();
};

}
}
