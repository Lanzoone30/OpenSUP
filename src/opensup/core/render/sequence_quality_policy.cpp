// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include "opensup/pch.h"
#include "opensup/core/render/sequence_quality_policy.h"

#include <algorithm>
#include <cmath>

namespace opensup {
namespace core {

sequence_quality_policy_c::sequence_quality_policy_c(const options_t& opts) noexcept
    : m_thresh(opts.quality_factor)
    , m_refresh_rate(opts.refresh_rate)
    , m_ssim_offset(0.014 * std::clamp(opts.ssim_tol, -1.0, 1.0))
    , m_insert_acqs(opts.insert_acquisitions)
    // Height maps 480 -> 0.972 and 1080 -> 0.986; no offset subtraction.
    , m_base_ssim(std::min(0.9999, 0.9608 +
          static_cast<double>(opts.height) * (0.986 - 0.972) / (1080.0 - 480.0)))
{
}

double
sequence_quality_policy_c::fuse_threshold(double cross_percentage) const noexcept
{
    // The 0.008333 term (~0.5/60) biases against fusing near the threshold.
    return std::min(1.0, m_base_ssim +
        (1.0 - m_base_ssim) * (1.0 - cross_percentage) -
        0.008333 * (1.0 - m_ssim_offset));
}

sequence_quality_policy_c::decision_t
sequence_quality_policy_c::decide(bool normal_case, bool reusable, bool forced,
                                  bool force_acquisition, bool acq_valid,
                                  double dtl) noexcept
{
    decision_t d;
    d.comp_state = (normal_case || reusable)
        ? pcs_c::composition_state_e::normal
        : pcs_c::composition_state_e::epoch_start;

    if (m_thresh == 0.0 && !reusable && !normal_case) {
        // compression=0 -> force all ACQUISITION
        d.comp_state = pcs_c::composition_state_e::acquisition;
        m_drought = 0.0;
    } else if (d.comp_state != pcs_c::composition_state_e::acquisition) {
        if (normal_case) {
            // A NORMAL-case winner is never downgraded to ACQUISITION.
        } else if (forced || force_acquisition) {
            // A new object popped in a window forces ACQUISITION and resets drought.
            d.comp_state = pcs_c::composition_state_e::acquisition;
            m_drought = 0.0;
        } else if (reusable && acq_valid &&
                   dtl > std::max(m_thresh - kDroughtDecay * m_drought, 0.0)) {
            // Reusable node with enough decode margin: acquire (re-emit the ODS).
            d.comp_state = pcs_c::composition_state_e::acquisition;
            d.reusable_refresh = true;
            m_drought = 0.0;
        } else if (d.comp_state == pcs_c::composition_state_e::normal) {
            // Prevent excessive acquisitions: grow the drought.
            m_drought += 1.0 * m_refresh_rate;
        }
    }

    // Count NORMAL events (palette updates) since the last acquisition.
    if (d.comp_state == pcs_c::composition_state_e::acquisition ||
        d.comp_state == pcs_c::composition_state_e::epoch_start) {
        m_updates_since_acq = 0;
    } else if (d.comp_state == pcs_c::composition_state_e::normal && m_insert_acqs > 0) {
        m_updates_since_acq++;
    }
    return d;
}

bool
sequence_quality_policy_c::acquisition_due(bool forced) const noexcept
{
    return m_insert_acqs > 0 && m_updates_since_acq > m_insert_acqs && !forced;
}

void
sequence_quality_policy_c::note_acquisition() noexcept
{
    m_updates_since_acq = 0;
    m_drought = 0.0;
}

} // namespace core
} // namespace opensup
