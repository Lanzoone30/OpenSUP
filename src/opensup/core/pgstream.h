// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "opensup/core/segments.h"

namespace opensup {
namespace core {

/// Buffer usage statistics collected by the leaky-bucket simulation.
struct buffer_stats_t {
    double min_usage = 1.0;  ///< Min buffer usage in [0..1] (1 = full).
    double avg_usage = 0.0;  ///< Average buffer usage in [0..1].
    double max_rate_1s = 0.0; ///< Max 1s rolling bitrate in Mbps.
    int64_t count = 0;        ///< Segments simulated.
    uint32_t ts_min = 0;      ///< 90 kHz tick of the minimum usage.
    uint32_t ts_avg = 0;      ///< 90 kHz tick of the peak 1s rate.
};

/// BD decoder buffer model (leaky bucket, 1 MiB) — ported from SUPer pgstream.py.
class leaky_buffer_c {
public:
    static constexpr int64_t SIZE = 1 << 20;           ///< Decoder buffer size in bytes.
    static constexpr uint64_t TS_MASK = 0xFFFFFFFFull; ///< 32-bit timestamp wrap.

    /// @param first_ts DTS tick just before the first segment; buffer starts full.
    /// @param bitrate  Refill rate in bytes/second.
    leaky_buffer_c(uint32_t first_ts, int64_t bitrate) noexcept;

    /// Consume one segment (refill then subtract its size).
    /// @return false when the buffer underflows.
    [[nodiscard]] bool step(const pg_segment_c& seg);

    /// Track peak and 1s-average bitrate of one display set.
    void set_bitrate(int64_t size_ds, uint32_t curr_ts) noexcept;

    /// Current buffer usage in [0..1].
    [[nodiscard]] double usage() const noexcept {
        return static_cast<double>(m_used_bytes) / static_cast<double>(SIZE);
    }
    /// Collected statistics.
    [[nodiscard]] buffer_stats_t stats() const noexcept { return m_stats; }

private:
    int64_t m_used_bytes = SIZE;
    int64_t m_bitrate;
    uint32_t m_last_ts;
    bool m_good_ds = true;
    buffer_stats_t m_stats;
    std::vector<std::pair<int64_t, uint32_t>> m_rate_past; ///< (bytes, tick) within 1s window.
};

/// Simulate the decoder buffer over the given segments. Returns true when no
/// underflow occurred; logs AVG/PEAK bitrate and underflow margins.
[[nodiscard]] bool test_rx_bitrate(
    const std::vector<std::shared_ptr<pg_segment_c>>& segments,
    int64_t bitrate_bytes_per_s);

} // namespace core
} // namespace opensup
