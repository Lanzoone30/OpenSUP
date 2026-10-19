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
    int quantizer = 3;       // SUPer ids: 2 = HexTree, 3 = LIQ (def); 0/1/4 reserved
    std::string bt_matrix = "bt709";
    bool overwrite = false;
    bool both_formats = false;
    bool full_palette = false;
    bool allow_normal_case = false;
    bool prefer_normal_case = false;
    bool overlap = false;
    bool alternate_oids = false;   // Legacy global 0/1 oid toggle (per-window oids by default)
    double redraw_period = 0.0;
    int max_kbps = 0;        // 0 = bitrate validation off
    int threads = 0;         // 0 = auto (physical cores)

    // Drought / quality parameters
    int compression = 80;    // quality_factor: 0-100 (def 80)
    int acqrate = 100;       // refresh_rate: 0-100 (def 100)
    int ssim_tol = 0;        // SSIM tolerance: -100..100 (def 0)
    int extra_acq = 2;       // insert_acquisitions: min palette updates (def 2)

    bool json_mode = false;

    // LIQ overrides from --config file ([PILIQ] section); -1 = not provided
    std::string config_path;
    int liq_quality_cfg = -1;
    int liq_speed_cfg = -1;
    int liq_dither_cfg = -1;   // percent
};

/**
 * @brief Parse command-line arguments into cli_options_t.
 *
 * Exits with a usage message on unknown flags or missing input.
 */
cli_options_t parse_args(int argc, char** argv);

/**
 * @brief Load LIQ overrides from an ini file ([PILIQ] quality/speed/dither).
 *
 * @return false on unreadable file or invalid (non-integer/out-of-range)
 *         value; the offending key and file are logged.
 */
bool read_liq_config(const std::string& path, cli_options_t& opts);

/// Translate CLI options into the core encoding configuration.
core::encode_config_t options_to_config(const cli_options_t& opts);

}
}
