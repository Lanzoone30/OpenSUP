#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "opensup/core/filestreams.h"
#include "opensup/core/renderer.h"

namespace opensup {
namespace core {

struct encode_result_t {
    bool success = false;
    std::string error;
    int events = 0;
    int epochs = 0;
    int segments = 0;
    int64_t duration_ms = 0;
};

struct encode_config_t {
    std::string input_path;
    std::string output_path;
    double fps = 24.0;
    int width = 1920;
    int height = 1080;
    int quantizer_id = 0;  // 0 = libimagequant, 1 = hextree
    bool overwrite = false;
    bool ignore_resolution = false;
    bool both_formats = false;
    bool allow_normal_case = false;
    bool overlap = false;
    std::string bt_matrix = "bt709";
    double ssim_tol = 0.0;
    bool full_palette = false;
    double redraw_period = 0.0;
};

class epoch_worker_c {
public:
    epoch_worker_c(std::queue<epoch_job_t>& jobs,
                   std::mutex& mutex,
                   std::condition_variable& cv,
                   std::atomic<bool>& done,
                   double fps, int width, int height);

    void operator()();

private:
    std::queue<epoch_job_t>& m_jobs;
    std::mutex& m_mutex;
    std::condition_variable& m_cv;
    std::atomic<bool>& m_done;
    double m_fps;
    int m_width, m_height;
};

class bdn_render_c {
public:
    explicit bdn_render_c(const encode_config_t& config);

    encode_result_t execute();

    [[nodiscard]] const std::vector<std::shared_ptr<pg_segment_c>>& segments() const noexcept {
        return m_segments;
    }

private:
    encode_config_t m_config;
    std::vector<std::shared_ptr<pg_segment_c>> m_segments;
};

} // namespace core
} // namespace opensup
