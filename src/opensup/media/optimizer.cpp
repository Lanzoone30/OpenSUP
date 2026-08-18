// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Quantization backends: libimagequant (GPL-3.0-or-later) and the
// HexTree quantizer from cubicibo/brule (MIT, ported in hextree_impl.cpp).

#include "opensup/pch.h"
#include "opensup/media/optimizer.h"
#include "opensup/media/hextree_impl.h"
#include "opensup/common/logger.h"

#include <cmath>
#include <array>
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <numeric>
#include <string>
#include <unordered_map>

#include <libimagequant.h>

namespace opensup {
namespace media {

using common::logger_c;

// ── libimagequant backend ──
quantize_result_t
libimagequant_t::quantize(const std::vector<uint8_t>& rgba,
                         int width, int height, int max_colors, bool dither)
{
    quantize_result_t result;

    liq_attr* handle = liq_attr_create();
    if (!handle) {
        logger_c::instance().error("Failed to create libimagequant handle");
        return result;
    }

    liq_set_max_colors(handle, max_colors);
    // Never degrade below 90% quality (defaults to 0-100 which can wreck
    // text palettes); keep dithering gentle for subtitles. Sequence
    // co-quantization needs dither off: per-frame dithering would scramble
    // per-pixel color sequences and explode the union bitmap.
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
    liq_set_dithering_level(quant_result, dither ? 0.3f : 0.0f);

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
                    int width, int /*height*/, int max_colors, bool /*dither*/
                    /* HexTree never dithers */)
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

// ── Group co-quantization (SUPer Optimise.solve_and_remap) ──

namespace {
/// Squared L2 distance between two F*4-byte YCrCbA sequences.
uint64_t seq_l2(const uint8_t* a, const uint8_t* b, size_t n) noexcept
{
    uint64_t d = 0;
    for (size_t i = 0; i < n; i++) {
        int diff = static_cast<int>(a[i]) - static_cast<int>(b[i]);
        d += static_cast<uint64_t>(diff * diff);
    }
    return d;
}
} // namespace

bool
solve_group(const std::vector<group_frame_t>& frames, int max_colors,
            group_solution_t& out)
{
    if (frames.empty() || max_colors < 1 || max_colors > 255) return false;
    const int w = frames[0].width, h = frames[0].height;
    if (w <= 0 || h <= 0) return false;
    const size_t np = static_cast<size_t>(w) * static_cast<size_t>(h);
    const size_t F = frames.size();
    for (const auto& f : frames) {
        if (f.width != w || f.height != h || f.rgba.size() != np * 4) return false;
    }

    auto opt = optimiser_c::get_available();
    if (opt.empty()) return false;

    // Per-frame quantization (dither off — per-frame dithering would scramble
    // per-pixel color sequences and explode the union bitmap).
    std::vector<std::vector<uint8_t>> indexed(F);
    std::vector<std::vector<uint8_t>> clut(F); // 256*4 YCrCbA per frame
    for (size_t ke = 0; ke < F; ke++) {
        quantize_result_t qr;
        bool ok = false;
        for (const auto& qz : opt) {
            qr = qz->quantize(frames[ke].rgba, w, h, max_colors, false);
            if (qr.indexed.size() == np) { ok = true; break; }
        }
        if (!ok) return false;
        indexed[ke] = std::move(qr.indexed);
        clut[ke].assign(256 * 4, 0);
        for (const auto& [idx, e] : qr.palette.entries()) {
            auto* c = &clut[ke][static_cast<size_t>(idx) * 4];
            c[0] = e.y; c[1] = e.cr; c[2] = e.cb; c[3] = e.alpha;
        }
    }

    // Per-pixel color sequences across frames (F*4 YCrCbA bytes each).
    std::vector<uint8_t> seq_data(np * F * 4);
    for (size_t p = 0; p < np; p++) {
        uint8_t* dst = &seq_data[p * F * 4];
        for (size_t ke = 0; ke < F; ke++) {
            std::memcpy(dst + ke * 4,
                        &clut[ke][static_cast<size_t>(indexed[ke][p]) * 4], 4);
        }
    }

    // Catalog distinct sequences. Row ids are assigned in pixel order; the
    // final ordering is made deterministic by a total sort below
    // (unordered_map is only used for lookup, never for iteration).
    struct seq_info_t { size_t count = 0; bool transparent = false; };
    std::vector<std::string> seq_bytes;
    std::vector<seq_info_t> seq_infos;
    std::unordered_map<std::string, size_t> seq_idof;
    std::vector<uint16_t> px_row(np);
    for (size_t p = 0; p < np; p++) {
        std::string key(reinterpret_cast<const char*>(&seq_data[p * F * 4]), F * 4);
        auto it = seq_idof.find(key);
        size_t row;
        if (it == seq_idof.end()) {
            row = seq_infos.size();
            seq_bytes.push_back(key);
            seq_idof.emplace(key, row);
            seq_infos.push_back(seq_info_t{});
        } else {
            row = it->second;
        }
        px_row[p] = static_cast<uint16_t>(row);

        seq_infos[row].count++;
        if (!seq_infos[row].transparent) {
            const uint8_t* s = reinterpret_cast<const uint8_t*>(seq_bytes[row].data());
            bool transp = true;
            for (size_t ke = 0; ke < F; ke++) {
                if (s[ke * 4 + 3] != 0) { transp = false; break; }
            }
            seq_infos[row].transparent = transp;
        }
    }

    // Order sequences by commonness — deterministic total order
    // (count desc, then bytes asc).
    const size_t nseq = seq_infos.size();
    std::vector<size_t> order(nseq);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        if (seq_infos[a].count != seq_infos[b].count)
            return seq_infos[a].count > seq_infos[b].count;
        int cmp = seq_bytes[a].compare(seq_bytes[b]);
        return cmp != 0 ? cmp < 0 : a < b;
    });

    std::vector<size_t> rank_of(nseq);
    for (size_t r = 0; r < nseq; r++) rank_of[order[r]] = r;

    // Transparent handling (SUPer solve_and_remap): the first fully
    // transparent sequence is dropped and remapped to index 0xFF; the other
    // sequences shift around it so the union bitmap matches the palette
    // layout (index 0 unused, transparent = 255).
    int t = -1;
    for (size_t r = 0; r < nseq; r++) {
        if (seq_infos[order[r]].transparent) { t = static_cast<int>(r); break; }
    }
    if (t == -1 && nseq >= static_cast<size_t>(max_colors)) {
        // All slots used and no reserved transparent index — SUPer lowers
        // the color count and retries.
        return solve_group(frames, max_colors - 1, out);
    }

    // Assign ranks, remapping overflow sequences to their nearest kept one.
    const size_t kept_n = std::min(static_cast<size_t>(max_colors), nseq);
    std::vector<uint8_t> bitmap(np);
    for (size_t p = 0; p < np; p++) {
        const size_t rank = rank_of[px_row[p]];
        if (rank < kept_n) {
            bitmap[p] = static_cast<uint8_t>(rank);
        } else {
            const auto* vb = reinterpret_cast<const uint8_t*>(
                seq_bytes[order[rank]].data());
            uint8_t best = 0;
            uint64_t best_d = seq_l2(vb, reinterpret_cast<const uint8_t*>(
                seq_bytes[order[0]].data()), F * 4);
            for (size_t k = 1; k < kept_n; k++) {
                uint64_t d = seq_l2(vb, reinterpret_cast<const uint8_t*>(
                    seq_bytes[order[k]].data()), F * 4);
                if (d < best_d) { best_d = d; best = static_cast<uint8_t>(k); }
            }
            bitmap[p] = best;
        }
    }

    // Final index layout: transparent -> 0xFF, others 1..max_colors.
    for (size_t p = 0; p < np; p++) {
        uint8_t b = bitmap[p];
        if (t == -1) {
            bitmap[p] = static_cast<uint8_t>(b + 1);
        } else if (b == t) {
            bitmap[p] = 0xFF;
        } else if (b < t) {
            bitmap[p] = static_cast<uint8_t>(b + 1);
        }
        // b > t keeps its value: the deleted transparent row shifts the
        // palette rows down, so value b still addresses the right entry.
    }

    // Per-frame palettes: frame 0 carries every entry; later frames only the
    // entries that changed since last set (SUPer diff_cluts). The transparent
    // row t is dropped entirely from the palette space.
    out.palettes.clear();
    out.palettes.reserve(F);
    const size_t pal_rows = kept_n - (t >= 0 && static_cast<size_t>(t) < kept_n ? 1 : 0);
    std::vector<std::array<uint8_t, 4>> acc(pal_rows);
    for (size_t j = 0; j < F; j++) {
        media::palette_t pal_j;
        size_t nrow = 0;
        for (size_t rnk = 0; rnk < kept_n; rnk++) {
            if (static_cast<int>(rnk) == t) continue;
            const uint8_t* s = reinterpret_cast<const uint8_t*>(
                seq_bytes[order[rnk]].data()) + j * 4;
            std::array<uint8_t, 4> e{s[0], s[1], s[2], s[3]};
            if (j == 0) {
                pal_j.set(static_cast<uint8_t>(nrow + 1),
                          palette_entry_t(s[0], s[1], s[2], s[3]));
                acc[nrow] = e;
            } else if (e != acc[nrow]) {
                pal_j.set(static_cast<uint8_t>(nrow + 1),
                          palette_entry_t(s[0], s[1], s[2], s[3]));
                acc[nrow] = e;
            }
            nrow++;
        }
        out.palettes.push_back(std::move(pal_j));
    }

    out.bitmap = std::move(bitmap);
    return true;
}

} // namespace media
} // namespace opensup
