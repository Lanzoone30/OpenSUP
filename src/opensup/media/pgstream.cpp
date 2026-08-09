// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.
//
// Adapted from the design of SUPer by cubicibo
// (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.
// Independently reimplemented in C++17; the original Python
// implementation is available in Referencias/SUPer-main/.

#include "opensup/pch.h"
#include "opensup/media/pgstream.h"
#include "opensup/media/pgraphics.h"
#include "opensup/common/logger.h"

#include <cmath>
#include <algorithm>
#include <map>
#include <set>
#include <cstring>

namespace opensup {
namespace media {

using common::logger_c;
using core::display_set_t;
using core::pcs_c;
using core::wds_c;
using core::pds_c;
using core::ods_c;
using core::ends_c;
using core::segment_type_e;
using core::c_object_t;

leaky_buffer_c::leaky_buffer_c(uint32_t first_ts, double bitrate)
    : m_bitrate(bitrate > 0.0 ? bitrate : pg_decoder_t::RX)
    , m_last_ts(first_ts)
{}

bool
leaky_buffer_c::step(const core::pg_segment_c& segment)
{
    auto s_type = segment.type();
    if (s_type == segment_type_e::pcs)
        m_good_ds = true;

    uint32_t new_ts = segment.tdts();
    uint32_t dticks = (new_ts - m_last_ts) & TS_MASK;

    m_used_bytes = std::min(
        m_used_bytes + static_cast<size_t>(std::round(
            static_cast<double>(dticks) * m_bitrate / pg_decoder_t::FREQ)),
        SIZE);

    auto seg_bytes = segment.to_bytes();
    m_used_bytes -= (seg_bytes.size() - 2);

    set_stats(segment.tdts());
    m_last_ts = new_ts;

    m_good_ds = m_good_ds && (m_used_bytes < SIZE);

    if (!m_good_ds && s_type == segment_type_e::end) {
        auto pts_str = m_tc_func ? m_tc_func(segment.tpts()) : std::to_string(segment.tpts());
        logger_c::instance().warn("PG stream underflow at " + pts_str + ": " +
                                   std::to_string(m_used_bytes) + " bytes.");
    }
    return m_used_bytes < SIZE;
}

void
leaky_buffer_c::set_bitrate(int size_ds, uint32_t curr_ts, uint32_t /*prev_ts*/)
{
    uint32_t dticks = 1; // ponytail: minimal delta
    if (dticks > 0) {
        double rate = static_cast<double>(size_ds) * pg_decoder_t::FREQ / static_cast<double>(dticks);
        if (rate >= m_stats.maxrate) {
            m_stats.maxrate = rate;
            m_stats.ts_maxrate = static_cast<int>(curr_ts);
        }
    }
    m_rate_past.emplace_back(size_ds, curr_ts);

    double crate = 0.0;
    for (auto& x : m_rate_past)
        crate += x.first;
    crate /= (128.0 * 1024.0);
    if (crate >= m_stats.max1s) {
        m_stats.ts_avg = static_cast<int>(curr_ts);
        m_stats.max1s = crate;
    }
}

void
leaky_buffer_c::set_tc_func(std::function<std::string(uint32_t)> func)
{
    m_tc_func = std::move(func);
}

double
leaky_buffer_c::get_usage() const noexcept
{
    return static_cast<double>(m_used_bytes) / static_cast<double>(SIZE);
}

buffer_stats_t
leaky_buffer_c::get_stats() const noexcept
{
    return m_stats;
}

std::tuple<double, double, double>
leaky_buffer_c::get_stats_summary() const noexcept
{
    return std::make_tuple(100.0 * m_stats.min, 100.0 * m_stats.avg,
                            std::round(m_stats.max1s * 1000.0) / 1000.0);
}

void
leaky_buffer_c::set_stats(uint32_t ts)
{
    double usage = get_usage();
    if (usage <= m_stats.min) {
        m_stats.min = usage;
        m_stats.ts_min = static_cast<int>(ts);
    }
    m_stats.avg = ((m_stats.avg * static_cast<double>(m_stats.count)) + usage) /
                   static_cast<double>(m_stats.count + 1);
    m_stats.count++;
}

bool
test_rx_bitrate(const std::vector<std::shared_ptr<display_set_t>>& epochs,
                 double bitrate, double /*fps*/)
{
    bool is_ok = true;
    auto tdts = epochs[0]->segments[0]->tdts();
    uint32_t prev_ts = (tdts - static_cast<uint32_t>(pg_decoder_t::FREQ)) & 0xFFFFFFFF;
    leaky_buffer_c leaky(prev_ts, bitrate);

    leaky.set_tc_func([](uint32_t pts) -> std::string {
        return std::to_string(static_cast<double>(pts) / pg_decoder_t::FREQ);
    });

    uint32_t ts_first = prev_ts;
    for (auto& epoch : epochs) {
        for (auto& seg : epoch->segments) {
            is_ok &= leaky.step(*seg);
        }
        leaky.set_bitrate(static_cast<int>(epoch->segments.size()),
                          epoch->segments.back()->tpts(), prev_ts);
        if (epoch->segments.back()->tpts() < prev_ts && ts_first != 0 && ts_first != prev_ts)
            ts_first = 0;
        prev_ts = epoch->segments.back()->tpts();
    }
    return is_ok;
}

[[maybe_unused]] static bool
test_display_set(display_set_t& ds)
{
    bool comply = true;
    auto& pcs = ds.pcs();

    comply = comply && pcs.n_objects() <= 2 && pcs.n_objects() == pcs.cobjects.size();
    if (pcs.composition_state() != pcs_c::composition_state_e::normal)
        comply = comply && !pcs.pal_flag();
    comply = comply && pcs.pal_id() < 8;

    auto wds_opt = ds.wds();
    if (wds_opt.has_value()) {
        auto& wds = wds_opt->get();
        comply = comply && !pcs.pal_flag();
        comply = comply && wds.n_windows() >= 1 && wds.n_windows() <= 2;
    }

    auto pds_list = ds.pds();
    std::set<uint8_t> pds_ids;
    for (auto& pds : pds_list) {
        comply = comply && (pds_ids.find(pds->p_id()) == pds_ids.end());
        pds_ids.insert(pds->p_id());
        if (pcs.pal_flag())
            comply = comply && pcs.pal_id() == pds->p_id();
        comply = comply && pds->p_id() < 8;
    }

    auto ods_list = ds.ods();
    int ctx_cnt = 0;
    for (auto& ods : ods_list) {
        if (ods->seq_flags() == ods_c::sequence_flags_e::first) ctx_cnt++;
        if (ods->seq_flags() == ods_c::sequence_flags_e::last)  ctx_cnt--;
    }
    comply = comply && (ctx_cnt == 0);
    comply = comply && (ds.segments.back()->type() == segment_type_e::end);
    return comply;
}

bool
is_compliant(const std::vector<std::shared_ptr<display_set_t>>& epochs,
              double /*fps*/, int& warnings)
{
    bool compliant = true;
    warnings = 0;
    bool seen_first_composition = false;

    for (auto& epoch : epochs) {
        auto& pcs = (*epoch).pcs();

        // Only the FIRST composition of the stream must be Epoch Start —
        // the decoder needs a sync point to begin. Later display sets may be
        // NORMAL (clear/undisplay DS, or reuse of objects already in the
        // buffer per US7620297B2).
        if (!pcs.cobjects.empty()) {
            if (!seen_first_composition) {
                if (!(static_cast<uint8_t>(pcs.composition_state()) & 0x80)) {
                    logger_c::instance().warn("First DS in epoch is not Epoch Start");
                    compliant = false;
                }
                seen_first_composition = true;
            }
        }

        auto wds_opt = (*epoch).wds();
        if (wds_opt.has_value()) {
            auto& wds = wds_opt->get();
            for (auto& wd : wds.windows) {
                if (wd.h_pos + wd.width > pcs.video_width() ||
                    wd.v_pos + wd.height > pcs.video_height()) {
                    logger_c::instance().error("Window out of screen");
                    compliant = false;
                }
            }
        }
    }
    return compliant;
}

bool
check_pts_dts_sanity(const std::vector<std::shared_ptr<display_set_t>>& epochs,
                      double /*fps*/)
{
    const uint32_t TS_MASK = 0xFFFFFFFF;
    bool is_ok = true;

    for (auto& epoch : epochs) {
        auto& pcs = (*epoch).pcs();

        // The full-screen decode margin only applies to Epoch Start display
        // sets, where the decoder must receive every object before presenting
        // (US20090185789A1). Normal/acquisition sets (reuse, clear, palette
        // updates) reference objects already in the buffer — their PTS-DTS
        // delta is the wipe time, which is much smaller.
        if (pcs.composition_state() != pcs_c::composition_state_e::epoch_start)
            continue;

        uint32_t wipe_duration = static_cast<uint32_t>(std::ceil(
            static_cast<double>(pcs.video_width()) * pcs.video_height() *
            pg_decoder_t::FREQ / pg_decoder_t::RC));

        uint32_t pts_dts_delta = (pcs.tpts() - pcs.tdts()) & TS_MASK;
        if (pts_dts_delta <= wipe_duration) {
            logger_c::instance().error("Incorrect PTS-DTS values for epoch start");
            is_ok = false;
        }
    }
    return is_ok;
}

} // namespace media
} // namespace opensup
