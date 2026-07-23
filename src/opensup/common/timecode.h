#pragma once

#include <cstdint>
#include <string>
#include <cmath>

#include "opensup/common/bdvideo.h"

namespace opensup {
namespace common {

constexpr uint64_t MPEGTS_FREQ = 90000;

class tc_t {
public:
    tc_t() = default;

    static tc_t from_seconds(double seconds, fps_e fps);
    static tc_t from_frames(int64_t frames, fps_e fps);
    static tc_t from_pts(double pts, fps_e fps);

    [[nodiscard]] int64_t frames() const noexcept { return m_frames; }
    [[nodiscard]] double to_pts() const noexcept;
    [[nodiscard]] double to_seconds() const noexcept;
    [[nodiscard]] fps_e fps() const noexcept { return m_fps; }

    tc_t operator+(const tc_t& other) const;
    tc_t operator+(int64_t delta_frames) const;
    tc_t& operator+=(const tc_t& other);
    tc_t& operator+=(int64_t delta_frames);

    bool operator==(const tc_t& other) const noexcept;
    bool operator!=(const tc_t& other) const noexcept { return !(*this == other); }

private:
    tc_t(int64_t frames, fps_e fps) : m_frames(frames), m_fps(fps) {}

    int64_t m_frames = 0;
    fps_e m_fps;
};

} // namespace common
} // namespace opensup
