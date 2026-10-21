// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#pragma once

#include <cstdint>
#include <string>
#include <cmath>

#include "opensup/common/bdvideo.h"

namespace opensup {
namespace common {

constexpr uint64_t MPEGTS_FREQ = 90000;

/**
 * @brief Timecode value tied to a frame rate, convertible to PTS.
 *
 * Stores time internally as a frame count so arithmetic is exact; PTS and
 * seconds are derived on demand.
 */
class tc_t {
public:
    tc_t() = default;

    /// Build a timecode from a time in seconds at the given frame rate.
    static tc_t from_seconds(double seconds, fps_e fps);
    /// Build a timecode from a raw frame count at the given frame rate.
    static tc_t from_frames(int64_t frames, fps_e fps);
    /// Build a timecode from a MPEG PTS (90 kHz clock) at the given frame rate.
    static tc_t from_pts(double pts, fps_e fps);
    /// Build a timecode from HH:MM:SS:FF fields. Frames are counted at the
    /// nominal integer rate; fractional rates (…/1001) keep 24/30/60.
    static tc_t from_hmsf(int h, int m, int s, int f, fps_e fps);

    /// Nominal frames-per-second used for timecode arithmetic: 24/30/60 for
    /// NTSC ratios, the exact integer rate otherwise.
    static int64_t nominal_rate(fps_e fps) noexcept;

    /// Frame count at the stored frame rate.
    [[nodiscard]] int64_t frames() const noexcept { return m_frames; }
    /// Presentation time in seconds, derived from the exact floored PTS tick count.
    [[nodiscard]] double to_pts() const noexcept;
    /// Time in seconds for this timecode.
    [[nodiscard]] double to_seconds() const noexcept;
    /// Frame rate this timecode was created with.
    [[nodiscard]] fps_e fps() const noexcept { return m_fps; }

    /// Shift this timecode by another (same-rate) timecode.
    tc_t operator+(const tc_t& other) const;
    /// Shift this timecode by a frame delta.
    tc_t operator+(int64_t delta_frames) const;
    /// In-place shift by another (same-rate) timecode.
    tc_t& operator+=(const tc_t& other);
    /// In-place shift by a frame delta.
    tc_t& operator+=(int64_t delta_frames);

    /// True when both timecodes reference the same instant (frame count).
    bool operator==(const tc_t& other) const noexcept;
    bool operator!=(const tc_t& other) const noexcept { return !(*this == other); }

private:
    tc_t(int64_t frames, fps_e fps) : m_frames(frames), m_fps(fps) {}

    int64_t m_frames = 0;
    fps_e m_fps;
};

}
}
