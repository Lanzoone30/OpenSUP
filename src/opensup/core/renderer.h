#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <optional>
#include <functional>

#include "opensup/core/segments.h"
#include "opensup/core/filestreams.h"
#include "opensup/media/optimizer.h"
#include "opensup/media/palette.h"
#include "opensup/common/geometry.h"

namespace opensup {
namespace core {

// ── CTU: recursive tile comparison ──
struct ctu_result_t {
    bool identical;
    common::box_t changed_region;
};

ctu_result_t compare_tiles(const uint8_t* a, const uint8_t* b,
                            int width, int height, int stride);

// ── Window Analyzer ──
struct window_analysis_t {
    common::box_t window;
    bool needs_redraw;
    double ssim_score;
};

class window_analyzer_c {
public:
    window_analyzer_c(double ssim_tol = 0.999);

    window_analysis_t analyze(const uint8_t* prev, const uint8_t* curr,
                               int width, int height, const common::box_t& window);

private:
    double m_ssim_tol;
};

// ── Padding Engine ──
std::vector<uint8_t> pad_image_8x8(const std::vector<uint8_t>& rgba,
                                    int width, int height,
                                    int& out_width, int& out_height);

// ── DS Node: timing model for a display set ──
class ds_node_t {
public:
    std::shared_ptr<display_set_t> ds;
    double pts_origin = 0.0;
    double dts_origin = 0.0;
    int cumulated_ods_size = 0;

    explicit ds_node_t(std::shared_ptr<display_set_t> display_set);

    void compute_timing(double fps, const common::box_t& window);
};

// ── Epoch Encoder ──
class epoch_encoder_c {
public:
    epoch_encoder_c(double fps, int width, int height, int quantizer_id = 0);

    std::vector<std::shared_ptr<pg_segment_c>>
    encode_epoch(const std::vector<bdn_xml_event_c>& events,
                  const std::vector<bool>& redraw_flags,
                  common::fps_e fps_enum,
                  int& palette_id_counter);

private:
    double m_fps;
    int m_width, m_height;
    int m_quantizer_id = 0;
    int m_composition_n = 1;
    int m_palette_vn = 0;
};

// ── Epoch Worker ──
struct epoch_job_t {
    std::vector<bdn_xml_event_c> events;
    std::vector<bool> redraw_flags;
    common::fps_e fps;
    int palette_base = 0;
};

struct epoch_result_t {
    std::vector<std::shared_ptr<pg_segment_c>> segments;
    int palette_base = 0;
};

} // namespace core
} // namespace opensup
