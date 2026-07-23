#include "opensup/pch.h"
#include "opensup/cli/cli_parser.h"

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

    app.add_option("-c,--compression", opts.compression,
                    "Compression rate [0-100] (def: 80)")
        ->check(CLI::Range(0.0, 100.0));

    app.add_option("-a,--acqrate", opts.acqrate,
                    "Acquisition rate [0-100] (def: 100)")
        ->check(CLI::Range(0.0, 100.0));

    app.add_option("-q,--quantizer", opts.quantizer,
                    "Quantizer [0:libimagequant, 1:HexTree] (def: 0)");

    app.add_option("-b,--bt", opts.bt_matrix,
                    "BT matrix [bt601, bt709, bt2020] (def: bt709)");

    app.add_flag("-y,--yes", opts.overwrite,
                  "Overwrite existing output file");

    app.add_option("-t,--threads", opts.threads,
                    "Thread count (0 = auto, def: 0)");

    app.add_option("--ssim-tol", opts.ssim_tol,
                    "SSIM tolerance [0-100] (def: 0)")
        ->check(CLI::Range(0.0, 100.0));

    app.add_flag("--ignore-resolution", opts.ignore_resolution,
                  "Accept non-standard video resolutions");

    app.add_flag("-w,--withsup", opts.both_formats,
                  "Output both SUP and PES/MUI formats");

    app.add_flag("-p,--palette", opts.full_palette,
                  "Use full palette for all epochs");

    app.add_option("--redraw-period", opts.redraw_period,
                    "Periodic redraw in seconds (0 = disable)");

    app.set_version_flag("-v,--version", "OpenSUP v1.0.0");

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
    cfg.compression = opts.compression / 100.0;
    cfg.acquisition_rate = opts.acqrate / 100.0;
    cfg.quantizer_id = opts.quantizer;
    cfg.threads = (opts.threads == 0) ? 4 : opts.threads;
    cfg.overwrite = opts.overwrite;
    cfg.ignore_resolution = opts.ignore_resolution;
    cfg.both_formats = opts.both_formats;
    return cfg;
}

} // namespace cli
} // namespace opensup
