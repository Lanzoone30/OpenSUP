// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include "opensup/pch.h"
#include "opensup/cli/cli_parser.h"
#include "opensup/cli/json_emitter.h"
#include "opensup/core/interface.h"
#include "opensup/common/logger.h"
#include "opensup/version.h"

#include <iostream>
#include <chrono>
#include <cstdio>
#ifdef _WIN32
#include <io.h>
#define TTY_STDERR _isatty(_fileno(stderr))
#else
#include <unistd.h>
#define TTY_STDERR isatty(fileno(stderr))
#endif

using namespace opensup;
using namespace std::chrono;

int
main(int argc, char** argv)
{
    auto opts = cli::parse_args(argc, argv);

    if (opts.output_path.find('.') == std::string::npos) {
        opts.output_path += ".sup";
        common::logger_c::instance().warn("No extension provided, assuming .sup");
    }

    auto config = cli::options_to_config(opts);

    common::logger_c::instance().set_level(
        opts.debug ? common::log_level_e::ldebug : common::log_level_e::info);

    if (opts.json_mode) {
        common::logger_c::instance().set_quiet(true);
        common::logger_c::instance().set_callback(
            [](const std::string& msg, int level) {
                cli::emit_log_event(level, msg);
            });

        config.progress_cb = [](int percent, int epoch, int total) {
            cli::emit_progress_event(percent, epoch, total);
        };
    } else {
        std::cerr << "OpenSUP v" << OPENSUP_VERSION_STRING << " - PGS Subtitle Encoder" << std::endl;
        std::cerr << "Input:  " << config.input_path << std::endl;
        std::cerr << "Output: " << config.output_path << std::endl;

        // Human progress on an interactive terminal only (never pollute
        // pipes/files; --json keeps its own structured progress above).
        if (TTY_STDERR) {
            auto start = steady_clock::now();
            config.progress_cb = [start](int percent, int epoch, int total) mutable {
                std::cerr << "\rEpoch " << epoch << "/" << total
                          << " · " << percent << "%";
                if (epoch > 0 && total > epoch) {
                    const auto elapsed = duration<double>(steady_clock::now() - start).count();
                    const auto remaining = elapsed * (total - epoch) / epoch;
                    if (remaining >= 60)
                        std::cerr << " · ETA " << static_cast<int>(remaining / 60)
                                  << "m " << static_cast<int>(remaining) % 60 << "s";
                    else
                        std::cerr << " · ETA " << static_cast<int>(remaining) << "s";
                }
                std::cerr.flush();
            };
        }
    }

    core::bdn_render_c renderer(config);
    auto result = renderer.execute();

    if (opts.json_mode) {
        cli::emit_done_event(result);
    } else {
        // Clear the in-place progress line before the final message so the
        // PASS/ERROR line starts on a fresh row.
        if (TTY_STDERR) std::cerr << "\r\x1b[K";
        if (result.success) {
            common::logger_c::instance().pass(
                "Success. Duration: " + std::to_string(static_cast<double>(result.duration_ms) / 1000.0) + "s");
        } else {
            common::logger_c::instance().error(result.error);
        }
    }

    return result.success ? 0 : 1;
}