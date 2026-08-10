// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.
//
// Quantization backends: libimagequant (GPL-3.0-or-later) and the
// HexTree quantizer from cubicibo/brule (MIT, ported in hextree_impl.cpp).

#include "opensup/pch.h"
#include "opensup/media/optimizer.h"
#include "opensup/media/hextree_impl.h"
#include "opensup/common/logger.h"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <map>
#include <set>

#include <libimagequant.h>

namespace opensup {
namespace media {

using common::logger_c;

// ── libimagequant backend ──
quantize_result_t
libimagequant_t::quantize(const std::vector<uint8_t>& rgba,
                           int width, int height, int max_colors)
{
    quantize_result_t result;

    liq_attr* handle = liq_attr_create();
    if (!handle) {
        logger_c::instance().error("Failed to create libimagequant handle");
        return result;
    }

    liq_set_max_colors(handle, max_colors);
    // Never degrade below 90% quality (defaults to 0-100 which can wreck
    // text palettes); keep dithering gentle for subtitles.
    liq_set_quality(handle, 90, 100);

    liq_image* img = liq_image_create_rgba(handle, rgba.data(), width, height, 0);
    if (!img) {
        liq_attr_destroy(handle);
        logger_c::instance().error("Failed to create libimagequant image");
        return result;
    }

    liq_result* quant_result = nullptr;
    if (liq_image_quantize(img, handle, &quant_result) != LIQ_OK) {
        liq_image_destroy(img);
        liq_attr_destroy(handle);
        logger_c::instance().error("libimagequant quantization failed");
        return result;
    }
    liq_set_dithering_level(quant_result, 0.3f);

    const liq_palette* pal = liq_get_palette(quant_result);
    for (int i = 0; i < static_cast<int>(pal->count); i++) {
        auto& e = pal->entries[i];
        auto entry = palette_entry_t::from_rgba(e.r, e.g, e.b, e.a);
        result.palette.set(static_cast<uint8_t>(i), entry);
    }
    auto pal_count = pal->count;  // save before destroy (use-after-free on MinGW)

    auto w = static_cast<size_t>(width);
    auto h = static_cast<size_t>(height);
    std::vector<uint8_t> indexed(w * h);
    if (liq_write_remapped_image(quant_result, img, indexed.data(),
                                  w * h) != LIQ_OK) {
        logger_c::instance().error("libimagequant remap failed");
    }

    result.indexed = std::move(indexed);

    liq_result_destroy(quant_result);
    liq_image_destroy(img);
    liq_attr_destroy(handle);

    logger_c::instance().log(common::log_level_e::hdebug, "libimagequant: " +
                                std::to_string(pal_count) + " colors");
    return result;
}
// ── HexTree backend (ported from cubicibo/brule, see hextree_impl.cpp) ──

quantize_result_t
hextree_t::quantize(const std::vector<uint8_t>& rgba,
                    int width, int /*height*/, int max_colors)
{
    quantize_result_t result;

    std::vector<uint8_t> palette_bytes;
    std::vector<uint8_t> indexed;
    int pal_count = 0;
    if (!hextree_quantize_core(rgba.data(), rgba.size() / 4,
                               static_cast<uint32_t>(width), max_colors,
                               palette_bytes, indexed, pal_count)) {
        logger_c::instance().error("HexTree quantization failed");
        return result;
    }

    for (int i = 0; i < pal_count; i++) {
        result.palette.set(static_cast<uint8_t>(i),
                           palette_entry_t::from_rgba(
                               palette_bytes[static_cast<size_t>(i) * 4],
                               palette_bytes[static_cast<size_t>(i) * 4 + 1],
                               palette_bytes[static_cast<size_t>(i) * 4 + 2],
                               palette_bytes[static_cast<size_t>(i) * 4 + 3]));
    }
    result.indexed = std::move(indexed);

    logger_c::instance().log(common::log_level_e::hdebug,
                             "HexTree: " + std::to_string(pal_count) + " colors");
    return result;
}

std::vector<std::unique_ptr<quantizer_base_c>>
optimiser_c::get_available()
{
    std::vector<std::unique_ptr<quantizer_base_c>> backends;
    backends.push_back(std::make_unique<libimagequant_t>());
    backends.push_back(std::make_unique<hextree_t>());
    return backends;
}

} // namespace media
} // namespace opensup
