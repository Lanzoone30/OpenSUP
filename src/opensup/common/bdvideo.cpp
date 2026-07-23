#include "opensup/pch.h"
#include "opensup/common/bdvideo.h"

#include <cmath>
#include <algorithm>
#include <array>

namespace opensup {
namespace common {

double
fps_e::to_double() const noexcept
{
    return static_cast<double>(m_numer) / static_cast<double>(m_denom);
}

static constexpr std::array<std::pair<double, int32_t>, 7> s_pcs_lut = {{
    {23.976, 0x10}, {24.0, 0x20}, {25.0, 0x30},
    {29.97, 0x40},  {50.0, 0x60}, {59.94, 0x70},
    {60.0, 0x80},
}};

int32_t
fps_e::to_pcsfps() const noexcept
{
    double rfps = std::round(to_double() * 100.0) / 100.0;
    // ponytail: linear scan of 7 entries, cache if measured hot
    for (const auto& [fps, pcs] : s_pcs_lut) {
        if (std::abs(rfps - fps) < 0.01)
            return pcs;
    }
    return 0x10;
}

fps_e
fps_e::from_pcsfps(int32_t pcsfps)
{
    for (const auto& [fps, pcs] : s_pcs_lut) {
        if (pcs == pcsfps)
            return fps_e::from_double(fps);
    }
    return fps_e(fps_e::film_ntsc);
}

fps_e
fps_e::from_double(double fps)
{
    // ponytail: find nearest match within 0.07 tolerance
    static constexpr std::array<std::pair<double, fps_e>, 7> s_fps_list = {{
        {24000.0/1001.0, fps_e(fps_e::film_ntsc)},
        {24.0,           fps_e(fps_e::film)},
        {25.0,           fps_e(fps_e::pal_p)},
        {30000.0/1001.0, fps_e(fps_e::ntsc_p)},
        {50.0,           fps_e(fps_e::pal_i)},
        {60000.0/1001.0, fps_e(fps_e::ntsc_i)},
        {60.0,           fps_e(fps_e::hfr_60)},
    }};

    double best_diff = 1e9;
    fps_e best = fps_e(fps_e::film_ntsc);
    for (const auto& [val, fps_e_val] : s_fps_list) {
        double diff = std::abs(val - fps);
        if (diff < best_diff) {
            best_diff = diff;
            best = fps_e_val;
        }
    }
    if (best_diff >= 0.07)
        throw std::invalid_argument("Framerate is not BD compliant");
    return best;
}

video_format_info_t
get_format_info(video_format_e fmt) noexcept
{
    switch (fmt) {
        case video_format_e::hd1080:   return {1920, 1080};
        case video_format_e::hd720:    return {1280, 720};
        case video_format_e::sd576_43: return {720,  576};
        case video_format_e::sd480_43: return {720,  480};
    }
    return {0, 0};
}

bdvideo_c::bdvideo_c(fps_e fps, int32_t height, std::optional<int32_t> width)
    : m_fps(fps)
    , m_pcsfps(static_cast<pcsfps_e>(fps.to_pcsfps()))
{
    if (width.has_value()) {
        m_format = video_format_e::hd1080; // placeholder
    } else {
        m_format = std::nullopt;
        for (auto f : {video_format_e::hd1080, video_format_e::hd720,
                        video_format_e::sd576_43, video_format_e::sd480_43}) {
            if (get_format_info(f).height == height) {
                m_format = f;
                break;
            }
        }
    }
}

std::pair<bool, std::vector<double>>
bdvideo_c::check_format_fps(video_format_e fmt, fps_e fps) noexcept
{
    std::vector<double> expected;
    auto add_expected = [&](fps_e f) {
        expected.push_back(f.to_double());
    };

    bool valid = true;
    switch (fmt) {
        case video_format_e::hd720:
            add_expected(fps_e(fps_e::film_ntsc));
            add_expected(fps_e(fps_e::film));
            add_expected(fps_e(fps_e::pal_i));
            add_expected(fps_e(fps_e::ntsc_i));
            valid = (fps == fps_e(fps_e::film_ntsc) || fps == fps_e(fps_e::film) ||
                     fps == fps_e(fps_e::pal_i) || fps == fps_e(fps_e::ntsc_i));
            break;
        case video_format_e::sd576_43:
            add_expected(fps_e(fps_e::pal_p));
            valid = (fps == fps_e(fps_e::pal_p));
            break;
        case video_format_e::sd480_43:
            add_expected(fps_e(fps_e::ntsc_p));
            valid = (fps == fps_e(fps_e::ntsc_p));
            break;
        default:
            for (auto f : {fps_e(fps_e::film_ntsc), fps_e(fps_e::film),
                            fps_e(fps_e::pal_p), fps_e(fps_e::ntsc_p),
                            fps_e(fps_e::pal_i), fps_e(fps_e::ntsc_i),
                            fps_e(fps_e::hfr_60)}) {
                add_expected(f);
            }
            break;
    }
    return {valid, expected};
}

} // namespace common
} // namespace opensup
