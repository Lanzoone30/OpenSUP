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

tc_t
tc_t::from_seconds(double seconds, fps_e fps)
{
    double adj_seconds = seconds;
    if (fps.denominator() != 1001) {
        // integer fps: no adjustment needed
    }
    int64_t f = static_cast<int64_t>(std::round(adj_seconds * fps.to_double()));
    return tc_t(f + 1, fps); // +1 for offset like Python version
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

double
tc_t::to_pts() const noexcept
{
    if (m_frames <= 0) return 0.0;
    double tpts = (static_cast<double>(m_frames - 1) / m_fps.to_double()) * MPEGTS_FREQ;
    return std::floor(tpts) / MPEGTS_FREQ;
}

double
tc_t::to_seconds() const noexcept
{
    return static_cast<double>(m_frames) / m_fps.to_double();
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

} // namespace common
} // namespace opensup
