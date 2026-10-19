// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include "opensup/pch.h"
#include "opensup/cli/cli_parser.h"
#include "opensup/cli/inspector.h"
#include "opensup/cli/json_emitter.h"
#include "opensup/core/interface.h"
#include "opensup/common/logger.h"
#include "opensup/version.h"

#include <iostream>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <io.h>
#define TTY_STDERR _isatty(_fileno(stderr))
#else
#include <unistd.h>
#define TTY_STDERR isatty(fileno(stderr))
#endif

using namespace opensup;
using namespace std::chrono;

namespace {

// <outfile>_build.log, with a trailing .sup extension stripped.
std::string
log_file_path(const std::string& out)
{
    const auto pos = out.rfind(".sup");
    const auto stem = (pos != std::string::npos && pos + 4 == out.size()) ? out.substr(0, pos) : out;
    return stem + "_build.log";
}

// Palette warnings arrive from the core as "palette: kind=<k> count=<n>";
// route them into a structured warn event, every other log stays a log line.
void emit_log_or_warn(const std::string& msg, int level) {
    if (level == static_cast<int>(common::log_level_e::warn) &&
        msg.rfind("palette: kind=", 0) == 0) {
        const auto k0 = msg.find('=') + 1;
        const auto k1 = msg.find(' ', k0);
        const auto c0 = msg.find("count=", k1) + 6;
        cli::emit_warn(msg.substr(k0, k1 - k0),
                       std::strtoull(msg.c_str() + c0, nullptr, 10));
        return;
    }
    cli::emit_log_event(level, msg);
}

}

int
main(int argc, char** argv)
{
    // Read-only inspection modes take over before the encode parser runs.
    if (cli::is_inspect_subcommand(argc, argv)) return cli::run_inspector(argc, argv);

    auto opts = cli::parse_args(argc, argv);

    if (opts.output_path.find('.') == std::string::npos) {
        opts.output_path += ".sup";
        common::logger_c::instance().warn("No extension provided, assuming .sup");
    }

    auto config = cli::options_to_config(opts);

    common::logger_c::instance().set_level(
        opts.debug ? common::log_level_e::ldebug : common::log_level_e::info);

    if (opts.log_to_file != 0) {
        // SUPer levels: 10 = debug, 20 = standard, 30 = warnings/errors.
        const auto level = opts.log_to_file == 10 ? common::log_level_e::ldebug
                          : opts.log_to_file == 30 ? common::log_level_e::warn
                                                   : common::log_level_e::info;
        common::logger_c::instance().set_level(level);
        common::logger_c::instance().set_file_output(log_file_path(opts.output_path));
    }

    if (opts.json_mode) {
        common::logger_c::instance().set_quiet(true);
        common::logger_c::instance().set_callback(emit_log_or_warn);

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
    // 034/US3: census of the stream we just wrote, same counters as `stats`.
    result.counters = core::count_pg_segments(renderer.segments());
    result.reuse_candidates = renderer.reuse_candidates();

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