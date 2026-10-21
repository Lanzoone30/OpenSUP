// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/common/timecode.h"
#include "opensup/common/bdvideo.h"

namespace opensup {
namespace common {

int64_t
tc_t::nominal_rate(fps_e fps) noexcept
{
    if (fps.denominator() == 1001)
        return fps.numerator() / 1000;
    return fps.numerator() / fps.denominator();
}

tc_t
tc_t::from_seconds(double seconds, fps_e fps)
{
    int64_t f = static_cast<int64_t>(std::round(seconds * static_cast<double>(nominal_rate(fps))));
    return tc_t(f + 1, fps); // frames are 1-based
}

tc_t
tc_t::from_frames(int64_t frames, fps_e fps)
{
    return tc_t(frames, fps);
}

tc_t
tc_t::from_pts(double pts, fps_e fps)
{
    double seconds = pts / static_cast<double>(MPEGTS_FREQ);
    return from_seconds(seconds, fps);
}

tc_t
tc_t::from_hmsf(int h, int m, int s, int f, fps_e fps)
{
    const int64_t frames =
        static_cast<int64_t>(h * 3600 + m * 60 + s) * nominal_rate(fps) + f + 1;
    return tc_t(frames, fps);
}

double
tc_t::to_pts() const noexcept
{
    if (m_frames <= 1) return 0.0;
    // Integer 90 kHz ticks, floored: (frames-1) * 90000 * denom / numer.
    // Fractional rates (…/1001) must not round through a float seconds value.
    const int64_t ticks = ((m_frames - 1) * static_cast<int64_t>(MPEGTS_FREQ) *
                           m_fps.denominator()) / m_fps.numerator();
    return static_cast<double>(ticks) / static_cast<double>(MPEGTS_FREQ);
}

double
tc_t::to_seconds() const noexcept
{
    return static_cast<double>(m_frames - 1) / m_fps.to_double();
}

tc_t
tc_t::operator+(const tc_t& other) const
{
    return tc_t(m_frames + other.m_frames, m_fps);
}

tc_t
tc_t::operator+(int64_t delta_frames) const
{
    return tc_t(m_frames + delta_frames, m_fps);
}

tc_t&
tc_t::operator+=(const tc_t& other)
{
    m_frames += other.m_frames;
    return *this;
}

tc_t&
tc_t::operator+=(int64_t delta_frames)
{
    m_frames += delta_frames;
    return *this;
}

bool
tc_t::operator==(const tc_t& other) const noexcept
{
    return m_frames == other.m_frames && m_fps == other.m_fps;
}

}
}
