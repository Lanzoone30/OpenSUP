// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/core/renderer.h"
#include "opensup/media/pgraphics.h"
#include "opensup/media/pgstream.h"
#include "opensup/common/logger.h"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <queue>

namespace opensup {
namespace core {

using common::logger_c;
using common::box_t;
using common::fps_e;
using media::pg_decoder_t;
using media::encode_rle;
using media::palette_t;
using media::palette_entry_t;
using media::optimiser_c;

// ── CTU: recursive tile comparison ──
ctu_result_t
compare_tiles(const uint8_t* a, const uint8_t* b,
               int width, int height, int stride)
{
    // ponytail: simple pixel-by-pixel diff, O(n). Upgrade to hierarchical CTU if hot.
    int min_x = width, min_y = height, max_x = 0, max_y = 0;
    bool identical = true;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int off = y * stride + x * 4;
            if (std::memcmp(a + off, b + off, 4) != 0) {
                identical = false;
                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
            }
        }
    }

    if (identical)
        return {true, {0, 0, 0, 0}};

    return {false, box_t::from_coords(min_x, min_y, max_x + 1, max_y + 1)};
}

// ── Window Analyzer ──
window_analyzer_c::window_analyzer_c(double ssim_tol)
    : m_ssim_tol(ssim_tol) {}

window_analysis_t
window_analyzer_c::analyze(const uint8_t* prev, const uint8_t* curr,
                            int width, int height, const common::box_t& window)
{
    // ponytail: pixel-diff based detection, SSIM stub. Replace with real SSIM when OpenCV is available.
    auto stride = static_cast<size_t>(width) * 4;
    auto result = compare_tiles(prev, curr, width, height, static_cast<int>(stride));
    return {window, !result.identical, result.identical ? 1.0 : 0.95};
}

// ── Padding Engine ──
std::vector<uint8_t>
pad_image_8x8(const std::vector<uint8_t>& rgba, int width, int height,
               int& out_width, int& out_height)
{
    out_width = ((width + 7) / 8) * 8;
    out_height = ((height + 7) / 8) * 8;

    if (out_width == width && out_height == height)
        return rgba;

    auto ow = static_cast<size_t>(out_width);
    auto oh = static_cast<size_t>(out_height);
    auto w_ = static_cast<size_t>(width);
    auto h_ = static_cast<size_t>(height);
    std::vector<uint8_t> padded(ow * oh * 4, 0);
    for (size_t y = 0; y < h_; y++) {
        std::memcpy(&padded[y * ow * 4], &rgba[y * w_ * 4], w_ * 4);
    }
    return padded;
}

// ── DS Node ──
ds_node_t::ds_node_t(std::shared_ptr<display_set_t> display_set)
    : ds(std::move(display_set)) {}

void
ds_node_t::compute_timing(double /*fps*/, const common::box_t& /*window*/)
{
    pts_origin = ds->pts();
    dts_origin = ds->segments[0]->dts();

    if (cumulated_ods_size > 0) {
        // Account for decode duration
        double decode_time = std::ceil(pg_decoder_t::FREQ *
            static_cast<double>(cumulated_ods_size) / pg_decoder_t::RD) / pg_decoder_t::FREQ;
        dts_origin = pts_origin - decode_time;
    }
}

void
trim_transparent_padding(const std::vector<uint8_t>& rgba, int width, int height,
                          std::vector<uint8_t>& out, int& out_w, int& out_h,
                          int& offset_x, int& offset_y)
{
    int min_x = width, min_y = height, max_x = 0, max_y = 0;
    bool found = false;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t idx = static_cast<size_t>(y * width + x) * 4;
            if (rgba[idx + 3] > 0) {  // alpha > 0
                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
                found = true;
            }
        }
    }
    if (!found) {
        min_x = 0; min_y = 0; max_x = 0; max_y = 0;
    }
    out_w = max_x - min_x + 1;
    out_h = max_y - min_y + 1;
    offset_x = min_x;
    offset_y = min_y;
    out.clear();
    if (found) {
        out.reserve(static_cast<size_t>(out_w) * static_cast<size_t>(out_h) * 4);
        for (int y = min_y; y <= max_y; y++) {
            auto src_start = rgba.begin() + static_cast<ptrdiff_t>((y * width + min_x) * 4);
            auto src_end = src_start + static_cast<ptrdiff_t>(out_w * 4);
            out.insert(out.end(), src_start, src_end);
        }
    }
}

// ── Epoch Encoder ──
epoch_encoder_c::epoch_encoder_c(double fps, int width, int height, int quantizer_id,
                                 bool allow_normal_case, bool overlap,
                                 bool full_palette, double ssim_tol)
    : m_fps(fps), m_width(width), m_height(height), m_quantizer_id(quantizer_id)
    , m_allow_normal_case(allow_normal_case), m_overlap(overlap)
    , m_full_palette(full_palette), m_ssim_tol(ssim_tol) {}

std::vector<std::shared_ptr<pg_segment_c>>
epoch_encoder_c::encode_epoch(const std::vector<bdn_xml_event_c>& events,
                               const std::vector<bool>& redraw_flags,
                               common::fps_e fps_enum,
                               int& palette_id_counter)
{
    std::vector<std::shared_ptr<pg_segment_c>> result;
    if (events.empty()) return result;
    (void)redraw_flags;

    auto palette_id = static_cast<uint8_t>(palette_id_counter % 8);
    palette_id_counter++;

    int double_buffer = 1;  // SUPer: double-buffering alternates 0/1 for 1 window

    // Track previous event for clear DS insertion (SUPer _get_undisplay pattern)
    double prev_tc_out = 0.0;
    int prev_pos_x = 0, prev_pos_y = 0, prev_obj_w = 0, prev_obj_h = 0;
    bool have_prev = false;

    // Previous trimmed bitmap (same epoch) and object id of the last ODS —
    // reused by following events with identical content.
    std::vector<uint8_t> prev_trimmed;
    int prev_trim_w = 0, prev_trim_h = 0;
    uint16_t prev_obj_id = 0;

    // Helper to emit a clear display set at a given PTS using previous event's window
    auto emit_clear_ds = [&](double pts) -> void {
        double wipe_dur = std::ceil(static_cast<double>(prev_obj_w) *
            static_cast<double>(prev_obj_h) * pg_decoder_t::FREQ / pg_decoder_t::RC) /
            pg_decoder_t::FREQ;
        double dts = pts - wipe_dur;
        if (dts < 0) dts = 0;
        // PCS: NORMAL state, empty cobjects, no palette flag
        auto clear_pcs = pcs_c::from_scratch(
            static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height),
            static_cast<uint8_t>(fps_enum.to_pcsfps()),
            static_cast<uint16_t>(m_composition_n++),
            pcs_c::composition_state_e::normal,
            false, palette_id, {}, pts, dts);
        result.push_back(std::make_shared<pcs_c>(std::move(clear_pcs)));
        // WDS: use previous event's window (clear where it was shown)
        window_definition_t clear_wd;
        clear_wd.window_id = 0;
        clear_wd.h_pos = static_cast<uint16_t>(prev_pos_x);
        clear_wd.v_pos = static_cast<uint16_t>(prev_pos_y);
        clear_wd.width = static_cast<uint16_t>(prev_obj_w);
        clear_wd.height = static_cast<uint16_t>(prev_obj_h);
        auto clear_wds = wds_c::from_scratch({clear_wd}, pts, dts);
        result.push_back(std::make_shared<wds_c>(std::move(clear_wds)));
        // ENDS
        auto clear_end = ends_c::from_scratch(pts, dts);
        result.push_back(std::make_shared<ends_c>(std::move(clear_end)));
    };

    // ponytail: SSIM and drought algorithm disabled — always emit full acquisition.
    // The decoder requires ODS for every event to display fresh pixel data.
    // Re-enable when real SSIM (not pixel-diff stub) and palette chain pre-computation
    // are implemented, matching SUPer Optimise.solve_and_remap.
    m_drought = 0.0;

    for (size_t k = 0; k < events.size(); k++) {
        auto& ev = events[k];

        // Full acquisition: clear DS before every non-first event
        if (have_prev) {
            logger_c::instance().log(common::log_level_e::hdebug,
                "Clear DS at " + std::to_string(prev_tc_out) +
                " before event at " + std::to_string(ev.tc_in()));
            emit_clear_ds(prev_tc_out);
        }

        auto rgba = ev.load_image();
        if (rgba.empty()) {
            logger_c::instance().warn("Empty image at event " + std::to_string(k));
            continue;
        }

        // Trim transparent padding (like legacy's getbbox())
        int trim_w, trim_h, trim_x, trim_y;
        std::vector<uint8_t> trimmed_rgba;
        trim_transparent_padding(rgba, ev.width(), ev.height(),
                                  trimmed_rgba, trim_w, trim_h, trim_x, trim_y);
        int obj_w = (trim_w > 0) ? trim_w : ev.width();
        int obj_h = (trim_h > 0) ? trim_h : ev.height();
        int crop_x = (trim_w > 0) ? trim_x : 0;
        int crop_y = (trim_h > 0) ? trim_y : 0;

        // A reusable event (same trimmed bitmap as the previous one) skips
        // PDS/ODS — its PCS references the previous object id, already in the
        // decoder buffer, with a new crop.
        bool reusable = (obj_w == prev_trim_w && obj_h == prev_trim_h &&
                         trimmed_rgba == prev_trimmed);
        if (reusable) {
            m_reuse_candidates++;
            logger_c::instance().log(common::log_level_e::hdebug,
                "Reuse candidate event " + std::to_string(k) + " (" +
                std::to_string(obj_w) + "x" + std::to_string(obj_h) + ")");
        }
        prev_trimmed = trimmed_rgba;
        prev_trim_w = obj_w;
        prev_trim_h = obj_h;

        // Quantize the trimmed RGBA image
        auto opt = optimiser_c::get_available();

        // Phase A: build transparent pixel mask (PGS: palette[0] must be fully transparent)
        size_t np = static_cast<size_t>(obj_w) * static_cast<size_t>(obj_h);
        std::vector<bool> trans_mask(np, false);
        for (size_t i = 0; i < np; i++) {
            trans_mask[i] = (trimmed_rgba[i * 4 + 3] < 128);
        }

        palette_t palette;
        std::vector<uint8_t> indexed;

        bool quant_ok = false;
        if (!opt.empty()) {
            auto qid = static_cast<size_t>(m_quantizer_id);
            if (qid >= opt.size()) qid = 0;
            int max_colors = m_full_palette ? 255 : 254;
            auto quant_result = opt[qid]->quantize(trimmed_rgba, obj_w, obj_h, max_colors);
            if (!quant_result.palette.empty() && !quant_result.indexed.empty()) {
                palette = std::move(quant_result.palette);
                indexed = std::move(quant_result.indexed);
                quant_ok = true;
            }
        }
        if (!quant_ok && opt.size() > 1) {
            size_t fallback_id = (m_quantizer_id == 0 && opt.size() > 1) ? 1 : 0;
            if (fallback_id < opt.size()) {
                int max_colors = m_full_palette ? 255 : 254;
                auto quant_result = opt[fallback_id]->quantize(trimmed_rgba, obj_w, obj_h, max_colors);
                if (!quant_result.palette.empty() && !quant_result.indexed.empty()) {
                    palette = std::move(quant_result.palette);
                    indexed = std::move(quant_result.indexed);
                    quant_ok = true;
                }
            }
        }

        if (quant_ok) {
            if (!m_full_palette) {
                // PGS fix: shift palette to make index 0 = fully transparent
                palette.offset(1);
                palette.set(0, palette_entry_t::from_rgba(0, 0, 0, 0));
                // Remap indexed image: transparent pixels → 0, others → old_idx + 1
                for (size_t i = 0; i < indexed.size(); i++) {
                    indexed[i] = trans_mask[i] ? 0 : static_cast<uint8_t>(indexed[i] + 1);
                }
            }
        } else {
            palette.set(0, palette_entry_t::from_rgba(0, 0, 0, 0));
            indexed.resize(np, 0);
        }

        // Full acquisition: emit all segments
        // Composition state: epoch_start by default, normal if allow_normal_case set.
        // A reused event must be NORMAL — it references an object already in
        // the decoder buffer (not reset between same-epoch events).
        auto comp_state = (reusable || m_allow_normal_case)
            ? pcs_c::composition_state_e::normal
            : pcs_c::composition_state_e::epoch_start;

        // Object ID double-buffering (SUPer: alternating 0/1 for single window).
        // Reused events keep the previous object id (already decoded).
        uint16_t obj_id;
        if (reusable) {
            obj_id = prev_obj_id;
        } else {
            double_buffer = 1 - double_buffer;
            obj_id = static_cast<uint16_t>(double_buffer);
        }

        // Build CObject: position = event position + crop offset
        std::vector<c_object_t> cobjects;
        c_object_t obj;
        obj.o_id = obj_id;
        obj.window_id = 0;
        obj.h_pos = static_cast<uint16_t>(ev.x() + crop_x);
        obj.v_pos = static_cast<uint16_t>(ev.y() + crop_y);
        obj.flags = ev.forced() ? c_object_t::forced : c_object_t::standard;
        cobjects.push_back(obj);

        // ── Per-segment timestamps (SUPer set_pts_dts_sc model) ──
        double base_pts = ev.tc_in();
        uint64_t area = static_cast<uint64_t>(obj_w) * static_cast<uint64_t>(obj_h);

        // Decode duration: full screen composition rate (SUPer EPOCH_START model).
        // +1 tick margin so PTS-DTS delta strictly exceeds wipe_duration
        // (check_pts_dts_sanity requires delta > wipe_duration, exact tie fails).
        double screen_area = static_cast<double>(m_width) * static_cast<double>(m_height);
        double decode_duration = (std::ceil(screen_area * pg_decoder_t::FREQ / pg_decoder_t::RC) + 1.0) /
                                 pg_decoder_t::FREQ;
        double base_dts = base_pts - decode_duration;

        double obj_decode_time = std::ceil(static_cast<double>(area) * pg_decoder_t::FREQ / pg_decoder_t::RD)
                                 / pg_decoder_t::FREQ;
        double wipe_dur = std::ceil(static_cast<double>(area) * pg_decoder_t::FREQ / pg_decoder_t::RC)
                          / pg_decoder_t::FREQ;

        // PCS: PTS = presentation, DTS = decode start
        auto pcs = pcs_c::from_scratch(
            static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height),
            static_cast<uint8_t>(fps_enum.to_pcsfps()),
            static_cast<uint16_t>(m_composition_n++), comp_state,
            false, palette_id, cobjects, base_pts, base_dts);
        result.push_back(std::make_shared<pcs_c>(std::move(pcs)));

        // WDS: PTS = PCS_PTS - wipe_dur
        double wds_pts = base_pts - wipe_dur;
        if (wds_pts < base_dts) wds_pts = base_dts + 1.0 / (pg_decoder_t::FREQ * 2);
        window_definition_t wd;
        wd.window_id = 0;
        wd.h_pos = static_cast<uint16_t>(ev.x() + crop_x);
        wd.v_pos = static_cast<uint16_t>(ev.y() + crop_y);
        wd.width = static_cast<uint16_t>(obj_w);
        wd.height = static_cast<uint16_t>(obj_h);
        auto wds = wds_c::from_scratch({wd}, wds_pts, base_dts);
        result.push_back(std::make_shared<wds_c>(std::move(wds)));

        // PDS: PTS = DTS (instant), or earlier in overlap mode.
        // Skipped for reused events — the palette is already in the decoder.
        double pds_pts = base_dts;
        double pds_dts = base_dts;
        if (m_overlap && result.size() >= 3) {
            auto prev_seg = result[result.size() - 3];
            if (auto prev_ends = std::dynamic_pointer_cast<ends_c>(prev_seg)) {
                pds_pts = prev_ends->pts();
                pds_dts = prev_ends->dts();
            }
        }
        if (!reusable) {
            auto p_vn = static_cast<uint8_t>(m_palette_vn++);
            auto pds = pds_c::from_scratch(palette, p_vn, palette_id, pds_pts, pds_dts);
            result.push_back(std::make_shared<pds_c>(std::move(pds)));

            // ODS: PTS = DTS + obj_decode_time, DTS = base_dts.
            // Skipped for reused events — the object is already decoded.
            double ods_pts = base_dts + obj_decode_time;
            auto rle_data = encode_rle(indexed, obj_w, obj_h);
            auto ods_list = ods_c::from_scratch(obj_id, 0,
                                                  static_cast<uint16_t>(obj_w),
                                                  static_cast<uint16_t>(obj_h),
                                                  rle_data, ods_pts, base_dts);
            for (auto& ods : ods_list)
                result.push_back(std::make_shared<ods_c>(std::move(ods)));

            // ENDS: PTS = DTS = after last ODS
            auto end = ends_c::from_scratch(ods_pts, ods_pts);
            result.push_back(std::make_shared<ends_c>(std::move(end)));
        } else {
            // ENDS: PTS = DTS = presentation time of the reused composition
            auto end = ends_c::from_scratch(base_pts, base_pts);
            result.push_back(std::make_shared<ends_c>(std::move(end)));
        }
        prev_obj_id = obj_id;

        // Save event data for next iteration's clear DS
        prev_tc_out = ev.tc_out();
        prev_pos_x = ev.x() + crop_x;
        prev_pos_y = ev.y() + crop_y;
        prev_obj_w = obj_w;
        prev_obj_h = obj_h;
        have_prev = true;
    }

    // ── Final clear DS after the last event (SUPer: end of epoch wipe) ──
    if (have_prev) {
        emit_clear_ds(prev_tc_out);
        logger_c::instance().log(common::log_level_e::hdebug, "Final clear DS at " + std::to_string(prev_tc_out));
    }

    logger_c::instance().info("Encoded epoch with " + std::to_string(events.size()) +
                                " events, " + std::to_string(result.size()) + " segments.");
    return result;
}

} // namespace core
} // namespace opensup
