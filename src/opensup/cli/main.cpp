// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.

#include "opensup/pch.h"
#include "opensup/cli/cli_parser.h"
#include "opensup/core/interface.h"
#include "opensup/common/logger.h"
#include "opensup/version.h"

#include <iostream>
#include <filesystem>
#include <chrono>

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

    common::logger_c::instance().set_level(common::log_level_e::info);

    std::cout << "OpenSUP v" << OPENSUP_VERSION_STRING << " - HDMV PGS encoder" << std::endl;
    std::cout << "Input:  " << config.input_path << std::endl;
    std::cout << "Output: " << config.output_path << std::endl;

    if (!config.overwrite && std::filesystem::exists(config.output_path)) {
        common::logger_c::instance().error(
            "Output file exists, use -y to overwrite");
        return 1;
    }

    core::bdn_render_c renderer(config);
    auto result = renderer.execute();

    if (result.success) {
        common::logger_c::instance().pass(
            "Success. Duration: " + std::to_string(static_cast<double>(result.duration_ms) / 1000.0) + "s");
        return 0;
    }
    common::logger_c::instance().error(result.error);
    return 1;
}
