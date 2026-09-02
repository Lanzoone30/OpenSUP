// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/core/pgstream.h"

#include "opensup/common/logger.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace opensup {
namespace core {

namespace {

using common::logger_c;

/// Python round() semantics (banker's rounding, half-to-even) as used by the original.
int64_t round_banker(double x) noexcept {
    double fl = std::floor(x);
    int64_t r = static_cast<int64_t>(fl);
    double frac = x - fl;
    if (frac > 0.5 || (frac == 0.5 && (r % 2) != 0))
        ++r;
    return r;
}

/// Full serialized length of a segment (13-byte header + payload).
int64_t full_len(const pg_segment_c& seg) noexcept {
    return static_cast<int64_t>(seg.size()) + PGS_HEADER_LEN;
}

/// A display set: one PCS plus its WDS/PDS/ODS/ENDS segments.
struct ds_group_t {
    std::vector<std::shared_ptr<pg_segment_c>> segments;
    uint32_t pts = 0; ///< PCS presentation time in 90 kHz ticks.
};

} // namespace

leaky_buffer_c::leaky_buffer_c(uint32_t first_ts, int64_t bitrate) noexcept
    : m_bitrate(bitrate), m_last_ts(first_ts) {}

bool leaky_buffer_c::step(const pg_segment_c& seg) {
    if (seg.type() == segment_type_e::pcs)
        m_good_ds = true; // display set boundary: restart underflow tracking

    const uint32_t new_ts = seg.tdts();
    const uint64_t dticks = (static_cast<uint64_t>(new_ts) - m_last_ts) & TS_MASK;

    // Refill at the given bitrate, capped at the buffer size (SUPer pgstream.py:74).
    m_used_bytes = std::min(
        m_used_bytes +
            round_banker(static_cast<double>(dticks) * static_cast<double>(m_bitrate) / PGS_FREQ),
        SIZE);

    // Payload consumes buffer; SUPer subtracts len(seg) - 2 ('PG' magic excluded).
    m_used_bytes -= full_len(seg) - 2;

    const double u = usage();
    if (u <= m_stats.min_usage) {
        m_stats.min_usage = u;
        m_stats.ts_min = new_ts;
    }
    m_stats.avg_usage = (m_stats.avg_usage * static_cast<double>(m_stats.count) + u) /
                        (static_cast<double>(m_stats.count) + 1.0);
    ++m_stats.count;

    m_last_ts = new_ts;
    m_good_ds &= m_used_bytes >= 0;

    if (!m_good_ds && seg.type() == segment_type_e::end) {
        logger_c::instance().warn("PG stream underflow at " +
                                  std::to_string(seg.tpts()) +
                                  " ticks: " + std::to_string(m_used_bytes) + " bytes.");
    }
    return m_used_bytes >= 0;
}

void leaky_buffer_c::set_bitrate(int64_t size_ds, uint32_t curr_ts) noexcept {
    // Keep only display-set sizes within the last second (SUPer pgstream.py:93).
    m_rate_past.erase(
        std::remove_if(m_rate_past.begin(), m_rate_past.end(),
                       [curr_ts](const auto& entry) {
                           return ((static_cast<uint64_t>(curr_ts) - entry.second) &
                                   leaky_buffer_c::TS_MASK) > PGS_FREQ;
                       }),
        m_rate_past.end());
    m_rate_past.emplace_back(size_ds, curr_ts);

    int64_t window_bytes = 0;
    for (const auto& entry : m_rate_past)
        window_bytes += entry.first;
    const double crate = static_cast<double>(window_bytes) / (128.0 * 1024.0); // Mbps
    if (crate >= m_stats.max_rate_1s) {
        m_stats.max_rate_1s = crate;
        m_stats.ts_avg = curr_ts;
    }
}

bool test_rx_bitrate(const std::vector<std::shared_ptr<pg_segment_c>>& segments,
                     int64_t bitrate_bytes_per_s) {
    if (segments.empty())
        return true;

    // Group into display sets (one per PCS) to drive per-DS rate measurement.
    std::vector<ds_group_t> display_sets;
    for (const auto& seg : segments) {
        if (seg->type() == segment_type_e::pcs) {
            display_sets.push_back({});
            display_sets.back().pts = seg->tpts();
        }
        if (display_sets.empty())
            continue; // segments before the first PCS: ignore
        display_sets.back().segments.push_back(seg);
    }
    if (display_sets.empty())
        return true;

    const uint32_t first_ds_pts = display_sets.front().pts;
    const uint32_t last_ds_pts = display_sets.back().pts;

    // Buffer starts full, one second before the first segment (SUPer pgstream.py:116).
    const uint32_t first_tdts = display_sets.front().segments.front()->tdts();
    const uint32_t first_ts = static_cast<uint32_t>(
        (static_cast<uint64_t>(first_tdts) - static_cast<uint64_t>(PGS_FREQ)) & 0xFFFFFFFFull);
    leaky_buffer_c leaky(first_ts, bitrate_bytes_per_s);

    bool is_ok = true;
    int64_t total_bytes = 0;
    uint32_t prev_ts = first_ts;
    int64_t dur_offset = 0;
    bool wrap_armed = true; // SUPer: add 2^32 once on the first 32-bit wrap

    for (const auto& ds : display_sets) {
        int64_t ds_bytes = 0;
        for (const auto& seg : ds.segments) {
            is_ok &= leaky.step(*seg);
            const int64_t len = full_len(*seg);
            total_bytes += len;
            ds_bytes += len;
        }
        leaky.set_bitrate(ds_bytes, ds.pts);
        if (ds.pts < prev_ts && wrap_armed && prev_ts != first_ts) {
            dur_offset += static_cast<int64_t>(leaky_buffer_c::TS_MASK) + 1;
            wrap_armed = false;
        }
        prev_ts = ds.pts;
    }
    dur_offset += static_cast<int64_t>(last_ds_pts) - first_ds_pts;

    const auto stats = leaky.stats();
    const double duration_s = static_cast<double>(dur_offset) / PGS_FREQ;
    const double avg_mbps =
        duration_s > 0.0 ? static_cast<double>(total_bytes) / (128.0 * 1024.0) / duration_s : 0.0;

    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "Bitrate: AVG=%.04f Mbps, PEAK(1s)=%.03f Mbps @ %.3fs.",
                  avg_mbps, stats.max_rate_1s, static_cast<double>(stats.ts_avg) / PGS_FREQ);
    logger_c::instance().info(buf);
    std::snprintf(buf, sizeof(buf),
                  "Target bitrate underflow margin (higher is better): AVG=%.02f%%, "
                  "MIN=%.02f%% @ %.3fs.",
                  stats.avg_usage * 100.0, stats.min_usage * 100.0,
                  static_cast<double>(stats.ts_min) / PGS_FREQ);
    if (is_ok)
        logger_c::instance().info(buf);
    else
        logger_c::instance().warn(buf);

    return is_ok;
}

} // namespace core
} // namespace opensup
