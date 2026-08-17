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
#include <string>
#include <atomic>
#include <functional>

#include "opensup/core/filestreams.h"
#include "opensup/core/renderer.h"

namespace opensup {
namespace core {

/// Outcome of a full encode run.
struct encode_result_t {
    bool success = false;
    std::string error;
    int events = 0;
    int epochs = 0;
    int segments = 0;
    int64_t duration_ms = 0;
};

/// Input parameters for a single encode run.
struct encode_config_t {
    std::string input_path;
    std::string output_path;
    double fps = 24.0;
    int width = 1920;
    int height = 1080;
    int quantizer_id = 0;  // 0 = libimagequant, 1 = hextree
    bool overwrite = false;
    bool ignore_resolution = false;
    bool both_formats = false;
    bool allow_normal_case = false;
    bool prefer_normal_case = false;
    bool overlap = false;
    std::string bt_matrix = "bt709";
    bool full_palette = false;
    double redraw_period = 0.0;
    int max_kbps = 0;  // >0: validate output against a max stream bitrate (no failure, only warn)
    std::atomic<bool>* abort_flag = nullptr; // set by caller, checked in execute() loop
    // Called after each epoch: (percent 0-100, epoch index 1-based, total epochs).
    std::function<void(int, int, int)> progress_cb;
};

/**
 * @brief Orchestrates a full BDN → SUP encode.
 *
 * Parses the BDN XML, renders epochs (optionally in parallel), and writes
 * the resulting PGS segments.
 */
class bdn_render_c {
public:
    explicit bdn_render_c(const encode_config_t& config);

    /// Run the whole encode; result carries success and counters.
    encode_result_t execute();

    /// Produced PGS segments (valid after execute()).
    [[nodiscard]] const std::vector<std::shared_ptr<pg_segment_c>>& segments() const noexcept {
        return m_segments;
    }

    /// Total reuse candidates detected across all epochs.
    [[nodiscard]] int reuse_candidates() const noexcept { return m_reuse_candidates; }

private:
    encode_config_t m_config;
    std::vector<std::shared_ptr<pg_segment_c>> m_segments;
    int m_reuse_candidates = 0;
};

} // namespace core
} // namespace opensup
