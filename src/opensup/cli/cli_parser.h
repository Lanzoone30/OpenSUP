// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.

#pragma once

#include <string>

#include "opensup/core/interface.h"

namespace opensup {
namespace cli {

/// CLI options parsed from argv; consumed by options_to_config().
struct cli_options_t {
    std::string input_path;
    std::string output_path;
    double compression = 80.0;
    double acqrate = 100.0;
    int quantizer = 3;
    std::string bt_matrix = "bt709";
    bool overwrite = false;
    int threads = 0;
    double ssim_tol = 0.0;
    bool ignore_resolution = false;
    bool both_formats = false;
    bool full_palette = false;
    bool allow_normal_case = false;
    bool overlap = false;
    double redraw_period = 0.0;
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
