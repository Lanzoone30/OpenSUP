// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/core/interface.h"
#include "opensup/core/renderer.h"
#include "opensup/core/filestreams.h"
#include "opensup/core/pgstream.h"
#include "opensup/core/layout_engine.h"
#include "opensup/common/logger.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>

namespace opensup {
namespace core {

using common::logger_c;

static std::vector<uint8_t> extract_alpha(const std::vector<uint8_t>& rgba, int w, int h) {
    std::vector<uint8_t> alpha;
    size_t reserve_size = static_cast<size_t>(std::max(0, w)) * static_cast<size_t>(std::max(0, h));
    alpha.reserve(reserve_size);
    for (size_t i = 0; i < rgba.size(); i += 4) {
        alpha.push_back(rgba[i + 3]);
    }
    return alpha;
}

static std::vector<window_definition_t>
determine_windows(const std::vector<bdn_xml_event_c>& events, int width, int height) {
    layout_engine_c layout_engine(width, height);
    
    for (const auto& event : events) {
        auto rgba = event.load_image();
        if (rgba.empty()) continue;
        
        int ew = event.width();
        int eh = event.height();
        if (ew <= 0 || eh <= 0) continue;
        
        auto alpha = extract_alpha(rgba, ew, eh);
        layout_engine.add(event.x(), event.y(), alpha, ew, eh);
        event.unload();  // free memory after use
    }
    
    auto [container, w0, w1, is_vertical] = layout_engine.get_layout();
    
    std::vector<window_definition_t> windows;
    window_definition_t wd0;
    wd0.window_id = 0;
    wd0.h_pos = static_cast<uint16_t>(w0.x);
    wd0.v_pos = static_cast<uint16_t>(w0.y);
    wd0.width = static_cast<uint16_t>(w0.dx);
    wd0.height = static_cast<uint16_t>(w0.dy);
    windows.push_back(wd0);
    
    if (w0.x != w1.x || w0.y != w1.y || w0.dx != w1.dx || w0.dy != w1.dy) {
        window_definition_t wd1;
        wd1.window_id = 1;
        wd1.h_pos = static_cast<uint16_t>(w1.x);
        wd1.v_pos = static_cast<uint16_t>(w1.y);
        wd1.width = static_cast<uint16_t>(w1.dx);
        wd1.height = static_cast<uint16_t>(w1.dy);
        windows.push_back(wd1);
    }
    
    return windows;
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

    // Encode epochs in parallel. Each epoch is independent (parity with the original:
    // palette ids restart at 0 per epoch) so results are deterministically
    // assembled by epoch index after all workers finish.
    const int total_epochs = static_cast<int>(event_groups.size());
    auto fps_enum = xml.fps();

    int n_threads = m_config.threads <= 0
                        ? static_cast<int>(std::thread::hardware_concurrency())
                        : m_config.threads;
    if (n_threads < 1)
        n_threads = 1;
    if (n_threads > total_epochs)
        n_threads = total_epochs;

    std::vector<std::vector<std::shared_ptr<pg_segment_c>>> epoch_results(
        static_cast<size_t>(total_epochs));
    std::atomic<int> next_epoch{0};
    std::atomic<int> completed{0};
    std::atomic<int> total_segments{0};
    std::atomic<int> reuse_candidates{0};
    std::atomic<bool> aborted{false};
    std::mutex progress_mtx;  // progress_cb is not thread-safe

    auto encode_one = [&](int epoch_index) {
        auto& group = event_groups[static_cast<size_t>(epoch_index)];
        std::vector<bool> no_redraw;

        if (m_config.redraw_period > 0.0) {
            auto [expanded, flags] = add_periodic_refreshes(group, m_config.fps, m_config.redraw_period);
            group = std::move(expanded);
            no_redraw = std::move(flags);
        }

        // Determine window layout for this epoch (before encoding)
        auto windows = determine_windows(group, m_config.width, m_config.height);

        // Encode (palette ids restart at 0 per epoch, threaded parity with the original)
        epoch_encoder_c encoder(m_config.fps, m_config.width, m_config.height,
                                m_config.quantizer_id,
                                m_config.allow_normal_case, m_config.overlap,
                                m_config.full_palette,
                                m_config.prefer_normal_case,
                                m_config.compression / 100.0,
                                m_config.acqrate / 100.0,
                                m_config.ssim_tol / 100.0,
                                m_config.extra_acq,
                                m_config.alternate_oids);
        int palette_base = 0;
        auto segs = encoder.encode_epoch(group, no_redraw, fps_enum, palette_base, windows);
        total_segments += static_cast<int>(segs.size());
        epoch_results[static_cast<size_t>(epoch_index)] = std::move(segs);

        int done = completed.fetch_add(1) + 1;
        reuse_candidates += encoder.reuse_candidates();

        if (m_config.progress_cb) {
            std::lock_guard<std::mutex> lock(progress_mtx);
            m_config.progress_cb(done * 100 / total_epochs, done, total_epochs);
        }
    };

    std::vector<std::future<void>> workers;
    workers.reserve(static_cast<size_t>(n_threads));
    for (int w = 0; w < n_threads; ++w) {
        workers.emplace_back(std::async(std::launch::async, [&]() {
            while (true) {
                const int i = next_epoch.fetch_add(1);
                if (i >= total_epochs)
                    break;
                // Allow early exit if user aborted
                if (aborted.load() ||
                    (m_config.abort_flag && m_config.abort_flag->load())) {
                    aborted.store(true);
                    break;
                }
                encode_one(i);
            }
        }));
    }
    for (auto& w : workers)
        w.get();

    if (aborted.load()) {
        result.error = "Aborted by user";
        logger_c::instance().warn(result.error);
        return result;
    }

    // Assemble in epoch order (deterministic regardless of completion order).
    for (auto& segs : epoch_results)
        m_segments.insert(m_segments.end(), segs.begin(), segs.end());
    result.segments = total_segments.load();
    m_reuse_candidates = reuse_candidates.load();

    // fix_composition_id: ensure sequential composition numbers across all epochs
    {
        uint16_t comp_n = 0;
        for (auto& seg : m_segments) {
            if (auto pcs = std::dynamic_pointer_cast<pcs_c>(seg))
                pcs->set_composition_n(comp_n++);
        }
    }

    // Optional stream bitrate validation (SUPer test_output max_kbps path).
    // Warns on decoder-buffer underflow; never fails the encode.
    if (m_config.max_kbps > 0) {
        logger_c::instance().info("Checking PGS bitrate and buffer usage w.r.t user max bitrate: " +
                                  std::to_string(m_config.max_kbps) + " Kbps...");
        // Result intentionally unused: underflow only warns, never fails (parity with the original).
        (void)test_rx_bitrate(m_segments,
                              static_cast<int64_t>(m_config.max_kbps) * 1000 / 8);
    }

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
