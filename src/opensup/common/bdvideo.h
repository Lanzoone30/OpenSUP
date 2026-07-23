#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <utility>

namespace opensup {
namespace common {

class fps_e {
public:
    enum value_t : uint8_t {
        film_ntsc  = 0,  // 24000/1001 ≈ 23.976
        film       = 1,  // 24/1
        pal_p      = 2,  // 25/1
        ntsc_p     = 3,  // 30000/1001 ≈ 29.97
        pal_i      = 4,  // 50/1
        ntsc_i     = 5,  // 60000/1001 ≈ 59.94
        hfr_60     = 6,  // 60/1
    };

    constexpr fps_e() : m_val(film_ntsc), m_numer(24000), m_denom(1001) {}
    constexpr fps_e(value_t v)
        : m_val(v), m_numer(1), m_denom(1)
    {
        switch (v) {
            case film_ntsc: m_numer = 24000; m_denom = 1001; break;
            case film:      m_numer = 24;    m_denom = 1;    break;
            case pal_p:     m_numer = 25;    m_denom = 1;    break;
            case ntsc_p:    m_numer = 30000; m_denom = 1001; break;
            case pal_i:     m_numer = 50;    m_denom = 1;    break;
            case ntsc_i:    m_numer = 60000; m_denom = 1001; break;
            case hfr_60:    m_numer = 60;    m_denom = 1;    break;
        }
    }

    [[nodiscard]] constexpr value_t value() const noexcept { return m_val; }
    [[nodiscard]] constexpr int numerator() const noexcept { return m_numer; }
    [[nodiscard]] constexpr int denominator() const noexcept { return m_denom; }
    [[nodiscard]] double to_double() const noexcept;
    [[nodiscard]] int32_t to_pcsfps() const noexcept;

    static fps_e from_pcsfps(int32_t pcsfps);
    static fps_e from_double(double fps);

    constexpr bool operator==(const fps_e& other) const noexcept { return m_val == other.m_val; }
    constexpr bool operator!=(const fps_e& other) const noexcept { return m_val != other.m_val; }

    constexpr bool operator<(const fps_e& other) const noexcept {
        return static_cast<uint64_t>(m_numer) * static_cast<uint64_t>(other.m_denom) <
               static_cast<uint64_t>(other.m_numer) * static_cast<uint64_t>(m_denom);
    }
    constexpr bool operator>(const fps_e& other) const noexcept { return other < *this; }

private:
    value_t m_val;
    int m_numer;
    int m_denom;
};

enum class video_format_e : uint8_t {
    hd1080   = 0,  // 1920x1080
    hd720    = 1,  // 1280x720
    sd576_43 = 2,  // 720x576
    sd480_43 = 3,  // 720x480
};

struct video_format_info_t {
    int32_t width;
    int32_t height;
};

[[nodiscard]] video_format_info_t get_format_info(video_format_e fmt) noexcept;

enum class pcsfps_e : uint8_t {
    film_ntsc_p = 0x10,
    film_24p    = 0x20,
    pal_p       = 0x30,
    ntsc_p      = 0x40,
    pal_i       = 0x60,
    ntsc_i      = 0x70,
    hfr_60      = 0x80,
};

class bdvideo_c {
public:
    bdvideo_c(fps_e fps, int32_t height, std::optional<int32_t> width = std::nullopt);

    [[nodiscard]] fps_e fps() const noexcept { return m_fps; }
    [[nodiscard]] pcsfps_e pcsfps() const noexcept { return m_pcsfps; }
    [[nodiscard]] std::optional<video_format_e> format() const noexcept { return m_format; }

    static std::pair<bool, std::vector<double>>
    check_format_fps(video_format_e fmt, fps_e fps) noexcept;

private:
    fps_e m_fps;
    pcsfps_e m_pcsfps;
    std::optional<video_format_e> m_format;
};

} // namespace common
} // namespace opensup
