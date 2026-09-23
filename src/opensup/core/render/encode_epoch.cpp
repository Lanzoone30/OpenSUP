// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/core/render/epoch_encoder.h"
#include "opensup/media/pgraphics.h"
#include "opensup/common/logger.h"
#include "opensup/common/ssim.h"
#include "opensup/core/ctu.h"

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

// Identical regions earn the full discount; non-overlapping ones lower the
// threshold toward the base. The 0.008333 term (~0.5/60) biases against
// fusing near the threshold.
static double ctu_fuse_threshold(double base_threshold, double cross_percentage,
                                 double ssim_offset) noexcept
{
    return std::min(1.0, base_threshold +
        (1.0 - base_threshold) * (1.0 - cross_percentage) -
        0.008333 * (1.0 - ssim_offset));
}

acq_signals_t find_acqs_signals(double dts, double prev_dts_end,
                                double pts, double prev_pts,
                                double write_duration, double margin)
{
    // Decode starts after the previous decode end and the PTS gap exceeds the
    // epoch write duration.
    acq_signals_t sig;
    sig.valid = (dts > prev_dts_end) && (pts - prev_pts > write_duration);
    // Decode slack normalized by the previous node duration; invalid timings
    // map to -1 (the first node never reaches here).
    sig.dtl = sig.valid ? (dts - prev_dts_end) / margin : -1.0;
    return sig;
}

epoch_encoder_c::epoch_encoder_c(double fps, int width, int height, int quantizer_id,
                                 bool allow_normal_case, bool overlap,
                                 bool full_palette,
                                 bool prefer_normal_case,
                                 double quality_factor,
                                 double refresh_rate,
                                 double ssim_tol,
                                 int insert_acquisitions,
                                 bool alternate_oids)
    : m_fps(fps), m_width(width), m_height(height), m_quantizer_id(quantizer_id)
    , m_allow_normal_case(allow_normal_case), m_prefer_normal_case(prefer_normal_case)
    , m_overlap(overlap)
    , m_full_palette(full_palette)
    , m_alternate_oids(alternate_oids)
    , m_quality_factor(quality_factor)
    , m_refresh_rate(refresh_rate)
    , m_ssim_tol(ssim_tol)
    , m_insert_acquisitions(insert_acquisitions)
    , m_windows{} {}

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
    // Decode duration: full-screen composition rate (epoch-start model).
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

uint8_t
epoch_encoder_c::next_ods_vn(uint16_t o_id)
{
    if (m_ods_vn.size() <= o_id)
        m_ods_vn.resize(static_cast<size_t>(o_id) + 1, 0);
    return m_ods_vn[o_id]++;
}

uint8_t
epoch_encoder_c::window_for(int ev_x, int ev_y, int crop_x, int crop_y) const noexcept
{
    // Single-window mode (no layout windows): everything lives in window 0.
    if (m_windows.size() < 2) return 0;
    const int px = ev_x + crop_x;
    const int py = ev_y + crop_y;
    for (const auto& wd : m_windows) {
        if (px >= wd.h_pos && px < wd.h_pos + wd.width &&
            py >= wd.v_pos && py < wd.v_pos + wd.height)
            return wd.window_id;
    }
    return 0;
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
    obj.window_id = in.window_id;
    obj.h_pos = static_cast<uint16_t>(in.ev_x + in.crop_x);
    obj.v_pos = static_cast<uint16_t>(in.ev_y + in.crop_y);
    obj.flags = in.ev_forced ? c_object_t::forced : c_object_t::standard;
    cobjects.push_back(obj);
    // Normal case: the other window's object stays on screen as a CObject
    // reference without a new ODS. Its palette/image are the one already
    // decoded from that window's last ODS.
    if (in.normal_case_ref) {
        c_object_t ref;
        ref.o_id = in.ref_obj_id;
        ref.window_id = in.ref_window_id;
        ref.h_pos = static_cast<uint16_t>(std::max(0, in.ref_x));
        ref.v_pos = static_cast<uint16_t>(std::max(0, in.ref_y));
        ref.flags = c_object_t::standard;
        // The kept (id_skipped) CObject must be listed FIRST in the
        // composition: "the refreshed object has to come first". Always, with
        // or without alternate_oids. The reference never alternates (it uses
        // the on-screen oid).
        cobjects.insert(cobjects.begin(), ref);
    }

    // Per-segment timestamps (set_pts_dts_sc model)
    const auto& timings = in.timings;

    // PCS: PTS = presentation, DTS = decode start
    auto pcs = pcs_c::from_scratch(
        static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height),
        static_cast<uint8_t>(in.fps_enum.to_pcsfps()),
        static_cast<uint16_t>(m_composition_n++), in.comp_state,
        false, palette_id, cobjects, timings.base_pts, timings.base_dts);
    result.push_back(std::make_shared<pcs_c>(std::move(pcs)));

    // WDS: PTS = PCS_PTS - wipe_dur. Window comes from the epoch layout when
    // windows were provided; otherwise fall back to the object rectangle
    // (single-window byte-compatible behaviour).
    double wds_pts = timings.base_pts - timings.wipe_dur;
    if (wds_pts < timings.base_dts) wds_pts = timings.base_dts + 1.0 / (pg_decoder_t::FREQ * 2);
    std::vector<window_definition_t> wds_windows;
    if (in.window_id < m_windows.size()) {
        wds_windows.push_back(m_windows[in.window_id]);
    } else {
        window_definition_t wd;
        wd.window_id = in.window_id;
        wd.h_pos = static_cast<uint16_t>(in.ev_x + in.crop_x);
        wd.v_pos = static_cast<uint16_t>(in.ev_y + in.crop_y);
        wd.width = static_cast<uint16_t>(in.obj_w);
        wd.height = static_cast<uint16_t>(in.obj_h);
        wds_windows.push_back(wd);
    }
    auto wds = wds_c::from_scratch(wds_windows, wds_pts, timings.base_dts);
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

        // Skipped for reused events — the object is already decoded.
        double ods_pts = timings.base_dts + timings.obj_decode_time;
        auto rle_data = encode_rle(in.indexed, in.obj_w, in.obj_h);
        auto ods_list = ods_c::from_scratch(in.obj_id, next_ods_vn(in.obj_id),
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
                               int& palette_id_counter,
                               const std::vector<window_definition_t>& windows)
{
    // The order of operations below is the byte-exact semantics of the
    // stream; do not reorder.
    std::vector<std::shared_ptr<pg_segment_c>> result;
    if (events.empty()) return result;

    m_ods_vn.clear();
    // Use provided windows or default to single full-screen window
    // Layout windows for this epoch (may be empty: single-window emission
    // then falls back to per-object windows below).
    m_windows = windows;
    // Overlap + two windows: acquisitions merge into the previous window's
    // nodes, so the whole epoch must be planned before emission (two-phase).
    // Default and single-window overlap keep the single-pass path below
    // (byte-frozen by FR-6).
    if (m_overlap && m_windows.size() == 2)
        return encode_epoch_overlap(events, redraw_flags, fps_enum,
                                    palette_id_counter);
    // redraw_flags marks forced ACQUISITION points (periodic refresh)
    const std::vector<bool>& forced_acq = redraw_flags;

    auto palette_id = static_cast<uint8_t>(palette_id_counter % 8);
    palette_id_counter++;

    // For single window: double_buffer = 1 (alternates 0/1)
    // For multi-window: more complex alternation; kept simple here.
    int double_buffer = 1;
    // Per-window slot sizing and new-object mask: max (dy,dx) seen per window
    // and which window got new content per event. Consumed by F2.
    const size_t nwin = m_windows.empty() ? 1 : m_windows.size();
    // Per-window oid alternation (double buffering), only active with
    // m_alternate_oids && 2+ windows.
    std::vector<int> double_buffer_db(nwin, static_cast<int>(nwin));
    std::vector<std::pair<int, int>> min_boxes(nwin, {0, 0});
    std::vector<uint8_t> new_mask(nwin, 0);

    // Track previous event for clear DS insertion
    double prev_tc_out = 0.0;
    int prev_pos_x = 0, prev_pos_y = 0, prev_obj_w = 0, prev_obj_h = 0;
    bool have_prev = false;
    int prev_window_id = 0; // layout window of the previous event's object

    // Previous event timing (dts/dts_end/margin) for the decode-margin
    // acquisition decision. pts_gap and dtl are computed per event.
    double prev_base_pts = 0.0;   // previous event's PTS (tc_in)
    double prev_tc_in = 0.0;      // previous event's tc_in (display start)
    double prev_wipe_dur = 0.0;   // previous event's object wipe duration
    double epoch_write_dur = 0.0; // first event's full-screen decode duration (write_duration)

    // Previous trimmed bitmap (same epoch) and object id of the last ODS —
    // reused by following events with identical content.
    std::vector<uint8_t> prev_trimmed;
    int prev_trim_w = 0, prev_trim_h = 0;
    uint16_t prev_obj_id = 0;

    // Composite bitmap for the current group.
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

    // m_quality_factor = compression/100 (0 = force all ACQUISITION)
    // m_dquality_factor = 0.035 (drought decay factor)
    // m_refresh_rate = acqrate/100 (scales drought increment)
    // m_ssim_tol = ssim_tol/100 (adjusts threshold per resolution)
    // m_insert_acquisitions = extra_acq (min palette updates to force acq)
    const double thresh = m_quality_factor;              // quality_factor
    const double refresh_rate = m_refresh_rate;          // acqrate/100
    const double ssim_offset = 0.014 * std::clamp(m_ssim_tol, -1.0, 1.0);  // ssim_tol adjustment
    const int insert_acqs = m_insert_acquisitions;       // extra_acq

    // Resolution-dependent base SSIM threshold:
    // min(0.9999, 0.9608 + height*(0.986-0.972)/(1080-480)) — height maps
    // 480->0.972 and 1080->0.986; no offset subtraction.
    const double base_ssim_threshold = std::min(0.9999, 0.9608 +
        static_cast<double>(m_height) * (0.986 - 0.972) / (1080.0 - 480.0));

    m_drought = 0.0;

    // Track palette updates since last acquisition for extra_acq logic
    int palette_updates_since_acq = 0;

    // Group buffering (P4b): a run of SSIM-fused / byte-identical events sharing one composition is
    // buffered and co-quantized at group end: 1 union ODS + per-event palette
    // diffs. Before P4b, fused events emitted no bitmap at all, so fades were
    // invisible.
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
                // Same gap-only clear rule as the main path.
                if (e.has_clear && (!m_overlap || e.clear_pts < e.timings.base_pts))
                    emit_clear_ds(e.clear_pts, e.clear_w, e.clear_h, e.clear_x, e.clear_y);
                event_emit_input_t in{e.w, e.h, e.crop_x, e.crop_y, e.ev_x, e.ev_y,
                                      e.ev_forced, e.comp_state, e.obj_id, window_for(e.ev_x, e.ev_y, e.crop_x, e.crop_y), palette_id,
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
            // The undisplay clear is emitted only for gap (wipe) nodes —
            // "zero unless there are no PG objects shown at some point in the
            // epoch". Contiguous updates replace/refresh the composition with
            // no clear (no blinking). In overlap mode the chain follows that
            // rule: the clear is emitted only when a real gap precedes the
            // event.
            if (e.has_clear && (!m_overlap || e.clear_pts < e.timings.base_pts))
                emit_clear_ds(e.clear_pts, e.clear_w, e.clear_h, e.clear_x, e.clear_y);
            if (i == 0) {
                // Group start: union bitmap + full palette.
                event_emit_input_t in{e.w, e.h, e.crop_x, e.crop_y, e.ev_x, e.ev_y,
                                      e.ev_forced, e.comp_state, e.obj_id, window_for(e.ev_x, e.ev_y, e.crop_x, e.crop_y), palette_id,
                                      false, fps_enum, sol.palettes[0], sol.bitmap,
                                      e.timings};
                auto segs = emit_event_segments(in, result);
                for (auto& seg : segs) result.push_back(std::move(seg));
            } else {
                // Palette update event: PCS/WDS/ENDS as today + diff PDS.
                event_emit_input_t in{e.w, e.h, e.crop_x, e.crop_y, e.ev_x, e.ev_y,
                                      e.ev_forced, e.comp_state, e.obj_id, window_for(e.ev_x, e.ev_y, e.crop_x, e.crop_y), palette_id,
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
            // composition late can re-acquire it.
            if (e.refresh && i > 0) {
                double ods_pts = e.timings.base_dts + e.timings.obj_decode_time;
                auto rle = encode_rle(sol.bitmap, e.w, e.h);
                auto ods_list = ods_c::from_scratch(e.obj_id, next_ods_vn(e.obj_id),
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
                                          e.obj_id, window_for(e.ev_x, e.ev_y, e.crop_x, e.crop_y), palette_id, false, fps_enum,
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
        // The drought effect belongs to the decode-margin check, and ssim_offset
        // applies ONLY inside thr_score (-0.008333*(1-ssim_offset)), never to
        // the base threshold.
        const double effective_ssim_threshold = base_ssim_threshold;

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

        // Reuse is position-free: a PCS can move the object without a new ODS.
        bool reusable = (obj_w == prev_trim_w && obj_h == prev_trim_h &&
                         trimmed_rgba == prev_trimmed);

        // SSIM comparison vs accumulated composite
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
            // CTU: recursive area-weighted SSIM with a 0.325 discount for
            // identical regions.
            const auto [ctu_score, ctu_cross] = core::ctu_c::evaluate(compo_rgba, trimmed_rgba, obj_w, obj_h);
            ssim_score = ctu_score;
            cross_percentage = ctu_cross;

            // Similarity threshold: identical regions get the full discount,
            // non-overlapping ones lower it toward the base threshold.
            double thr_score = ctu_fuse_threshold(effective_ssim_threshold,
                                                  cross_percentage, ssim_offset);

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

        // Per-segment timestamps. Computed here so the drought decision below
        // can use the decode-margin (acq/dtl) signals.
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

    pcs_c::composition_state_e comp_state;
    bool emit_reusable = reusable; // refresh acquisition re-emits the ODS
    // Layout window this event's object targets (multi-window Fase 1).
    const uint8_t cur_window = window_for(ev.x(), ev.y(), crop_x, crop_y);
    // Slot sizing and new-object mask: the window slot grows to the max object
    // bbox seen; a non-reusable event brings new content to its window
    // (new_mask drives the normal-case filter).
    {
        auto& mb = min_boxes[cur_window];
        mb.first = std::max(mb.first, obj_h);
        mb.second = std::max(mb.second, obj_w);
        std::fill(new_mask.begin(), new_mask.end(), 0);
        new_mask[cur_window] = reusable ? 0 : 1;
    }

    // The two-window normal case below redefines one window as NORMAL; the
    // single-window analog is `reusable`, which also drives the NORMAL state.

    // Decode-margin signals, ported via find_acqs_signals(). Margin is the
    // previous node duration: the inter-event gap (wipe node) when one exists,
    // else the previous event duration. Object-specific decode time (RD read +
    // RC write) is the dts numerator; the emitted base_dts keeps the
    // conservative full-screen model.
    bool acq_valid = false;
    double dtl = -1.0;
    bool nc_margin = false; // normal case: new object decodes after the previous one
    if (have_prev) {
        const double obj_decode_dur = timings.obj_decode_time + timings.wipe_dur;
        const double dts_eff = timings.base_pts - obj_decode_dur;
        const double prev_dts_end = prev_base_pts - prev_wipe_dur;
        const double gap = ev.tc_in() - prev_tc_out;
        const double margin = (gap > 0.0) ? gap : (prev_tc_out - prev_tc_in);
        const acq_signals_t sig = find_acqs_signals(
            dts_eff, prev_dts_end, timings.base_pts, prev_base_pts,
            epoch_write_dur, margin);
        acq_valid = sig.valid;
        dtl = sig.dtl;
        nc_margin = (dts_eff > prev_dts_end);
    }

    // Normal case: when the epoch has two layout windows and the new event
    // takes over a different window while the previous object is still on
    // screen, this display set may redefine only that window as NORMAL instead
    // of restarting the whole composition. Requires allow_normal_case and
    // enough decode margin (dts_start_nc > dts_start, the nc_margin signal
    // below). The other window's object stays on screen as a CObject reference.
    const bool normal_case =
        m_allow_normal_case && m_windows.size() == 2 && have_prev &&
        ev.tc_in() < prev_tc_out && cur_window != prev_window_id &&
        nc_margin;
    comp_state = (normal_case || reusable)
        ? pcs_c::composition_state_e::normal
        : pcs_c::composition_state_e::epoch_start;
    // Drought logic:
    // if (forced or (acq and margin > max(thresh - dthresh*drought, 0))) and
    //    NOT nc_refresh -> ACQUISITION
    // else -> drought += refresh_rate
    // Reusable palette-update nodes NEVER become ACQUISITION; with enough
    // decode margin they only re-emit the ODS (stay NORMAL).
    if (thresh == 0.0 && !reusable && !normal_case) {
        // compression=0 -> force all ACQUISITION
        comp_state = pcs_c::composition_state_e::acquisition;
        m_drought = 0.0;
    } else if (comp_state != pcs_c::composition_state_e::acquisition) {
        if (normal_case) {
            // A NORMAL-case winner is never downgraded to ACQUISITION.
        } else if (forced || force_acquisition) {
            // force_acquisition (SSIM break) consolidates the absolute marker:
            // a new object popped in a window — both force ACQUISITION and
            // reset the drought.
            comp_state = pcs_c::composition_state_e::acquisition;
            m_drought = 0.0;
        } else if (reusable && acq_valid &&
                   dtl > std::max(thresh - m_dquality_factor * m_drought, 0.0)) {
            // Refresh: same object but enough decode margin — re-send the ODS
            // so late-starting decoders can re-sync, yet stay NORMAL (a
            // palette-update node, not a group restart).
            emit_reusable = false;
            m_drought = 0.0;
        } else {
            // Prevent excessive acquisitions, as we want to compress the stream
            m_drought += 1.0 * refresh_rate;
            if (comp_state == pcs_c::composition_state_e::normal) {
                // NC-refresh candidate: kept NORMAL; only logged for now.
            }
        }
    }

    // Extra acquisition counter. Counts NORMAL events (palette updates) since
    // the last acquisition; EPOCH_START and ACQUISITION both start a new group
    // (reset). The mid-event acquisition itself is inserted after the display
    // set below.
    if (comp_state == pcs_c::composition_state_e::acquisition ||
        comp_state == pcs_c::composition_state_e::epoch_start) {
        palette_updates_since_acq = 0;
    } else if (comp_state == pcs_c::composition_state_e::normal && insert_acqs > 0) {
        palette_updates_since_acq++;
    }

        // Object ID double buffering (alternates 0/1 for single window).
        // Reused events keep the previous object id (already decoded).
        uint16_t obj_id;
        if (reusable) {
            obj_id = prev_obj_id;
        } else if (m_alternate_oids && m_windows.size() >= 2) {
            // Per-window double buffering: each window alternates its own oid
            // so the decoder never rewrites the buffer being presented.
            int& dbw = double_buffer_db[static_cast<size_t>(cur_window)];
            dbw = static_cast<int>(std::abs(static_cast<long long>(m_windows.size()) - dbw));
            obj_id = static_cast<uint16_t>(cur_window + dbw);
        } else {
            double_buffer = 1 - double_buffer;
            obj_id = static_cast<uint16_t>(double_buffer);
        }

    // Composite tracking:
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

    // Computed for both paths: inline insert (non-group events) or a marker
    // on the buffered record (flushed together with the group).
    bool acq_ok = false;
    epoch_timings_t acq_timings;
    if (insert_acqs > 0 && palette_updates_since_acq > insert_acqs && !forced) {
        const double t_diff = ev.tc_out() - ev.tc_in();
        // Temporal margin: the event must be long enough to fit the
        // acquisition with decode room (t_diff > 4.5*write_dur/FREQ).
        if (t_diff > 4.5 * timings.wipe_dur) {
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
            // (dts - dts_end < 0.25 and frame_added <= (durs-1)>>1).
            const double dts_margin = pushed_dts - target_dts_end;
            const int max_frames = static_cast<int>((t_diff * m_fps - 1.0) / 2.0);
            if (dts_margin < 0.25 && frame_added <= max_frames) {
                acq_ok = true;
                acq_timings = compute_timings(pushed_pts,
                    static_cast<uint64_t>(obj_w) * static_cast<uint64_t>(obj_h));
            }
        }
    }

    // Group lookahead (P4b): would the next event fuse into this composition?
    // If so, current event is a group member and its emission is deferred
    // until the group ends.
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
                    // CTU for P4b lookahead
                    const auto [nctu_score, nctu_cross] = core::ctu_c::evaluate(compo_rgba, ntrim, naw, nah);
                    const double nthr = ctu_fuse_threshold(effective_ssim_threshold,
                                                           nctu_cross, ssim_offset);
                    next_fuses = nctu_score >= nthr;
                }
            }
        }
    }

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
        rec.refresh = (emit_reusable == false);  // Reusable palette-update marker
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
        // The mid-stream clear is only written when the previous node belongs
        // to a chain, which is already handled by flush_chain()'s has_clear
        // path. An independent acquisition (no parent) replaces the composition
        // outright, so no clear display set precedes it. The end-of-epoch clear
        // below remains.
        // Emit the full display set for this event.
        event_emit_input_t in{
            obj_w, obj_h, crop_x, crop_y, ev.x(), ev.y(), ev.forced(),
            comp_state, obj_id, window_for(ev.x(), ev.y(), crop_x, crop_y), palette_id, emit_reusable, fps_enum,
            palette, indexed, timings};
        in.normal_case_ref = normal_case;
        in.ref_window_id = static_cast<uint8_t>(prev_window_id);
        in.ref_obj_id = prev_obj_id;
        in.ref_x = prev_pos_x;
        in.ref_y = prev_pos_y;
        auto segs = emit_event_segments(in, result);
        for (auto& seg : segs)
            result.push_back(std::move(seg));
        if (acq_ok) {
            // Same object, same obj_id (re-acquire the current buffer).
            event_emit_input_t acq_in{
                obj_w, obj_h, crop_x, crop_y, ev.x(), ev.y(), ev.forced(),
                pcs_c::composition_state_e::acquisition, obj_id, window_for(ev.x(), ev.y(), crop_x, crop_y), palette_id,
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
        prev_window_id = cur_window;

        // Save event data for next iteration's clear DS and decode-margin check
        prev_tc_out = ev.tc_out();
        prev_pos_x = ev.x() + crop_x;
        prev_pos_y = ev.y() + crop_y;
        prev_obj_w = obj_w;
        prev_obj_h = obj_h;
        prev_base_pts = timings.base_pts;
        prev_tc_in = ev.tc_in();
        prev_wipe_dur = timings.wipe_dur;
        if (k == 0) epoch_write_dur = timings.decode_duration; // First node's write duration
        have_prev = true;

        // Keep prev_trimmed for byte-exact fast path
        prev_trimmed = trimmed_rgba;
        prev_trim_w = obj_w;
        prev_trim_h = obj_h;
    }

    // Final clear DS after the last event (end of epoch wipe)
    if (chain_active) {
        flush_chain();
        chain_active = false;
    }
    if (have_prev) {
        emit_clear_ds(prev_tc_out, prev_obj_w, prev_obj_h, prev_pos_x, prev_pos_y);
        logger_c::instance().log(common::log_level_e::hdebug, "Final clear DS at " + std::to_string(prev_tc_out));
    }
    // align_palette_updates (overlap mode): buffered palette updates must
    // keep a strictly monotonic DTS: a display
    // set may never start decoding before the previous one finished. Shifting
    // puts each PU DTS into (prev_dts_end+1 tick, next_dts); the same +1-tick
    // rule applies over the emitted stream, pushing both timestamps of an
    // offending segment forward (frame by frame if needed).
    if (m_overlap) {
        double last_dts = -1.0;
        for (auto& seg : result) {
            if (seg->dts() <= last_dts) {
                const double nd = last_dts + 1.0 / pg_decoder_t::FREQ;
                seg->set_dts(nd);
                if (seg->pts() < nd)
                    seg->set_pts(nd);
                last_dts = nd;
            } else {
                last_dts = seg->dts();
            }
        }
    }
    // Slot sizes accumulated over the epoch (find_acqs min_boxes) — diagnostic
    // parity aid for F2 (specs/007) slot-based decode timing.
    for (size_t w = 0; w < min_boxes.size(); ++w) {
        logger_c::instance().log(common::log_level_e::hdebug,
            "min_boxes w" + std::to_string(w) + " dy=" + std::to_string(min_boxes[w].first) +
            " dx=" + std::to_string(min_boxes[w].second));
    }
    logger_c::instance().info("Encoded epoch with " + std::to_string(events.size()) +
                                " events, " + std::to_string(result.size()) + " segments.");
    return result;
}
} // namespace core
} // namespace opensup
