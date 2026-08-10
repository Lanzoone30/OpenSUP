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
#include <optional>
#include <string>

#include "opensup/common/geometry.h"
#include "opensup/core/segments.h"

namespace opensup {
namespace media {

/**
 * @brief Planning context of one epoch during buffer simulation.
 *
 * Tracks the bounding box, candidate windows and the DTS/PTS window the
 * epoch occupies. The optimizer fills this to test reuse candidates.
 */
struct epoch_context_t {
    common::box_t box;
    std::vector<common::box_t> windows;
    double min_dts = -1e100;
    double max_pts = 1e100;
    std::vector<bool> redraw_flags;
};

/// Aggregated statistics of a leaky-buffer simulation run.
struct buffer_stats_t {
    double min = 1e100;
    double avg = 0.0;
    double maxrate = 0.0;
    int count = 0;
    int ts_maxrate = 0;
    int ts_min = 0;
    int ts_avg = 0;
    double max1s = 0.0;
};

/**
 * @brief Simulates the PGS decoder's receive buffer (leaky bucket).
 *
 * The Blu-ray spec caps how much subtitle data may arrive per second;
 * a stream that overflows the decoder buffer is not player-compliant.
 * This class replays segments through that model so the encoder can keep
 * bitrate within limits.
 */
class leaky_buffer_c {
public:
    static constexpr size_t SIZE = 1 << 20;
    static constexpr uint32_t TS_MASK = 0xFFFFFFFF;

    leaky_buffer_c(uint32_t first_ts, double bitrate = 0.0);

    /// Feed one segment through the buffer; false if it overflows.
    bool step(const core::pg_segment_c& segment);
    /// Update the drain rate from a measured display-set bitrate.
    void set_bitrate(int size_ds, uint32_t curr_ts, uint32_t prev_ts);
    void set_tc_func(std::function<std::string(uint32_t)> func);

    /// Current fill ratio (0..1).
    [[nodiscard]] double get_usage() const noexcept;
    [[nodiscard]] buffer_stats_t get_stats() const noexcept;
    [[nodiscard]] std::tuple<double, double, double> get_stats_summary() const noexcept;

private:
    void set_stats(uint32_t ts);

    size_t m_used_bytes = SIZE;
    double m_bitrate;
    uint32_t m_last_ts;
    buffer_stats_t m_stats;
    bool m_good_ds = true;
    std::vector<std::pair<int, uint32_t>> m_rate_past;
    std::function<std::string(uint32_t)> m_tc_func;
};

/// True if the epochs fit the receiver bitrate limit at the given fps.
bool test_rx_bitrate(const std::vector<std::shared_ptr<core::display_set_t>>& epochs,
                      double bitrate, double fps);

/**
 * @brief Full PGS compliance check over a set of epochs.
 *
 * Wraps the buffer and timing checks; increments `warnings` for each
 * non-fatal violation found.
 */
bool is_compliant(const std::vector<std::shared_ptr<core::display_set_t>>& epochs,
                   double fps, int& warnings);

/// True if PTS/DTS timestamps are monotonically sane across epochs.
bool check_pts_dts_sanity(const std::vector<std::shared_ptr<core::display_set_t>>& epochs,
                           double fps);

} // namespace media
} // namespace opensup
