#pragma once

#include <string>

#include "opensup/core/interface.h"

namespace opensup {
namespace cli {

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
    double redraw_period = 0.0;
};

cli_options_t parse_args(int argc, char** argv);
core::encode_config_t options_to_config(const cli_options_t& opts);

} // namespace cli
} // namespace opensup
