// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.
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
    virtual quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                                       int width, int height, int max_colors) = 0;
    virtual std::string name() const = 0;
};

/// Quantizer backed by libimagequant (high quality, dithering).
class libimagequant_t : public quantizer_base_c {
public:
    quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                               int width, int height, int max_colors) override;
    std::string name() const override { return "libimagequant"; }
};

/// Quantizer backed by the ported HexTree algorithm (no external deps).
class hextree_t : public quantizer_base_c {
public:
    quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                               int width, int height, int max_colors) override;
    std::string name() const override { return "HexTree"; }
};

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
