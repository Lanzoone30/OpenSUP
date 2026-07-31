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
#include "opensup/core/interface.h"
#include "opensup/core/renderer.h"
#include "opensup/core/filestreams.h"
#include "opensup/media/pgstream.h"
#include "opensup/common/logger.h"

#include <filesystem>
#include <fstream>
#include <chrono>

namespace opensup {
namespace core {

using common::logger_c;

// ── Epoch Worker ──
epoch_worker_c::epoch_worker_c(std::queue<epoch_job_t>& jobs,
                                 std::mutex& mutex,
                                 std::condition_variable& cv,
                                 std::atomic<bool>& done,
                                 double fps, int width, int height)
    : m_jobs(jobs), m_mutex(mutex), m_cv(cv), m_done(done)
    , m_fps(fps), m_width(width), m_height(height) {}

void
epoch_worker_c::operator()()
{
    // ponytail: single-thread encoding per worker, fine for <100 epochs
    epoch_encoder_c encoder(m_fps, m_width, m_height);

    while (true) {
        epoch_job_t job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() {
                return !m_jobs.empty() || m_done.load();
            });
            if (m_jobs.empty() && m_done.load())
                return;
            job = std::move(m_jobs.front());
            m_jobs.pop();
        }

        auto segments = encoder.encode_epoch(job.events, job.redraw_flags,
                                               job.fps, job.palette_base);
        // segments are accumulated by the caller
        // ponytail: direct accumulation, no return queue needed for single-thread debug
    }
}

// ── BDN Render ──
bdn_render_c::bdn_render_c(const encode_config_t& config)
    : m_config(config) {}

encode_result_t
bdn_render_c::execute()
{
    encode_result_t result;
    auto start_time = std::chrono::steady_clock::now();

    // Parse BDN XML
    bdn_xml_c xml;
    if (!xml.parse(m_config.input_path, m_config.ignore_resolution)) {
        result.error = "Failed to parse BDN XML: " + m_config.input_path;
        logger_c::instance().error(result.error);
        return result;
    }

    m_config.fps = xml.fps().to_double();
    m_config.width = xml.width();
    m_config.height = xml.height();

    logger_c::instance().info("Input: " + std::to_string(xml.events().size()) +
                               " events, " + std::to_string(m_config.width) + "x" +
                               std::to_string(m_config.height) +
                               " @" + std::to_string(m_config.fps) + " fps");

    // Split events into epochs (group by proximity: 1s gap threshold)
    auto event_groups = xml.groups(1.0);
    result.events = static_cast<int>(xml.events().size());
    result.epochs = static_cast<int>(event_groups.size());

    logger_c::instance().info("Split into " + std::to_string(result.epochs) +
                                " epoch(s)");

    // Overwrite guard
    if (!m_config.overwrite && std::filesystem::exists(m_config.output_path)) {
        result.error = "Output file exists, use -y to overwrite";
        logger_c::instance().error(result.error);
        return result;
    }

    // Encode each epoch
    int palette_base = 0;
    int total_segments = 0;

    auto fps_enum = xml.fps();

    for (auto& group : event_groups) {
        // Allow early exit if user aborted
        if (m_config.abort_flag && m_config.abort_flag->load()) {
            result.error = "Aborted by user";
            logger_c::instance().warn(result.error);
            return result;
        }

        std::vector<bool> no_redraw;

        // Periodic redraw: split events if redraw_period > 0
        if (m_config.redraw_period > 0.0) {
            auto [expanded, flags] = add_periodic_refreshes(group, m_config.fps, m_config.redraw_period);
            group = std::move(expanded);
            no_redraw = std::move(flags);
        }

        // Encode
        epoch_encoder_c encoder(m_config.fps, m_config.width, m_config.height,
                                m_config.quantizer_id,
                                m_config.allow_normal_case, m_config.overlap,
                                m_config.full_palette, m_config.ssim_tol);
        auto segs = encoder.encode_epoch(group, no_redraw,
                                          fps_enum, palette_base);
        m_segments.insert(m_segments.end(), segs.begin(), segs.end());
        total_segments += static_cast<int>(segs.size());
    }

    // fix_composition_id: ensure sequential composition numbers across all epochs
    {
        uint16_t comp_n = 0;
        for (auto& seg : m_segments) {
            if (auto pcs = std::dynamic_pointer_cast<pcs_c>(seg))
                pcs->set_composition_n(comp_n++);
        }
    }

    // Write output SUP
    sup_file_c::write_sup(m_config.output_path, m_segments);

    if (m_config.both_formats) {
        auto sup = m_config.output_path;
        auto pes = sup;
        auto dot = pes.rfind('.');
        if (dot != std::string::npos)
            pes = pes.substr(0, dot) + ".pes";
        else
            pes = pes + ".pes";
        auto mui = pes + ".mui";
        logger_c::instance().info("Both formats: " + sup + " + " + pes + " + " + mui);
        sup_file_c::write_pes_mui(pes, mui, m_segments);
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    result.success = true;
    result.segments = total_segments;

    logger_c::instance().pass("Encoding complete: " +
                               std::to_string(result.segments) + " segments, " +
                               std::to_string(result.events) + " events, " +
                               std::to_string(result.duration_ms) + " ms");

    logger_c::instance().log(common::log_level_e::hdebug, "Output: " + m_config.output_path);
    return result;
}

} // namespace core
} // namespace opensup
