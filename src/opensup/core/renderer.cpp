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
#include "opensup/common/ssim.h"

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

/// Straight-alpha compositing: dst = over(dst, src) — like PIL.Image.alpha_composite.
/// Both src and dst must have same dimensions and 4 channels (RGBA).
static void alpha_over(std::vector<uint8_t>& dst, const uint8_t* src, int width, int height)
{
    const size_t np = static_cast<size_t>(width) * static_cast<size_t>(height);
    for (size_t i = 0; i < np; ++i) {
        const size_t idx = i * 4;
        const uint8_t sa = src[idx + 3];
        if (sa == 0) continue;                    // fully transparent src → no change
        const uint8_t da = dst[idx + 3];
        const int sa_i = static_cast<int>(sa);
        const int da_i = static_cast<int>(da);
        const uint16_t oa = static_cast<uint16_t>(sa_i + da_i * (255 - sa_i) / 255);
        if (oa == 0) {                            // both transparent
            dst[idx + 3] = 0;
            continue;
        }
        const double sfa = static_cast<double>(sa) / static_cast<double>(oa);
        const double dfa = (1.0 - sfa) * static_cast<double>(da) / static_cast<double>(oa);
        dst[idx + 0] = static_cast<uint8_t>(std::clamp(
            static_cast<double>(src[idx + 0]) * sfa + static_cast<double>(dst[idx + 0]) * dfa + 0.5, 0.0, 255.0));
        dst[idx + 1] = static_cast<uint8_t>(std::clamp(
            static_cast<double>(src[idx + 1]) * sfa + static_cast<double>(dst[idx + 1]) * dfa + 0.5, 0.0, 255.0));
        dst[idx + 2] = static_cast<uint8_t>(std::clamp(
            static_cast<double>(src[idx + 2]) * sfa + static_cast<double>(dst[idx + 2]) * dfa + 0.5, 0.0, 255.0));
        dst[idx + 3] = static_cast<uint8_t>(oa);
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

    // Previous event timing (SUPer find_acqs: dts/dts_end/margin for the
    // decode-margin acquisition decision). pts_gap and dtl are computed per event.
    double prev_base_pts = 0.0;   // previous event's PTS (tc_in)
    double prev_tc_in = 0.0;      // previous event's tc_in (display start)
    double prev_wipe_dur = 0.0;   // previous event's object wipe duration
    double epoch_write_dur = 0.0; // first event's full-screen decode duration (write_duration)

    // Previous trimmed bitmap (same epoch) and object id of the last ODS —
    // reused by following events with identical content.
    std::vector<uint8_t> prev_trimmed;
    int prev_trim_w = 0, prev_trim_h = 0;
    uint16_t prev_obj_id = 0;

    // Composite bitmap for the current group (SUPer alpha_compo).
    // Accumulates via alpha_over; compared against via compare_with_alpha.
    std::vector<uint8_t> compo_rgba;
    int compo_w = 0, compo_h = 0;
    int compo_x = 0, compo_y = 0;
    bool have_compo = false;

    // Helper to emit a clear display set at a given PTS using a window
    // geometry (previous event's window for inline emits, recorded geometry
    // when flushing a buffered group).
    auto emit_clear_ds = [&](double pts, int w, int h, int x, int y) -> void {
        double wipe_dur = std::ceil(static_cast<double>(w) *
            static_cast<double>(h) * pg_decoder_t::FREQ / pg_decoder_t::RC) /
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
        clear_wd.h_pos = static_cast<uint16_t>(x);
        clear_wd.v_pos = static_cast<uint16_t>(y);
        clear_wd.width = static_cast<uint16_t>(w);
        clear_wd.height = static_cast<uint16_t>(h);
        auto clear_wds = wds_c::from_scratch({clear_wd}, pts, dts);
        result.push_back(std::make_shared<wds_c>(std::move(clear_wds)));
        // ENDS
        auto clear_end = ends_c::from_scratch(pts, dts);
        result.push_back(std::make_shared<ends_c>(std::move(clear_end)));
    };

    // ── Drought / quality parameters (from SUPer render2.py) ──
    // m_quality_factor = compression/100 (0 = force all ACQUISITION)
    // m_dquality_factor = 0.035 (drought decay factor)
    // m_refresh_rate = acqrate/100 (scales drought increment)
    // m_ssim_tol = ssim_tol/100 (adjusts threshold per resolution)
    // m_insert_acquisitions = extra_acq (min palette updates to force acq)
    const double thresh = m_quality_factor;              // quality_factor
    const double refresh_rate = m_refresh_rate;          // acqrate/100
    const double ssim_offset = 0.014 * std::clamp(m_ssim_tol, -1.0, 1.0);  // ssim_tol adjustment
    const int insert_acqs = m_insert_acquisitions;       // extra_acq

    // Resolution-dependent base SSIM threshold (SUPer formula)
    const double base_ssim_threshold = std::min(0.9999, 0.9608 +
        (static_cast<double>(m_height) - 480.0) * (0.986 - 0.972) / (1080.0 - 480.0));

    m_drought = 0.0;

    // Track palette updates since last acquisition for extra_acq logic
    int palette_updates_since_acq = 0;

    // ── Group buffering (P4b) ──
    // A run of SSIM-fused / byte-identical events sharing one composition is
    // buffered and co-quantized at group end: 1 union ODS + per-event palette
    // diffs (SUPer solve_and_remap + diff_cluts). Before P4b, fused events
    // emitted no bitmap at all, so fades were invisible.
    struct chain_event_t {
        std::vector<uint8_t> rgba;      // trimmed frame (raw, for co-quant)
        int w = 0, h = 0, crop_x = 0, crop_y = 0, ev_x = 0, ev_y = 0;
        bool ev_forced = false;
        pcs_c::composition_state_e comp_state = pcs_c::composition_state_e::epoch_start;
        uint16_t obj_id = 0;
        epoch_timings_t timings;
        bool has_clear = false;      // previous-event clear DS params
        int clear_x = 0, clear_y = 0, clear_w = 0, clear_h = 0;
        double clear_pts = 0.0;
        bool refresh = false;        // re-emit union ODS (late decoders)
        bool acq_insert = false;     // extra_acq mid-group acquisition
        epoch_timings_t acq_timings;
        media::palette_t own_palette;   // quantized own frame (acq insert)
        std::vector<uint8_t> own_indexed;
    };
    std::vector<chain_event_t> chain;
    bool chain_active = false;

    // Flush the buffered chain: 1 union ODS + per-event palette diffs.
    auto flush_chain = [&]() -> void {
        if (chain.empty()) return;
        std::vector<media::group_frame_t> frames;
        frames.reserve(chain.size());
        for (const auto& e : chain) {
            media::group_frame_t f;
            f.rgba = e.rgba;
            f.width = e.w;
            f.height = e.h;
            frames.push_back(std::move(f));
        }
        media::group_solution_t sol;
        const bool solved = media::solve_group(
            frames, m_full_palette ? 255 : 254, sol);
        if (!solved || sol.palettes.size() != chain.size()) {
            // Fallback: emit each event standalone (fresh ODS each).
            logger_c::instance().warn(
                "Group co-quantization failed; falling back to per-event emission");
            for (const auto& e : chain) {
                if (e.has_clear)
                    emit_clear_ds(e.clear_pts, e.clear_w, e.clear_h, e.clear_x, e.clear_y);
                event_emit_input_t in{e.w, e.h, e.crop_x, e.crop_y, e.ev_x, e.ev_y,
                                      e.ev_forced, e.comp_state, e.obj_id, palette_id,
                                      false, fps_enum, e.own_palette, e.own_indexed,
                                      e.timings};
                auto segs = emit_event_segments(in, result);
                for (auto& seg : segs) result.push_back(std::move(seg));
            }
            chain.clear();
            chain_active = false;
            return;
        }

        for (size_t i = 0; i < chain.size(); i++) {
            auto& e = chain[i];
            if (e.has_clear)
                emit_clear_ds(e.clear_pts, e.clear_w, e.clear_h, e.clear_x, e.clear_y);
            if (i == 0) {
                // Group start: union bitmap + full palette.
                event_emit_input_t in{e.w, e.h, e.crop_x, e.crop_y, e.ev_x, e.ev_y,
                                      e.ev_forced, e.comp_state, e.obj_id, palette_id,
                                      false, fps_enum, sol.palettes[0], sol.bitmap,
                                      e.timings};
                auto segs = emit_event_segments(in, result);
                for (auto& seg : segs) result.push_back(std::move(seg));
            } else {
                // Palette update event: PCS/WDS/ENDS as today + diff PDS.
                event_emit_input_t in{e.w, e.h, e.crop_x, e.crop_y, e.ev_x, e.ev_y,
                                      e.ev_forced, e.comp_state, e.obj_id, palette_id,
                                      true /* reuse: no ODS */, fps_enum,
                                      sol.palettes[i], e.own_indexed, e.timings};
                auto segs = emit_event_segments(in, result);
                std::shared_ptr<pds_c> diff_pds;
                if (!sol.palettes[i].empty()) {
                    double pds_pts = e.timings.base_dts, pds_dts = e.timings.base_dts;
                    if (m_overlap && !result.empty()) {
                        if (auto prev = std::dynamic_pointer_cast<ends_c>(result.back())) {
                            pds_pts = prev->pts();
                            pds_dts = prev->dts();
                        }
                    }
                    diff_pds = std::make_shared<pds_c>(pds_c::from_scratch(
                        sol.palettes[i], static_cast<uint8_t>(m_palette_vn++),
                        palette_id, pds_pts, pds_dts));
                }
                for (auto& seg : segs) {
                    if (diff_pds && dynamic_cast<ends_c*>(seg.get()) != nullptr)
                        result.push_back(diff_pds);  // PDS right before ENDS
                    result.push_back(std::move(seg));
                }
            }
            // Refresh: re-send the union object so decoders joining the
            // composition late can re-acquire it (SUPer nc_refresh).
            if (e.refresh && i > 0) {
                double ods_pts = e.timings.base_dts + e.timings.obj_decode_time;
                auto rle = encode_rle(sol.bitmap, e.w, e.h);
                auto ods_list = ods_c::from_scratch(e.obj_id, 0,
                    static_cast<uint16_t>(e.w), static_cast<uint16_t>(e.h),
                    rle, ods_pts, e.timings.base_dts);
                for (auto& ods : ods_list)
                    result.push_back(std::make_shared<ods_c>(std::move(ods)));
                result.push_back(std::make_shared<ends_c>(
                    ends_c::from_scratch(ods_pts, ods_pts)));
            }
            // Mid-group acquisition (extra_acq): re-send this event's own
            // frame so late decoders can re-acquire mid-fade.
            if (e.acq_insert) {
                event_emit_input_t acq_in{e.w, e.h, e.crop_x, e.crop_y,
                                          e.ev_x, e.ev_y, e.ev_forced,
                                          pcs_c::composition_state_e::acquisition,
                                          e.obj_id, palette_id, false, fps_enum,
                                          e.own_palette, e.own_indexed, e.acq_timings};
                auto acq_segs = emit_event_segments(acq_in, result);
                for (auto& seg : acq_segs) result.push_back(std::move(seg));
            }
        }
        chain.clear();
        chain_active = false;
    };

    for (size_t k = 0; k < events.size(); k++) {
        // Fixed SSIM bar: similarity decides reusable vs force_acquisition only.
        // The drought effect belongs to the decode-margin check (SUPer render2:258).
        const double effective_ssim_threshold = std::min(1.0, base_ssim_threshold +
            ssim_offset);

        auto& ev = events[k];
        bool forced = (k < forced_acq.size() && forced_acq[k]);

        auto rgba = ev.load_image();
        if (rgba.empty()) {
            logger_c::instance().warn("Empty image at event " + std::to_string(k));
            if (chain_active) {
                flush_chain();
                chain_active = false;
            }
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

        // Check for reusable event (identical trimmed bitmap to previous) —
        // position-free (OpenSUP win: PCS move without ODS; SUPer would break on move).
        bool reusable = (obj_w == prev_trim_w && obj_h == prev_trim_h &&
                         trimmed_rgba == prev_trimmed);

        // ── SSIM comparison vs accumulated composite (SUPer WindowAnalyzer) ──
        double ssim_score = 1.0;
        double cross_percentage = 1.0;
        bool force_acquisition = false;

        // Current event's absolute bbox in the window
        const int cur_x = ev.x() + crop_x;
        const int cur_y = ev.y() + crop_y;
        const bool bbox_matches_compo = have_compo &&
            (cur_x == compo_x && cur_y == compo_y && obj_w == compo_w && obj_h == compo_h);

        if (have_prev && !reusable && bbox_matches_compo) {
            // Compare current trimmed bitmap against the group's accumulated composite
            ssim_score = common::ssim_c::compare_with_alpha(
                compo_rgba.data(), trimmed_rgba.data(),
                obj_w, obj_h, cross_percentage);

            // SUPer threshold logic (WindowAnalyzer line 1366)
            double thr_score = std::min(1.0, effective_ssim_threshold +
                (1.0 - effective_ssim_threshold) * (1.0 - cross_percentage) - 0.008333 * (1.0 - ssim_offset));

            if (ssim_score >= thr_score) {
                // FUSE: similar enough — add to composite (alpha_over)
                alpha_over(compo_rgba, trimmed_rgba.data(), obj_w, obj_h);
                reusable = true;
            } else {
                // BREAK: drifted too far — force acquisition, reset composite
                force_acquisition = true;
            }
        } else if (have_prev && !reusable && !bbox_matches_compo) {
            // Bbox changed (move/resize) or no composite yet — treat as new group
            force_acquisition = true;
        }

        // Forced/redraw events always break the group
        // Forced/redraw events always break the group
        if (k < forced_acq.size() && forced_acq[k]) {
            force_acquisition = true;
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

        // Per-segment timestamps (SUPer set_pts_dts_sc model). Computed here so
        // the drought decision below can use the decode-margin (acq/dtl) signals.
        epoch_timings_t timings = compute_timings(ev.tc_in(),
            static_cast<uint64_t>(obj_w) * static_cast<uint64_t>(obj_h));

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
    bool emit_reusable = reusable; // refresh acquisition re-emits the ODS

    if (reusable || m_allow_normal_case || m_prefer_normal_case) {
        // Candidate for NORMAL state
        comp_state = pcs_c::composition_state_e::normal;
    } else {
        comp_state = pcs_c::composition_state_e::epoch_start;
    }

    // Decode-margin signals (SUPer find_acqs): acq = valid timing, dtl = slack
    // ratio between this event's decode start and the previous decode end.
    // Object-specific decode time (RD read + RC write) as in SUPer's
    // get_decode_duration; the emitted base_dts keeps the conservative
    // full-screen model.
    bool acq_valid = false;
    double dtl = 0.0;
    if (have_prev) {
        const double obj_decode_dur = timings.obj_decode_time + timings.wipe_dur;
        const double dts_eff = timings.base_pts - obj_decode_dur;
        const double prev_dts_end = prev_base_pts - prev_wipe_dur;
        const double pts_gap = timings.base_pts - prev_base_pts;
        const double prev_display_dur = prev_tc_out - prev_tc_in;
        acq_valid = (dts_eff > prev_dts_end) && (pts_gap > epoch_write_dur);
        dtl = (prev_display_dur > 0.0) ? (dts_eff - prev_dts_end) / prev_display_dur : 0.0;
    }

    // SUPer drought logic:
    // if (forced or (acq and margin > max(thresh - dthresh*drought, 0))) and
    //    NOT nc_refresh -> ACQUISITION   (render2.py:258)
    // else -> drought += refresh_rate
    // nc_refresh nodes (reusable palette updates) NEVER become ACQUISITION;
    // with enough decode margin they only re-emit the ODS (stay NORMAL).
    if (thresh == 0.0 && !reusable) {
        // compression=0 -> force all ACQUISITION
        comp_state = pcs_c::composition_state_e::acquisition;
        m_drought = 0.0;
    } else if (comp_state != pcs_c::composition_state_e::acquisition) {
        if (forced || force_acquisition) {
            comp_state = pcs_c::composition_state_e::acquisition;
            m_drought = 0.0;
        } else if (reusable && acq_valid &&
                   dtl > std::max(thresh - m_dquality_factor * m_drought, 0.0)) {
            // Refresh: same object but enough decode margin — re-send the ODS
            // so late-starting decoders can re-sync, yet stay NORMAL (this is
            // a palette-update node, not a group restart: SUPer nc_refresh).
            emit_reusable = false;
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

    // Extra acquisition counter (SUPer: len(pals[0]) per window).
    // Counts NORMAL events (palette updates) since the last acquisition;
    // EPOCH_START and ACQUISITION both start a new group (reset).
    // The mid-event acquisition itself is inserted after the display set below.
    if (comp_state == pcs_c::composition_state_e::acquisition ||
        comp_state == pcs_c::composition_state_e::epoch_start) {
        palette_updates_since_acq = 0;
    } else if (comp_state == pcs_c::composition_state_e::normal && insert_acqs > 0) {
        palette_updates_since_acq++;
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

    // Composite tracking (SUPer alpha_compo):
    // - fuse (reusable via SSIM): already updated via alpha_over in the SSIM block
    // - break/forced: reset composite to current trimmed bitmap
    // Computed before the emit decision because the group lookahead needs the
    // composite state left by this event.
    if (force_acquisition || (k < forced_acq.size() && forced_acq[k])) {
        // BREAK: new group starts with current event
        compo_rgba = trimmed_rgba;
        compo_w = obj_w;
        compo_h = obj_h;
        compo_x = ev.x() + crop_x;
        compo_y = ev.y() + crop_y;
        have_compo = true;
    } else if (!have_compo) {
        // First event: initialize composite
        compo_rgba = trimmed_rgba;
        compo_w = obj_w;
        compo_h = obj_h;
        compo_x = ev.x() + crop_x;
        compo_y = ev.y() + crop_y;
        have_compo = true;
    }
    // If reusable via SSIM, alpha_over already updated compo_rgba in the SSIM block
    // If byte-exact reusable: composite unchanged (deviation: position-free)

    // ── Mid-event acquisition candidate (SUPer render2.py:1003-1036) ──
    // Computed for both paths: inline insert (non-group events) or a marker
    // on the buffered record (flushed together with the group).
    bool acq_ok = false;
    epoch_timings_t acq_timings;
    if (insert_acqs > 0 && palette_updates_since_acq > insert_acqs && !forced) {
        const double t_diff = ev.tc_out() - ev.tc_in();
        // Temporal margin: the event must be long enough to fit the
        // acquisition with decode room (SUPer: t_diff > 4.5*write_dur/FREQ).
        if (t_diff > 4.5 * timings.wipe_dur) {
            // Push PTS forward frame-by-frame to create decode margin
            // (SUPer: while dts < dts_end or pts < npts + wd/FREQ: tc_pts += 1).
            const double dts_end = timings.base_dts + timings.wipe_dur;
            const double target_dts_end = dts_end + 2.0 / pg_decoder_t::FREQ;
            const double target_pts = timings.base_pts + 2.0 / pg_decoder_t::FREQ;
            double pushed_pts = timings.base_pts;
            double pushed_dts = timings.base_dts;
            const double frame_dur = 1.0 / m_fps;
            int frame_added = 0;
            while (pushed_dts < target_dts_end ||
                   pushed_pts < target_pts + timings.wipe_dur) {
                pushed_pts += frame_dur;
                pushed_dts = pushed_pts - timings.decode_duration;
                frame_added++;
                if (pushed_pts >= ev.tc_out()) break;  // never exceed the event
            }
            // Guard: tight margin only, and don't eat more than half the event
            // (SUPer: dts - dts_end < 0.25 and frame_added <= (durs-1)>>1).
            const double dts_margin = pushed_dts - target_dts_end;
            const int max_frames = static_cast<int>((t_diff * m_fps - 1.0) / 2.0);
            if (dts_margin < 0.25 && frame_added <= max_frames) {
                acq_ok = true;
                acq_timings = compute_timings(pushed_pts,
                    static_cast<uint64_t>(obj_w) * static_cast<uint64_t>(obj_h));
            }
        }
    }

    // ── Group lookahead (P4b) ──
    // Would the next event fuse into this composition? If so, current event
    // is a group member and its emission is deferred until the group ends.
    bool next_fuses = false;
    if (k + 1 < events.size() &&
        comp_state != pcs_c::composition_state_e::acquisition && have_compo) {
        auto& nev = events[k + 1];
        const bool nforced = (k + 1 < forced_acq.size() && forced_acq[k + 1]);
        if (!nforced) {
            auto nrgba = nev.load_image();
            if (!nrgba.empty()) {
                int ntw, nth, ntx, nty;
                std::vector<uint8_t> ntrim;
                trim_transparent_padding(nrgba, nev.width(), nev.height(),
                                          ntrim, ntw, nth, ntx, nty);
                const int naw = (ntw > 0) ? ntw : nev.width();
                const int nah = (nth > 0) ? nth : nev.height();
                const int nx = nev.x() + ((ntw > 0) ? ntx : 0);
                const int ny = nev.y() + ((nth > 0) ? nty : 0);
                if (nx == compo_x && ny == compo_y &&
                    naw == compo_w && nah == compo_h) {
                    double ncross = 0.0;
                    const double nssim = common::ssim_c::compare_with_alpha(
                        compo_rgba.data(), ntrim.data(), naw, nah, ncross);
                    const double nthr = std::min(1.0, effective_ssim_threshold +
                        (1.0 - effective_ssim_threshold) * (1.0 - ncross) -
                        0.008333 * (1.0 - ssim_offset));
                    next_fuses = nssim >= nthr;
                }
            }
        }
    }

    // ── Emission decision ──
    const bool is_member =
        comp_state != pcs_c::composition_state_e::acquisition &&
        (chain_active || next_fuses);
    if (is_member) {
        // Defer: this event flows into the group's union bitmap.
        chain_event_t rec;
        rec.rgba = trimmed_rgba;
        rec.w = obj_w;
        rec.h = obj_h;
        rec.crop_x = crop_x;
        rec.crop_y = crop_y;
        rec.ev_x = ev.x();
        rec.ev_y = ev.y();
        rec.ev_forced = ev.forced();
        rec.comp_state = comp_state;
        rec.obj_id = obj_id;
        rec.timings = timings;
        rec.has_clear = have_prev;
        rec.clear_x = prev_pos_x;
        rec.clear_y = prev_pos_y;
        rec.clear_w = prev_obj_w;
        rec.clear_h = prev_obj_h;
        rec.clear_pts = prev_tc_out;
        rec.refresh = (emit_reusable == false);  // SUPer nc_refresh marker
        rec.acq_insert = acq_ok;
        rec.acq_timings = acq_timings;
        rec.own_palette = palette;
        rec.own_indexed = indexed;
        if (acq_ok) {
            palette_updates_since_acq = 0;
            m_drought = 0.0;
        }
        chain.push_back(std::move(rec));
        chain_active = true;
    } else {
        if (chain_active) {
            flush_chain();
            chain_active = false;
        }
        // Full acquisition: clear DS before every non-first event.
        if (have_prev) {
            logger_c::instance().log(common::log_level_e::hdebug,
                "Clear DS at " + std::to_string(prev_tc_out) +
                " before event at " + std::to_string(ev.tc_in()));
            emit_clear_ds(prev_tc_out, prev_obj_w, prev_obj_h,
                          prev_pos_x, prev_pos_y);
        }
        // Emit the full display set for this event.
        event_emit_input_t in{
            obj_w, obj_h, crop_x, crop_y, ev.x(), ev.y(), ev.forced(),
            comp_state, obj_id, palette_id, emit_reusable, fps_enum,
            palette, indexed, timings};
        auto segs = emit_event_segments(in, result);
        for (auto& seg : segs)
            result.push_back(std::move(seg));
        if (acq_ok) {
            // Same object, same obj_id (re-acquire the current buffer).
            event_emit_input_t acq_in{
                obj_w, obj_h, crop_x, crop_y, ev.x(), ev.y(), ev.forced(),
                pcs_c::composition_state_e::acquisition, obj_id, palette_id,
                false /* emit ODS: fresh acquisition */, fps_enum,
                palette, indexed, acq_timings};
            auto acq_segs = emit_event_segments(acq_in, result);
            for (auto& seg : acq_segs)
                result.push_back(std::move(seg));
            palette_updates_since_acq = 0;
            m_drought = 0.0;
        }
    }

        prev_obj_id = obj_id;

        // Save event data for next iteration's clear DS and decode-margin check
        prev_tc_out = ev.tc_out();
        prev_pos_x = ev.x() + crop_x;
        prev_pos_y = ev.y() + crop_y;
        prev_obj_w = obj_w;
        prev_obj_h = obj_h;
        prev_base_pts = timings.base_pts;
        prev_tc_in = ev.tc_in();
        prev_wipe_dur = timings.wipe_dur;
        if (k == 0) epoch_write_dur = timings.decode_duration; // SUPer: write_duration of first node
        have_prev = true;

        // Keep prev_trimmed for byte-exact fast path
        prev_trimmed = trimmed_rgba;
        prev_trim_w = obj_w;
        prev_trim_h = obj_h;
    }

    // ── Final clear DS after the last event (SUPer: end of epoch wipe) ──
    if (chain_active) {
        flush_chain();
        chain_active = false;
    }
    if (have_prev) {
        emit_clear_ds(prev_tc_out, prev_obj_w, prev_obj_h, prev_pos_x, prev_pos_y);
        logger_c::instance().log(common::log_level_e::hdebug, "Final clear DS at " + std::to_string(prev_tc_out));
    }
    logger_c::instance().info("Encoded epoch with " + std::to_string(events.size()) +
                                " events, " + std::to_string(result.size()) + " segments.");
    return result;
}

} // namespace core
} // namespace opensup
