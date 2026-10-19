// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/core/pgstats.h"

#include "opensup/media/pgraphics.h"

#include <cmath>

namespace opensup {
namespace core {

namespace {

constexpr uint32_t PTS_MASK = 0xFFFFFFFFu;

uint32_t md(uint32_t a, uint32_t b) noexcept
{
    return (a - b) & PTS_MASK;
}

/// Window decode time of the epoch's first WDS, in 90 kHz ticks.
uint32_t wds_ticks(const std::vector<const pg_segment_c*>& ds) noexcept
{
    for (auto* seg : ds) {
        if (seg->type() != segment_type_e::wds) continue;
        const auto* wds = static_cast<const wds_c*>(seg);
        uint32_t sum = 0;
        for (auto& w : wds->windows)
            sum += static_cast<uint32_t>(std::ceil(static_cast<double>(w.width) * w.height *
                                                    media::pg_decoder_t::FREQ /
                                                    media::pg_decoder_t::RC));
        return sum;
    }
    return 0;
}

bool is_epoch_start(const std::vector<const pg_segment_c*>& ds) noexcept
{
    for (auto* seg : ds) {
        if (seg->type() != segment_type_e::pcs) return false;
        const auto* pcs = static_cast<const pcs_c*>(seg);
        return (static_cast<uint8_t>(pcs->composition_state()) & 0x80) != 0;
    }
    return false;
}

}  // namespace

pg_counters_t count_pg_segments(const std::vector<std::shared_ptr<pg_segment_c>>& segments)
{
    pg_counters_t c;
    c.segments = static_cast<int>(segments.size());

    // Display sets close on ENDS (what the .sup parser does too).
    std::vector<std::vector<const pg_segment_c*>> dss;
    std::vector<const pg_segment_c*> cur;
    for (auto& seg : segments) {
        cur.push_back(seg.get());
        if (seg->type() == segment_type_e::end) {
            dss.push_back(std::move(cur));
            cur.clear();
        }
    }
    if (!cur.empty()) dss.push_back(std::move(cur));
    c.display_sets = static_cast<int>(dss.size());

    std::vector<std::pair<size_t, size_t>> epochs; // [begin, end)
    size_t begin = 0;
    for (size_t i = 0; i < dss.size(); ++i) {
        if (i > 0 && is_epoch_start(dss[i])) {
            epochs.emplace_back(begin, i);
            begin = i;
        }
    }
    if (!dss.empty()) epochs.emplace_back(begin, dss.size());
    c.epochs = static_cast<int>(epochs.size());

    for (auto& [b, e] : epochs) {
        const uint32_t pts_delta = wds_ticks(dss[b]);
        for (size_t i = b; i < e; ++i) {
            uint8_t state = 0;
            const pcs_c* pcs = nullptr;
            bool has_wds = false, has_ods = false;
            for (auto* seg : dss[i]) {
                switch (seg->type()) {
                    case segment_type_e::pcs:
                        pcs = static_cast<const pcs_c*>(seg);
                        state = static_cast<uint8_t>(pcs->composition_state());
                        break;
                    case segment_type_e::wds: has_wds = true; break;
                    case segment_type_e::ods: has_ods = true; break;
                    default: break;
                }
            }
            if (state == 0) ++c.nc;
            else if (state & 0x40) ++c.acq;
            else if (state & 0x80) ++c.es;

            if (pcs && pcs->pal_flag()) {
                ++c.pu;
                if (md(pcs->tpts(), pcs->tdts()) > pts_delta + 1) ++c.pu_buffered;
            } else if (has_wds && state == 0 && has_ods) {
                ++c.nc_ods;
            }
        }
    }
    return c;
}

}
}
