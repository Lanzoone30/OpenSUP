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

/// Inputs to emit one event's display set (PCS/WDS/PDS/ODS/ENDS).
struct event_emit_input_t {
    int obj_w = 0, obj_h = 0;     ///< Trimmed object dimensions.
    int crop_x = 0, crop_y = 0;   ///< Crop offset applied to the event position.
    int ev_x = 0, ev_y = 0;       ///< Event position on the tile grid.
    bool ev_forced = false;       ///< Event forced-subtitle flag.
    pcs_c::composition_state_e comp_state = pcs_c::composition_state_e::epoch_start;
    uint16_t obj_id = 0;          ///< Object id (double-buffering/reuse aware).
    uint8_t palette_id = 0;       ///< Palette/window id for this event.
    bool reusable = false;        ///< Event reuses the previous event's object.
    common::fps_e fps_enum;       ///< Source frame rate (for the PCS fps byte).
    const media::palette_t& palette;     ///< Ready-to-emit palette (transparent offset applied).
    const std::vector<uint8_t>& indexed; ///< Ready-to-emit indexed pixels.
    const epoch_timings_t& timings;      ///< Per-segment timestamps.
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
                     int insert_acquisitions = 2);

    /// Render the given events; returns the PGS segments for this epoch.
    std::vector<std::shared_ptr<pg_segment_c>>
    encode_epoch(const std::vector<bdn_xml_event_c>& events,
                  const std::vector<bool>& redraw_flags,
                  common::fps_e fps_enum,
                  int& palette_id_counter);

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
    bool m_allow_normal_case = false;
    bool m_prefer_normal_case = false;
    bool m_overlap = false;
    bool m_full_palette = false;
    double m_quality_factor = 0.8;       // compression/100 (0 = force all ACQUISITION)
    double m_dquality_factor = 0.035;    // drought decay factor (SUPer default)
    double m_refresh_rate = 1.0;         // acqrate/100 (scales drought)
    double m_ssim_tol = 0.0;             // ssim_tol/100 (adjusts threshold per resolution)
    int m_insert_acquisitions = 2;       // extra_acq (min palette updates to force acq)
    double m_drought = 0.0;
    int m_composition_n = 1;
    int m_palette_vn = 0;
    int m_reuse_candidates = 0;
};

} // namespace core
} // namespace opensup
