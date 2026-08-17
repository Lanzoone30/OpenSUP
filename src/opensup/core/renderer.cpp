// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/core/renderer.h"
#include "opensup/media/pgraphics.h"
#include "opensup/common/logger.h"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <queue>

namespace opensup {
namespace core {

using common::logger_c;
using common::fps_e;
using media::pg_decoder_t;
using media::encode_rle;
using media::palette_t;
using media::palette_entry_t;
using media::optimiser_c;

/// Trim fully-transparent borders from an RGBA image (defined below).
void trim_transparent_padding(const std::vector<uint8_t>& rgba, int width, int height,
                              std::vector<uint8_t>& out, int& out_w, int& out_h,
                              int& offset_x, int& offset_y);

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
                                 bool full_palette,
                                 bool prefer_normal_case,
                                 double quality_factor,
                                 double refresh_rate,
                                 double ssim_tol,
                                 int insert_acquisitions)
    : m_fps(fps), m_width(width), m_height(height), m_quantizer_id(quantizer_id)
    , m_allow_normal_case(allow_normal_case), m_prefer_normal_case(prefer_normal_case)
    , m_overlap(overlap)
    , m_full_palette(full_palette)
    , m_quality_factor(quality_factor)
    , m_refresh_rate(refresh_rate)
    , m_ssim_tol(ssim_tol)
    , m_insert_acquisitions(insert_acquisitions) {}

bool
epoch_encoder_c::quantize_image(const std::vector<uint8_t>& rgba, int width, int height,
                                media::palette_t& out_palette,
                                std::vector<uint8_t>& out_indexed) const
{
    auto opt = media::optimiser_c::get_available();
    if (opt.empty()) return false;

    auto try_quantize = [&](size_t id) -> bool {
        if (id >= opt.size()) return false;
        int max_colors = m_full_palette ? 255 : 254;
        auto result = opt[id]->quantize(rgba, width, height, max_colors);
        if (result.palette.empty() || result.indexed.empty()) return false;
        out_palette = std::move(result.palette);
        out_indexed = std::move(result.indexed);
        return true;
    };

    size_t qid = static_cast<size_t>(m_quantizer_id);
    if (qid >= opt.size()) qid = 0;
    if (try_quantize(qid)) return true;

    // Fall back to the other backend when the preferred one failed.
    size_t fallback_id = (m_quantizer_id == 0 && opt.size() > 1) ? 1 : 0;
    return fallback_id != qid && try_quantize(fallback_id);
}

epoch_timings_t
epoch_encoder_c::compute_timings(double base_pts, uint64_t area) const
{
    // Decode duration: full screen composition rate (SUPer EPOCH_START model).
    // +1 tick margin so PTS-DTS delta strictly exceeds wipe_duration
    // (check_pts_dts_sanity requires delta > wipe_duration, exact tie fails).
    double screen_area = static_cast<double>(m_width) * static_cast<double>(m_height);
    double decode_duration = (std::ceil(screen_area * pg_decoder_t::FREQ / pg_decoder_t::RC) + 1.0) /
                             pg_decoder_t::FREQ;

    epoch_timings_t t;
    t.base_pts = base_pts;
    t.base_dts = base_pts - decode_duration;
    t.obj_decode_time = std::ceil(static_cast<double>(area) * pg_decoder_t::FREQ / pg_decoder_t::RD)
                        / pg_decoder_t::FREQ;
    t.wipe_dur = std::ceil(static_cast<double>(area) * pg_decoder_t::FREQ / pg_decoder_t::RC)
                 / pg_decoder_t::FREQ;
    t.decode_duration = decode_duration;
    return t;
}

std::vector<std::shared_ptr<pg_segment_c>>
epoch_encoder_c::emit_event_segments(const event_emit_input_t& in,
                                     const std::vector<std::shared_ptr<pg_segment_c>>& result_so_far)
{
    std::vector<std::shared_ptr<pg_segment_c>> result;
    auto palette_id = in.palette_id;

    // Build CObject: position = event position + crop offset
    std::vector<c_object_t> cobjects;
    c_object_t obj;
    obj.o_id = in.obj_id;
    obj.window_id = 0;
    obj.h_pos = static_cast<uint16_t>(in.ev_x + in.crop_x);
    obj.v_pos = static_cast<uint16_t>(in.ev_y + in.crop_y);
    obj.flags = in.ev_forced ? c_object_t::forced : c_object_t::standard;
    cobjects.push_back(obj);

    // ── Per-segment timestamps (SUPer set_pts_dts_sc model) ──
    const auto& timings = in.timings;

    // PCS: PTS = presentation, DTS = decode start
    auto pcs = pcs_c::from_scratch(
        static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height),
        static_cast<uint8_t>(in.fps_enum.to_pcsfps()),
        static_cast<uint16_t>(m_composition_n++), in.comp_state,
        false, palette_id, cobjects, timings.base_pts, timings.base_dts);
    result.push_back(std::make_shared<pcs_c>(std::move(pcs)));

    // WDS: PTS = PCS_PTS - wipe_dur
    double wds_pts = timings.base_pts - timings.wipe_dur;
    if (wds_pts < timings.base_dts) wds_pts = timings.base_dts + 1.0 / (pg_decoder_t::FREQ * 2);
    window_definition_t wd;
    wd.window_id = 0;
    wd.h_pos = static_cast<uint16_t>(in.ev_x + in.crop_x);
    wd.v_pos = static_cast<uint16_t>(in.ev_y + in.crop_y);
    wd.width = static_cast<uint16_t>(in.obj_w);
    wd.height = static_cast<uint16_t>(in.obj_h);
    auto wds = wds_c::from_scratch({wd}, wds_pts, timings.base_dts);
    result.push_back(std::make_shared<wds_c>(std::move(wds)));

    // PDS: PTS = DTS (instant), or earlier in overlap mode (previous ENDS).
    // Skipped for reused events — the palette is already in the decoder.
    double pds_pts = timings.base_dts;
    double pds_dts = timings.base_dts;
    if (m_overlap && !result_so_far.empty()) {
        auto prev_seg = result_so_far.back();
        if (auto prev_ends = std::dynamic_pointer_cast<ends_c>(prev_seg)) {
            pds_pts = prev_ends->pts();
            pds_dts = prev_ends->dts();
        }
    }
    if (!in.reusable) {
        auto p_vn = static_cast<uint8_t>(m_palette_vn++);
        auto pds = pds_c::from_scratch(in.palette, p_vn, palette_id, pds_pts, pds_dts);
        result.push_back(std::make_shared<pds_c>(std::move(pds)));

        // ODS: PTS = DTS + obj_decode_time, DTS = timings.base_dts.
        // Skipped for reused events — the object is already decoded.
        double ods_pts = timings.base_dts + timings.obj_decode_time;
        auto rle_data = encode_rle(in.indexed, in.obj_w, in.obj_h);
        auto ods_list = ods_c::from_scratch(in.obj_id, 0,
                                              static_cast<uint16_t>(in.obj_w),
                                              static_cast<uint16_t>(in.obj_h),
                                              rle_data, ods_pts, timings.base_dts);
        for (auto& ods : ods_list)
            result.push_back(std::make_shared<ods_c>(std::move(ods)));

        // ENDS: PTS = DTS = after last ODS
        auto end = ends_c::from_scratch(ods_pts, ods_pts);
        result.push_back(std::make_shared<ends_c>(std::move(end)));
    } else {
        // ENDS: PTS = DTS = presentation time of the reused composition
        auto end = ends_c::from_scratch(timings.base_pts, timings.base_pts);
        result.push_back(std::make_shared<ends_c>(std::move(end)));
    }

    return result;
}

std::vector<std::shared_ptr<pg_segment_c>>
epoch_encoder_c::encode_epoch(const std::vector<bdn_xml_event_c>& events,
                               const std::vector<bool>& redraw_flags,
                               common::fps_e fps_enum,
                               int& palette_id_counter)
{
    std::vector<std::shared_ptr<pg_segment_c>> result;
    if (events.empty()) return result;

    // SUPer: redraw_flags marks forced ACQUISITION points (periodic refresh)
    const std::vector<bool>& forced_acq = redraw_flags;

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

    // ── Drought / quality parameters (from SUPer render2.py) ──
    // m_quality_factor = compression/100 (0 = force all ACQUISITION)
    // m_refresh_rate = acqrate/100 (scales drought increment)
    // m_ssim_tol = ssim_tol/100 (adjusts threshold per resolution)
    // m_insert_acquisitions = extra_acq (min palette updates to force acq)
    const double thresh = m_quality_factor;              // quality_factor
    const double refresh_rate = m_refresh_rate;          // acqrate/100
    const double ssim_offset = 0.014 * std::clamp(m_ssim_tol, -1.0, 1.0);  // ssim_tol adjustment
    const int insert_acqs = m_insert_acquisitions;       // extra_acq
    (void)refresh_rate;  // used in drought logic below

    // Resolution-dependent base SSIM threshold (SUPer formula)
    const double base_ssim_threshold = std::min(0.9999, 0.9608 +
        (static_cast<double>(m_height) - 480.0) * (0.986 - 0.972) / (1080.0 - 480.0));

    m_drought = 0.0;

    // Track palette updates since last acquisition for extra_acq logic
    int palette_updates_since_acq = 0;

    for (size_t k = 0; k < events.size(); k++) {
        auto& ev = events[k];
        bool forced = (k < forced_acq.size() && forced_acq[k]);

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

        // Check for reusable event (identical trimmed bitmap to previous)
        bool reusable = (obj_w == prev_trim_w && obj_h == prev_trim_h &&
                         trimmed_rgba == prev_trimmed);

        // ── SSIM comparison (simplified: pixel-exact on trimmed RGBA) ──
        double ssim_score = 1.0;
        double cross_percentage = 1.0;
        bool force_acquisition = false;

        if (have_prev && !reusable) {
            // Compare previous trimmed RGBA vs current trimmed RGBA
            if (trimmed_rgba != prev_trimmed) {
                // Simple difference metric as proxy for SSIM
                int diff_count = 0;
                size_t np = std::min(trimmed_rgba.size(), prev_trimmed.size()) / 4;
                for (size_t i = 0; i < np; ++i) {
                    if (trimmed_rgba[i*4] != prev_trimmed[i*4] ||
                        trimmed_rgba[i*4+1] != prev_trimmed[i*4+1] ||
                        trimmed_rgba[i*4+2] != prev_trimmed[i*4+2] ||
                        trimmed_rgba[i*4+3] != prev_trimmed[i*4+3]) {
                        diff_count++;
                    }
                }
                ssim_score = 1.0 - static_cast<double>(diff_count) / static_cast<double>(np);
                cross_percentage = 1.0;  // full overlap for same-position events

                // SUPer threshold logic
                double thr_score = std::min(1.0, base_ssim_threshold +
                    (1.0 - base_ssim_threshold) * (1.0 - cross_percentage) - 0.008333 * (1.0 - ssim_offset));

                if (ssim_score >= thr_score) {
                    // Images similar enough — can be NORMAL (redefine object)
                    reusable = true;
                } else {
                    force_acquisition = true;
                }
            }
        }

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
        // Phase A: build transparent pixel mask (PGS: palette[0] must be fully transparent)
        size_t np = static_cast<size_t>(obj_w) * static_cast<size_t>(obj_h);
        std::vector<bool> trans_mask(np, false);
        for (size_t i = 0; i < np; i++) {
            trans_mask[i] = (trimmed_rgba[i * 4 + 3] < 128);
        }
        palette_t palette;
        std::vector<uint8_t> indexed;
        bool quant_ok = quantize_image(trimmed_rgba, obj_w, obj_h, palette, indexed);
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
            logger_c::instance().warn("Quantization failed for event " + std::to_string(k));
            continue;
        }

        // ── Drought decision (SUPer shape_stream logic) ──
        pcs_c::composition_state_e comp_state;

        if (reusable || m_allow_normal_case || m_prefer_normal_case) {
            // Candidate for NORMAL state
            comp_state = pcs_c::composition_state_e::normal;
        } else {
            comp_state = pcs_c::composition_state_e::epoch_start;
        }

        // SUPer drought logic:
        // if (forced or (acq and margin > max(thresh - dthresh*drought, 0))) -> ACQUISITION
        // else -> drought += refresh_rate
        if (thresh == 0.0 && !reusable) {
            // compression=0 -> force all ACQUISITION
            comp_state = pcs_c::composition_state_e::acquisition;
            m_drought = 0.0;
        } else if (comp_state != pcs_c::composition_state_e::acquisition) {
            if (forced || force_acquisition) {
                comp_state = pcs_c::composition_state_e::acquisition;
                m_drought = 0.0;
            } else {
                // Prevent excessive acquisitions, as we want to compress the stream
                m_drought += 1.0 * refresh_rate;
                if (comp_state == pcs_c::composition_state_e::normal) {
                    // Mark as NC refresh candidate (SUPer: node.nc_refresh = True)
                    // For now, just log
                }
            }
        }

        // Extra acquisition insert logic (SUPer: insert_acquisitions)
        if (comp_state == pcs_c::composition_state_e::acquisition) {
            palette_updates_since_acq = 0;
        } else if (!reusable && insert_acqs > 0) {
            palette_updates_since_acq++;
            if (palette_updates_since_acq >= insert_acqs) {
                comp_state = pcs_c::composition_state_e::acquisition;
                palette_updates_since_acq = 0;
                m_drought = 0.0;
            }
        }

        // Object ID double-buffering (SUPer: alternating 0/1 for single window).
        // Reused events keep the previous object id (already decoded).
        uint16_t obj_id;
        if (reusable) {
            obj_id = prev_obj_id;
        } else {
            double_buffer = 1 - double_buffer;
            obj_id = static_cast<uint16_t>(double_buffer);
        }

    // ── Per-segment timestamps (SUPer set_pts_dts_sc model) ──
        epoch_timings_t timings = compute_timings(ev.tc_in(),
            static_cast<uint64_t>(obj_w) * static_cast<uint64_t>(obj_h));

        // Emit the full display set for this event
        event_emit_input_t in{
            obj_w, obj_h, crop_x, crop_y, ev.x(), ev.y(), ev.forced(),
            comp_state, obj_id, palette_id, reusable, fps_enum,
            palette, indexed, timings};
        auto segs = emit_event_segments(in, result);
        for (auto& seg : segs)
            result.push_back(std::move(seg));

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
