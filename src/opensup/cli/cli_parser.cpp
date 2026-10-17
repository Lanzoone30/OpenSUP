// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include "opensup/pch.h"
#include "opensup/cli/cli_parser.h"
#include "opensup/version.h"
#include "opensup/common/logger.h"

#include <CLI/CLI.hpp>
#include <cctype>
#include <charconv>
#include <iostream>

namespace opensup {
namespace cli {

cli_options_t
parse_args(int argc, char** argv)
{
    CLI::App app("OpenSUP - PGS Subtitle Encoder");

    cli_options_t opts;

    app.add_option("-i,--input", opts.input_path, "Input BDN XML file")
        ->required()->check(CLI::ExistingFile);

    app.add_option("output", opts.output_path, "Output .sup file")
        ->required();

    app.add_option("-q,--quantizer", opts.quantizer,
                    "Quantizer [0:libimagequant (best, fast), 1:HexTree (good, very fast)] (def: 0)");

    app.add_option("-b,--bt", opts.bt_matrix,
                    "Color Space [BT.709, BT.601, BT.2020] (def: BT.709)");

    app.add_flag("-y,--yes", opts.overwrite,
                  "Overwrite existing output file");

    app.add_flag("--ignore-resolution", opts.ignore_resolution,
                  "Ignore Resolution Validation (Experimental)");

    app.add_flag("-w,--withsup", opts.both_formats,
                  "Generate both SUP and PES+MUI files.");

    app.add_flag("-p,--palette", opts.full_palette,
                  "Write full palette for all epochs.");

    app.add_flag("--allow-normal", opts.allow_normal_case,
                  "Allow normal case object redefinition.");
    app.add_flag("--overlap", opts.overlap,
                  "Allow palette update buffering.");
    app.add_flag("--alternate-oids", opts.alternate_oids,
                  "Alternate per-window object ids (double buffering, multi-window).");

    app.add_flag("--json", opts.json_mode,
                  "Emit NDJSON events to stdout (log/progress/done)");
    app.add_flag("-d,--debug", opts.debug,
                  "Enable debug logging (LDEBUG level)");

    app.add_option("--redraw-period", opts.redraw_period,
                    "Anchor interval in seconds (0 = disable, minimum: 1)");

    app.add_option("-m,--max-kbps", opts.max_kbps,
                    "Validate output against a max bitrate in Kbps [0 = off, 1-9 rejected, >48000 warns] (def: 0)");

    app.add_option("-j,--threads", opts.threads,
                    "Parallel epoch encoding workers [0 = auto, 1 = sequential, max 10] (def: 0 = auto)")
        ->check(CLI::Range(0, 10));

    // Drought / quality parameters
    app.add_option("-c,--compression", opts.compression,
                    "Compression/quality factor [0-100, 0 = force all acquisitions] (def: 80)")
        ->check(CLI::Range(0, 100));
    app.add_option("-a,--acqrate", opts.acqrate,
                    "Acquisition rate / drought scaling [0-100] (def: 100)")
        ->check(CLI::Range(0, 100));
    app.add_option("-t,--ssim-tol", opts.ssim_tol,
                    "SSIM tolerance [-100..100, adjusts threshold per resolution] (def: 0)")
        ->check(CLI::Range(-100, 100));
    app.add_option("-e,--extra-acq", opts.extra_acq,
                    "Insert acquisition after N palette updates [0 = off] (def: 2)")
        ->check(CLI::Range(0, 100));

    app.add_option("--config", opts.config_path,
                    "LIQ overrides ini file ([PILIQ] quality/speed/dither)");

    app.set_version_flag("-v,--version", "OpenSUP v" OPENSUP_VERSION_STRING);

    app.footer("EXAMPLES\n"
               "  opensup_cli -i input.xml output.sup\n"
               "  opensup_cli -i input.xml output.sup -q 1 -y\n\n"
               "Project: https://github.com/Lanzoone30/OpenSUP");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        std::exit(app.exit(e));
    }

    if (opts.redraw_period < 0.0) {
        common::logger_c::instance().error("Negative anchor interval, aborting.");
        std::exit(1);
    }

    if (opts.max_kbps < 10 && opts.max_kbps != 0) {
        common::logger_c::instance().error("Meaningless max bitrate, aborting.");
        std::exit(1);
    }

    if (!opts.config_path.empty() && !read_liq_config(opts.config_path, opts)) {
        std::exit(1);
    }

    return opts;
}

namespace {

/// Canonicalize a color-space name (bt601|601|BT.709|bt-2020...); empty on error.
std::string
canonical_bt_matrix(const std::string& raw)
{
    std::string k;
    for (char c : raw) {
        if (c == '.' || c == ' ' || c == '-' || c == '_') continue;
        k += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (k.rfind("bt", 0) == 0) k.erase(0, 2);
    if (k == "601" || k == "709" || k == "2020") return "bt" + k;
    return {};
}

/// Strip leading/trailing blanks from a line.
std::string
trim(const std::string& s)
{
    const auto first = s.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r");
    return s.substr(first, last - first + 1);
}

} // namespace

bool
read_liq_config(const std::string& path, cli_options_t& opts)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        common::logger_c::instance().error("Cannot open config file: " + path);
        return false;
    }

    std::string line;
    bool in_piliq = false;
    while (std::getline(file, line)) {
        const std::string text = trim(line);
        if (text.empty() || text[0] == ';' || text[0] == '#') continue;
        if (text[0] == '[') {
            const auto close = text.find(']');
            in_piliq = close != std::string::npos
                       && text.substr(1, close - 1) == "PILIQ";
            continue;
        }
        if (!in_piliq) continue;

        const auto eq = text.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(text.substr(0, eq));
        const std::string value = trim(text.substr(eq + 1));

        int* target = nullptr;
        int max_value = 0;
        if (key == "quality") { target = &opts.liq_quality_cfg; max_value = 100; }
        else if (key == "speed") { target = &opts.liq_speed_cfg; max_value = 10; }
        else if (key == "dither") { target = &opts.liq_dither_cfg; max_value = 100; }
        else continue;

        int parsed = 0;
        const auto res = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (res.ec != std::errc{} || res.ptr != value.data() + value.size()
            || parsed < 0 || parsed > max_value) {
            common::logger_c::instance().error(
                "Invalid [PILIQ] '" + key + "' value in config file: " + path);
            return false;
        }
        *target = parsed;
    }
    return true;
}

core::encode_config_t
options_to_config(const cli_options_t& opts)
{
    core::encode_config_t cfg;
    cfg.input_path = opts.input_path;
    cfg.output_path = opts.output_path;
    cfg.quantizer_id = opts.quantizer;
    cfg.overwrite = opts.overwrite;
    cfg.ignore_resolution = opts.ignore_resolution;
    cfg.both_formats = opts.both_formats;
    cfg.allow_normal_case = opts.allow_normal_case;
    cfg.overlap = opts.overlap;
    cfg.alternate_oids = opts.alternate_oids;
    cfg.bt_matrix = canonical_bt_matrix(opts.bt_matrix);
    if (cfg.bt_matrix.empty()) {
        common::logger_c::instance().warn(
            "Unknown color space '" + opts.bt_matrix + "', using BT.709.");
        cfg.bt_matrix = "bt709";
    }
    cfg.full_palette = opts.full_palette;
    cfg.redraw_period = opts.redraw_period;
    if (cfg.redraw_period > 0.0 && cfg.redraw_period < 1.0) {
        common::logger_c::instance().warn("Anchor interval below 1s, disabling.");
        cfg.redraw_period = 0.0;
    }
    cfg.max_kbps = opts.max_kbps;
    if (cfg.max_kbps > 48000) {
        common::logger_c::instance().warn("Max bitrate is beyond BDAV limit.");
    } else if (cfg.max_kbps > 10 && cfg.max_kbps < 500) {
        common::logger_c::instance().warn(
            "Max bitrate is low. Buffer underflow errors will be spammed.");
    }
    cfg.threads = opts.threads;
    cfg.compression = opts.compression;
    cfg.acqrate = opts.acqrate;
    cfg.ssim_tol = opts.ssim_tol;
    cfg.extra_acq = opts.extra_acq;
    if (opts.liq_quality_cfg >= 0) cfg.liq_quality = opts.liq_quality_cfg;
    if (opts.liq_speed_cfg >= 0) cfg.liq_speed = opts.liq_speed_cfg;
    if (opts.liq_dither_cfg >= 0) cfg.liq_dither_pct = opts.liq_dither_cfg;
    return cfg;
}

}
}