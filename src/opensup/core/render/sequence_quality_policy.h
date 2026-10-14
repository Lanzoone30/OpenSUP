// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#pragma once

#include <cstdint>

#include "opensup/core/segments.h"

namespace opensup {
namespace core {

/**
 * @brief Drought / acquisition policy for a run of events.
 *
 * Owns the four tuning knobs (compression, acq rate, SSIM tolerance, extra
 * acquisitions) and the state they evolve, so the knobs and their interaction
 * are read and adjusted in one place. Pure logic: no PGS emission.
 */
class sequence_quality_policy_c {
public:
    /// Tuning knobs. Defaults are the single source of truth.
    struct options_t {
        double quality_factor = 0.8;  // compression/100 (0 = force all ACQUISITION)
        double refresh_rate = 1.0;    // acqrate/100 (scales the drought increment)
        double ssim_tol = 0.0;        // ssim_tol/100 (per-resolution threshold adjust)
        int insert_acquisitions = 2;  // extra_acq (min palette updates to force acq)
        int height = 1080;            // resolution-dependent SSIM base
    };

    /// Outcome of resolving one event's composition state.
    struct decision_t {
        pcs_c::composition_state_e comp_state = pcs_c::composition_state_e::epoch_start;
        bool reusable_refresh = false;  // reusable node re-emits its ODS
    };

    explicit sequence_quality_policy_c(const options_t& opts) noexcept;

    /// Resolution-dependent base SSIM threshold.
    [[nodiscard]] double base_ssim_threshold() const noexcept { return m_base_ssim; }

    /// Similarity threshold for a cross-percentage: identical regions get the
    /// full discount, non-overlapping ones lower it toward the base.
    [[nodiscard]] double fuse_threshold(double cross_percentage) const noexcept;

    /// Resolves the composition state for one event and advances the drought.
    [[nodiscard]] decision_t decide(bool normal_case, bool reusable, bool forced,
                                    bool force_acquisition, bool acq_valid,
                                    double dtl) noexcept;

    /// True when enough NORMAL events accumulated to force an extra acquisition.
    [[nodiscard]] bool acquisition_due(bool forced) const noexcept;

    /// Records an emitted acquisition: resets drought and the update counter.
    void note_acquisition() noexcept;

private:
    double m_thresh;        // quality_factor
    double m_refresh_rate;  // per-event drought increment
    double m_ssim_offset;   // 0.014 * clamp(ssim_tol, -1, 1)
    int m_insert_acqs;      // extra_acq
    double m_base_ssim;     // resolution-dependent SSIM base
    double m_drought = 0.0;
    int m_updates_since_acq = 0;

    static constexpr double kDroughtDecay = 0.035;
};

} // namespace core
} // namespace opensup
