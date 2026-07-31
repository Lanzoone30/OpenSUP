// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.
//
// Quantization backends: libimagequant (GPL-3.0-or-later) and an
// independently implemented octree quantizer inspired by the
// HexTree approach used in SUPer's brule library.

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

class quantizer_base_c {
public:
    virtual ~quantizer_base_c() = default;
    virtual quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                                       int width, int height, int max_colors) = 0;
    virtual std::string name() const = 0;
};

class libimagequant_t : public quantizer_base_c {
public:
    quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                               int width, int height, int max_colors) override;
    std::string name() const override { return "libimagequant"; }
};

class hextree_t : public quantizer_base_c {
public:
    quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                               int width, int height, int max_colors) override;
    std::string name() const override { return "HexTree"; }
};

class optimiser_c {
public:
    static std::vector<std::unique_ptr<quantizer_base_c>> get_available();
};

} // namespace media
} // namespace opensup
