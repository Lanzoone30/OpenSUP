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

/// Per-event PGS timestamps derived from the BD decode-rate model.
struct epoch_timings_t {
    double base_pts = 0.0;        ///< Presentation time of the event.
    double base_dts = 0.0;        ///< Decode time (PTS - screen compose duration).
    double obj_decode_time = 0.0; ///< Object decode duration (RD rate).
    double wipe_dur = 0.0;        ///< Object compose/wipe duration (RC rate).
    double decode_duration = 0.0; ///< Full-screen compose duration (RC rate).
};

/// Per-event decode-margin signals (SUPer find_acqs, render2.py:1073-1135).
struct acq_signals_t {
    bool valid = false;    ///< valid[k] (render2.py:1132): the object decodes after the
                           ///< previous decode end and the PTS gap exceeds the epoch
                           ///< write duration.
    double dtl = -1.0;     ///< dtl[k] (render2.py:1133): decode slack normalized by the
                           ///< previous node duration (margin = prev_dt/fps, :1125);
                           ///< -1 when the timing is invalid.
    bool absolute = false; ///< absolutes[k] (render2.py:1119): a new object appeared
                           ///< in a window (C++ analog: SSIM break / force_acquisition).
};

/// Per-event node for the overlap pipeline (SUPer DSNode analog).
/// Carries the information needed for shift_forward_overlay and
/// set_pgobjects_extended_visibilities before emission.
struct epoch_node_t {
    // Decision signals (SUPer DSNode / find_acqs)
    int window_id = 0;                ///< Layout window this event's object occupies.
    bool has_object = false;          ///< This event has an object (non-wipe).
    bool new_mask = false;            ///< New object in this window (SUPer new_mask per window).
    bool absolute = false;            ///< absolutes[k]: forced acquisition (SSIM break / redraw).
    bool acq = false;                 ///< acqs[k]: valid acquisition candidate (margin OK).
    bool nc_refresh = false;          ///< SUPer nc_refresh marker (reusable palette update).
    pcs_c::composition_state_e state = pcs_c::composition_state_e::epoch_start;
    epoch_timings_t timings;          ///< Per-segment timestamps.
    double dts_end = 0.0;             ///< dts + wipe_dur (decode end, SUPer dts_end()).
    bool forced = false;              ///< Event forced-subtitle flag.

    // Emission payload (object bitmap + derived data)
    std::vector<uint8_t> rgba;        ///< Trimmed frame (raw, for ODS/co-quant).
    int w = 0, h = 0;                 ///< Trimmed dimensions.
    int crop_x = 0, crop_y = 0;       ///< Crop offset.
    int ev_x = 0, ev_y = 0;           ///< Event position.
    uint16_t obj_id = 0;              ///< Object id (double-buffering/reuse aware).
    media::palette_t palette;         ///< Quantized palette (transparent offset applied).
    std::vector<uint8_t> indexed;     ///< Quantized indexed pixels.
    bool reusable = false;            ///< Reuses the previous event's object.
    bool has_clear = false;           ///< Previous-event clear DS params (US1).
    int clear_x = 0, clear_y = 0, clear_w = 0, clear_h = 0;
    double clear_pts = 0.0;

    // Shift-forward context:
    int pad_left = 0;                 ///< Nodes the acquisition was merged left.
    bool shifted = false;             ///< This node absorbed an acquisition from a later node.
};

/// Decode-margin computation of find_acqs (render2.py:1122-1133), adapted to
/// the object-based decode model (dts = pts - object decode+write; the SUPer
/// node/slot numerator is a documented ceiling, specs/006 plan §Riesgos).
/// @param dts decode start of this event's object.
/// @param prev_dts_end decode end of the previous event's object.
/// @param pts presentation time of this event.
/// @param prev_pts presentation time of the previous event.
/// @param write_duration epoch write duration (first event, DSNode.write_duration).
/// @param margin previous node duration in seconds (render2.py:1122-1125):
///        the inter-event gap when one exists (wipe node), else the previous
///        event duration.
[[nodiscard]] acq_signals_t find_acqs_signals(double dts, double prev_dts_end,
                                              double pts, double prev_pts,
                                              double write_duration, double margin);

/// Inputs to emit one event's display set (PCS/WDS/PDS/ODS/ENDS).
struct event_emit_input_t {
    int obj_w = 0, obj_h = 0;     ///< Trimmed object dimensions.
    int crop_x = 0, crop_y = 0;   ///< Crop offset applied to the event position.
    int ev_x = 0, ev_y = 0;       ///< Event position on the tile grid.
    bool ev_forced = false;       ///< Event forced-subtitle flag.
    pcs_c::composition_state_e comp_state = pcs_c::composition_state_e::epoch_start;
    uint16_t obj_id = 0;          ///< Object id (double-buffering/reuse aware).
    uint8_t window_id = 0;        ///< Window ID for this object (0 or 1).
    uint8_t palette_id = 0;       ///< Palette ID for this event.
    bool reusable = false;        ///< Event reuses the previous event's object.
    common::fps_e fps_enum;       ///< Source frame rate (for the PCS fps byte).
    const media::palette_t& palette;     ///< Ready-to-emit palette (transparent offset applied).
    const std::vector<uint8_t>& indexed; ///< Ready-to-emit indexed pixels.
    const epoch_timings_t& timings;      ///< Per-segment timestamps.
    // Normal-case refresh: also re-emit the kept other-window object as a
    // CObject reference without a new ODS. Defaults keep the classic layout
    // and are only set by the caller that computes `normal_case`.
    bool normal_case_ref = false; ///< Emit the reference CObject too.
    uint8_t ref_window_id = 0;    ///< Window ID of the kept object.
    uint16_t ref_obj_id = 0;      ///< Object id of the kept object (its last ODS).
    int ref_x = 0, ref_y = 0;     ///< Position of the kept object.
};

// ── Epoch Encoder ──
/**
 * @brief Renders one epoch (a run of consecutive events) into PGS segments.
 *
 * Handles image composition, quantization, palette assignment and the
 * object-reuse optimization between consecutive identical events.
 */
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

    /// Render the given events; returns the PGS segments for this epoch.
    /// @param windows Window definitions for this epoch (1 or 2 windows). 
    ///                If empty, defaults to single full-screen window.
    std::vector<std::shared_ptr<pg_segment_c>>
    encode_epoch(const std::vector<bdn_xml_event_c>& events,
                  const std::vector<bool>& redraw_flags,
                  common::fps_e fps_enum,
                  int& palette_id_counter,
                  const std::vector<window_definition_t>& windows = {});

    /// Number of events detected as reusable (same bitmap as previous event).
    [[nodiscard]] int reuse_candidates() const noexcept { return m_reuse_candidates; }

private:
    /// Quantize an RGBA bitmap into palette + indexed pixels, preferring
    /// m_quantizer_id and falling back to another backend when it fails.
    /// Returns false when no backend produced a usable result.
    [[nodiscard]] bool quantize_image(const std::vector<uint8_t>& rgba, int width, int height,
                                      media::palette_t& out_palette,
                                      std::vector<uint8_t>& out_indexed) const;

    /// Compute PGS timestamps for one event from its presentation time and
    /// object area using the BD screen/object decode-rate model.
    [[nodiscard]] epoch_timings_t compute_timings(double base_pts, uint64_t area) const;

    /// Emit one event's full display set (PCS/WDS/PDS/ODS/ENDS) as segments.
    /// Mutates m_composition_n and m_palette_vn. reuses the overlap PDS timings
    /// from result_so_far (previous event's ENDS) when m_overlap is active.
    std::vector<std::shared_ptr<pg_segment_c>>
    emit_event_segments(const event_emit_input_t& in,
                        const std::vector<std::shared_ptr<pg_segment_c>>& result_so_far);

    double m_fps;
    int m_width, m_height;
    int m_quantizer_id = 0;
    // Normal-case flags: SUPer's normal case requires two windows (redefine one
    // while keeping the other).
    bool m_allow_normal_case = false;
    bool m_prefer_normal_case = false;
    bool m_overlap = false;
    bool m_full_palette = false;
    bool m_alternate_oids = false;       // SUPer double_buffering[wid] (render2.py:749-750)
    double m_quality_factor = 0.8;       // compression/100 (0 = force all ACQUISITION)
    double m_dquality_factor = 0.035;    // drought decay factor (original default)
    double m_refresh_rate = 1.0;         // acqrate/100 (scales drought)
    double m_ssim_tol = 0.0;             // ssim_tol/100 (adjusts threshold per resolution)
    int m_insert_acquisitions = 2;       // extra_acq (min palette updates to force acq)
    double m_drought = 0.0;
    int m_composition_n = 1;
    int m_palette_vn = 0;
    int m_reuse_candidates = 0;
    std::vector<window_definition_t> m_windows;  // Window definitions for this epoch
    /// Resolve the window owning the point (ev_x+crop_x, ev_y+crop_y).
    /// 0 when no layout windows are provided (single-window mode).
    [[nodiscard]] uint8_t window_for(int ev_x, int ev_y, int crop_x, int crop_y) const noexcept;
    // F2: overlap pipeline pre-passes (SUPer render2.py:315-408, 408-460)
    void shift_forward_overlay(std::vector<epoch_node_t>& nodes,
                                const std::vector<bool>& forced_acq) const;
    void set_extended_visibilities(std::vector<epoch_node_t>& nodes) const;
    // Two-phase encoding for overlap + 2 windows: build nodes, run the
    // overlap pre-passes, then emit from the prepared nodes.
    std::vector<std::shared_ptr<pg_segment_c>>
    encode_epoch_overlap(const std::vector<bdn_xml_event_c>& events,
                         const std::vector<bool>& redraw_flags,
                         common::fps_e fps_enum,
                         int& palette_id_counter);
    // Emit the full epoch from the prepared nodes (post overlap pre-passes).
    std::vector<std::shared_ptr<pg_segment_c>>
    emit_epoch_from_nodes(const std::vector<epoch_node_t>& nodes,
                          common::fps_e fps_enum,
                          int& palette_id_counter);
};

} // namespace core
} // namespace opensup
