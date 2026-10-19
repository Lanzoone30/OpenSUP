// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <algorithm>
#include <map>

#include "opensup/core/segments.h"
#include "opensup/core/filestreams.h"
#include "opensup/core/render/sequence_quality_policy.h"
#include "opensup/media/optimizer.h"
#include "opensup/media/palette.h"

namespace opensup {
namespace core {

/// PGS timestamps for one event, per the BD decode-rate model.
struct epoch_timings_t {
    double base_pts = 0.0;
    double base_dts = 0.0;        // PTS - screen compose duration
    double obj_decode_time = 0.0; // object decode duration, RD rate
    double wipe_dur = 0.0;        // object compose/wipe duration, RC rate
    double decode_duration = 0.0; // full-screen compose duration, RC rate
};

/// Decode-margin signals from find_acqs.
struct acq_signals_t {
    bool valid = false;    // object decodes after prev decode end and the PTS gap
                           // exceeds the epoch write duration
    double dtl = -1.0;     // decode slack / previous node duration; -1 = invalid timing
    bool absolute = false; // new object in a window (SSIM break / forced acquisition)
};

/// Per-event node for the overlap pipeline. Carries the state consumed by
/// shift_forward_overlay and set_extended_visibilities before emission.
struct epoch_node_t {
    int window_id = 0;
    bool has_object = false;  // false for a wipe (no object this event)
    bool new_mask = false;    // new object in this window
    bool absolute = false;    // forced acquisition (SSIM break / redraw)
    bool acq = false;         // valid acquisition candidate (decode margin OK)
    bool nc_refresh = false;  // reusable palette update (no new object)
    pcs_c::composition_state_e state = pcs_c::composition_state_e::epoch_start;
    epoch_timings_t timings;
    double dts_end = 0.0;     // dts + wipe_dur
    double tc_out = 0.0;      // event display end; end-of-epoch clear PTS
    bool forced = false;

    // Normal-case takeover: conserve the other window's still-displayed object
    // as a reference CObject without a new ODS, mirroring event_emit_input_t.
    bool normal_case_ref = false;
    uint8_t ref_window_id = 0;
    uint16_t ref_obj_id = 0;  // last ODS of the kept object
    int ref_x = 0, ref_y = 0;

    std::vector<uint8_t> rgba; // trimmed frame (raw)
    int w = 0, h = 0;
    int crop_x = 0, crop_y = 0;
    int ev_x = 0, ev_y = 0;
    uint16_t obj_id = 0;
    media::palette_t palette;  // quantized, transparent offset applied
    std::vector<uint8_t> indexed;
    bool reusable = false;
    bool has_clear = false;    // previous-event clear DS params
    int clear_x = 0, clear_y = 0, clear_w = 0, clear_h = 0;
    double clear_pts = 0.0;

    int pad_left = 0;   // nodes the acquisition was merged left
    bool shifted = false; // this node absorbed an acquisition from a later node
};

/// Decode margin for one event under the object-based decode model
/// (dts = pts - object decode+write).
/// @param margin previous node duration in seconds: the inter-event gap when
///        one exists (wipe node), else the previous event duration.
[[nodiscard]] acq_signals_t find_acqs_signals(double dts, double prev_dts_end,
                                              double pts, double prev_pts,
                                              double write_duration, double margin);

/// Inputs to emit one event's display set (PCS/WDS/PDS/ODS/ENDS).
struct event_emit_input_t {
    int obj_w = 0, obj_h = 0;     // trimmed object dimensions
    int crop_x = 0, crop_y = 0;   // crop offset applied to the event position
    int ev_x = 0, ev_y = 0;       // event position on the tile grid
    bool ev_forced = false;       // event forced-subtitle flag
    pcs_c::composition_state_e comp_state = pcs_c::composition_state_e::epoch_start;
    uint16_t obj_id = 0;
    uint8_t window_id = 0;
    uint8_t palette_id = 0;
    bool reusable = false;
    common::fps_e fps_enum;              // for the PCS fps byte
    const media::palette_t& palette;     // transparent offset applied
    const std::vector<uint8_t>& indexed;
    const epoch_timings_t& timings;

    // Also re-emit the kept other-window object as a CObject reference without
    // a new ODS. Defaults keep the classic layout.
    bool normal_case_ref = false;
    uint8_t ref_window_id = 0;
    uint16_t ref_obj_id = 0; // last ODS of the kept object
    int ref_x = 0, ref_y = 0;
};

/// Encoder options for one epoch. Defaults here are the single source of truth.
struct epoch_render_options_t {
    double fps = 24.0;
    int width = 1920;
    int height = 1080;
    int quantizer_id = 3;  // SUPer ids: 2 = HexTree, 3 = LIQ (def)
    bool allow_normal_case = false;
    bool overlap = false;
    bool full_palette = false;
    bool prefer_normal_case = false;
    double quality_factor = 0.8;   // compression/100 (0 = force all ACQUISITION)
    double refresh_rate = 1.0;     // acqrate/100 (scales drought)
    double ssim_tol = 0.0;         // ssim_tol/100 (per-resolution threshold adjust)
    int insert_acquisitions = 2;   // extra_acq (min palette updates to force acq)
    bool alternate_oids = false;
    std::string bt_matrix = "bt709";
    bool is_last_epoch = false;
    int liq_quality = 95;
    int liq_speed = 4;
    int liq_dither_pct = 95;
};

/// Renders one epoch (a run of consecutive events) into PGS segments.
class epoch_encoder_c {
public:
    explicit epoch_encoder_c(const epoch_render_options_t& opts);

    /// An empty `windows` defaults to a single full-screen window.
    std::vector<std::shared_ptr<pg_segment_c>>
    encode_epoch(const std::vector<bdn_xml_event_c>& events,
                  const std::vector<bool>& redraw_flags,
                  common::fps_e fps_enum,
                  int& palette_id_counter,
                  const std::vector<window_definition_t>& windows = {});

    [[nodiscard]] int reuse_candidates() const noexcept { return m_reuse_candidates; }

private:
    /// Prefers m_quantizer_id, falls back to another backend on failure.
    /// Returns false when no backend produced a usable result.
    [[nodiscard]] bool quantize_image(const std::vector<uint8_t>& rgba, int width, int height,
                                      media::palette_t& out_palette,
                                      std::vector<uint8_t>& out_indexed) const;

    [[nodiscard]] epoch_timings_t compute_timings(double base_pts, uint64_t area) const;

    /// Mutates m_composition_n and m_palette_vn. Reuses the overlap PDS timings
    /// from result_so_far (previous event's ENDS) when m_overlap is active.
    std::vector<std::shared_ptr<pg_segment_c>>
    emit_event_segments(const event_emit_input_t& in,
                        const std::vector<std::shared_ptr<pg_segment_c>>& result_so_far,
                        epoch_timings_t* timing_out = nullptr);

    uint8_t next_ods_vn(uint16_t o_id);

    /// Palette-wipe clear: PCS NORMAL with palette_update_flag, an
    /// all-transparent PDS and END, instead of a WDS wipe. Emitted when the
    /// cleared window's redraw cannot fit the frame budget (large window or
    /// short last event); the screen blanks via a palette swap with no WDS.
    void emit_palette_wipe(std::vector<std::shared_ptr<pg_segment_c>>& out,
                           uint8_t pcsfps, double pts, double dts,
                           uint8_t palette_id);

    /// Emit a PDS for palette_id. Full palette when the id changes or when
    /// full-palette mode is active; otherwise a delta against the last palette
    /// sent for the same id.
    std::shared_ptr<pds_c> make_pds(const media::palette_t& palette,
                                    uint8_t palette_id, double pts, double dts);

    double m_fps;
    int m_width, m_height;
    int m_quantizer_id = 0;
    std::string m_bt_matrix = "bt709";
    // Normal case needs two windows: it redefines one, keeps the other's object up.
    bool m_allow_normal_case = false;
    // 032/D-032-5: flag + storage only. The faithful oracle rule
    // (render2.py:544, j_nc > j / prefer) needs the decode-node buffer chain,
    // which this encoder does not model; allow alone already behaves as
    // prefer-always for the modelled cases, so -k widens to a no-op here.
    [[maybe_unused]] bool m_prefer_normal_case = false;
    bool m_overlap = false;
    bool m_full_palette = false;
    bool m_alternate_oids = false;
    media::liq_options_t m_liq_options;
    sequence_quality_policy_c m_quality;
    int m_composition_n = 1;
    int m_palette_vn = 0;
    std::vector<uint8_t> m_ods_vn;   // per-oid ODS version counter
    int m_reuse_candidates = 0;
    std::vector<window_definition_t> m_windows;
    // The last epoch has no successor to bound it, so its end-of-epoch palette
    // swap may be followed by a WDS wipe a few frames later.
    bool m_is_last_epoch = false;
    // Objects of the last emitted display set; a palette-wipe re-references
    // them while its palette turns every entry transparent.
    std::vector<c_object_t> m_last_cobjs;

    /// Last palette sent per palette id, for delta-PDS generation.
    std::map<uint8_t, media::palette_t> m_palette_cache;
    uint8_t m_last_palette_id = 0xFF;

    /// Visible-area slot tracked per window, epoch-scoped.
    struct slot_t { int x = 0, y = 0, w = 0, h = 0; };
    std::map<uint8_t, slot_t> m_slots;

    /// Accumulate the union of rect (x0,y0)-(x1,y1) into window_id's slot,
    /// with a minimum extent of 8x8.
    void slot_touch(uint8_t window_id, int x0, int y0, int x1, int y1) {
        auto [it, inserted] = m_slots.try_emplace(
            window_id, slot_t{x0, y0, x1 - x0, y1 - y0});
        slot_t& s = it->second;
        if (!inserted) {
            const int rx = std::max(s.x + s.w, x1);
            const int by = std::max(s.y + s.h, y1);
            s.x = std::min(s.x, x0);
            s.y = std::min(s.y, y0);
            s.w = rx - s.x;
            s.h = by - s.y;
        }
        s.w = std::max(s.w, 8);
        s.h = std::max(s.h, 8);
    }

    friend struct epoch_flags_test_access_t;

    /// Window owning the point (ev_x+crop_x, ev_y+crop_y); 0 when no layout
    /// windows are provided (single-window mode).
    [[nodiscard]] uint8_t window_for(int ev_x, int ev_y, int crop_x, int crop_y) const noexcept;

    void shift_forward_overlay(std::vector<epoch_node_t>& nodes,
                                const std::vector<bool>& forced_acq) const;
    void set_extended_visibilities(std::vector<epoch_node_t>& nodes) const;

    /// Two-phase encoding for overlap + 2 windows: build nodes, run the overlap
    /// pre-passes, then emit from the prepared nodes.
    std::vector<std::shared_ptr<pg_segment_c>>
    encode_epoch_overlap(const std::vector<bdn_xml_event_c>& events,
                         const std::vector<bool>& redraw_flags,
                         common::fps_e fps_enum,
                         int& palette_id_counter);
    std::vector<std::shared_ptr<pg_segment_c>>
    emit_epoch_from_nodes(const std::vector<epoch_node_t>& nodes,
                          common::fps_e fps_enum,
                          int& palette_id_counter);
};

}
}
