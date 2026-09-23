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

#include "opensup/core/segments.h"
#include "opensup/core/filestreams.h"
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
    bool has_clear = false;    // previous-event clear DS params (US1)
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

/// Renders one epoch (a run of consecutive events) into PGS segments.
class epoch_encoder_c {
public:
    epoch_encoder_c(double fps, int width, int height, int quantizer_id = 0,
                     bool allow_normal_case = false, bool overlap = false,
                     bool full_palette = false,
                     bool prefer_normal_case = false,
                     double quality_factor = 0.8,
                     double refresh_rate = 1.0,
                     double ssim_tol = 0.0,
                     int insert_acquisitions = 2,
                     bool alternate_oids = false);

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
                        const std::vector<std::shared_ptr<pg_segment_c>>& result_so_far);

    uint8_t next_ods_vn(uint16_t o_id);

    double m_fps;
    int m_width, m_height;
    int m_quantizer_id = 0;
    // Normal case needs two windows: it redefines one, keeps the other's object up.
    bool m_allow_normal_case = false;
    // Reserved for the faithful prefer_normal_case implementation
    // (specs/013-prefer-normal-case). Stored but not yet read: without the SUPer
    // j/j_nc decode-node chain there is no tie to break, so allow alone already
    // behaves as prefer-always for the modelled cases.
    [[maybe_unused]] bool m_prefer_normal_case = false;
    bool m_overlap = false;
    bool m_full_palette = false;
    bool m_alternate_oids = false;
    double m_quality_factor = 0.8;   // compression/100 (0 = force all ACQUISITION)
    double m_dquality_factor = 0.035; // drought decay factor
    double m_refresh_rate = 1.0;     // acqrate/100 (scales drought)
    double m_ssim_tol = 0.0;         // ssim_tol/100 (per-resolution threshold adjust)
    int m_insert_acquisitions = 2;   // extra_acq (min palette updates to force acq)
    double m_drought = 0.0;
    int m_composition_n = 1;
    int m_palette_vn = 0;
    std::vector<uint8_t> m_ods_vn;   // per-oid ODS version counter
    int m_reuse_candidates = 0;
    std::vector<window_definition_t> m_windows;

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

} // namespace core
} // namespace opensup
