// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#pragma once

#include <string>

#include "opensup/core/interface.h"

namespace opensup {
namespace cli {

/// CLI options parsed from argv; consumed by options_to_config().
struct cli_options_t {
    std::string input_path;
    std::string output_path;
    int quantizer = 0;
    std::string bt_matrix = "bt709";
    bool overwrite = false;
    double ssim_tol = 0.0;
    bool ignore_resolution = false;
    bool both_formats = false;
    bool full_palette = false;
    bool allow_normal_case = false;
    bool overlap = false;
    double redraw_period = 0.0;
    bool json_mode = false;
};

/**
 * @brief Parse command-line arguments into cli_options_t.
 *
 * Exits with a usage message on unknown flags or missing input.
 */
cli_options_t parse_args(int argc, char** argv);

/// Translate CLI options into the core encoding configuration.
core::encode_config_t options_to_config(const cli_options_t& opts);

} // namespace cli
} // namespace opensup
