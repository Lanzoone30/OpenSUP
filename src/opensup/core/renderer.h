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
                     bool full_palette = false, double ssim_tol = 0.0);

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

    double m_fps;
    int m_width, m_height;
    int m_quantizer_id = 0;
    bool m_allow_normal_case = false;
    bool m_overlap = false;
    bool m_full_palette = false;
    double m_ssim_tol = 0.0;
    double m_drought = 0.0;
    int m_composition_n = 1;
    int m_palette_vn = 0;
    int m_reuse_candidates = 0;
};

} // namespace core
} // namespace opensup
