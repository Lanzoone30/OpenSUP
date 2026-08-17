// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include "opensup/pch.h"
#include "opensup/cli/cli_parser.h"
#include "opensup/version.h"

#include <CLI/CLI.hpp>
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
    app.add_flag("--prefer-normal", opts.prefer_normal_case,
                  "Prefer normal case object redefinition.");
    app.add_flag("--overlap", opts.overlap,
                  "Allow palette update buffering.");

    app.add_flag("--json", opts.json_mode,
                  "Emit NDJSON events to stdout (log/progress/done)");
    app.add_flag("-d,--debug", opts.debug,
                  "Enable debug logging (LDEBUG level)");

    app.add_option("--redraw-period", opts.redraw_period,
                    "Anchor interval in seconds (0 = disable, minimum: 1)");

    app.add_option("-m,--max-kbps", opts.max_kbps,
                    "Validate output against a max bitrate in Kbps [10-48000, 0 = off] (def: 0)")
        ->check(CLI::Range(0, 48000));

    app.add_option("-j,--threads", opts.threads,
                    "Parallel epoch encoding workers [0 = auto, 1 = sequential] (def: 1)")
        ->check(CLI::Range(0, 1024));

    // Drought / quality parameters (SUPer parity)
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

    return opts;
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
    cfg.prefer_normal_case = opts.prefer_normal_case;
    cfg.overlap = opts.overlap;
    cfg.bt_matrix = opts.bt_matrix;
    cfg.full_palette = opts.full_palette;
    cfg.redraw_period = opts.redraw_period;
    cfg.max_kbps = opts.max_kbps;
    cfg.threads = opts.threads;
    cfg.compression = opts.compression;
    cfg.acqrate = opts.acqrate;
    cfg.ssim_tol = opts.ssim_tol;
    cfg.extra_acq = opts.extra_acq;
    return cfg;
}

} // namespace cli
} // namespace opensup