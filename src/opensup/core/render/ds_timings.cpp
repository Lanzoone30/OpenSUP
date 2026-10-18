// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include "opensup/pch.h"
#include "opensup/core/render/ds_timings.h"

#include "opensup/media/pgraphics.h"

#include <algorithm>
#include <map>
#include <set>

namespace opensup {
namespace core {

namespace {

using media::pg_decoder_t;

constexpr int64_t kFreq = static_cast<int64_t>(pg_decoder_t::FREQ);
constexpr int64_t kRd = static_cast<int64_t>(pg_decoder_t::RD);
constexpr int64_t kRc = static_cast<int64_t>(pg_decoder_t::RC);

/// Copy a plane region at the compose rate (RC); result in ticks.
[[nodiscard]] int64_t copy_ticks(int64_t area) noexcept {
    return (area * kFreq + kRc - 1) / kRc;
}

/// Decode an object at the object rate (RD); result in ticks.
[[nodiscard]] int64_t ods_ticks(int64_t area) noexcept {
    return (area * kFreq + kRd - 1) / kRd;
}

}  // namespace

ds_timing_t
compute_ds_timing(const ds_timing_input_t& in) noexcept
{
    // ponytail: wipe is the copy time of the summed area — one ceil at the end,
    // not the sum of per-window ceils (matches the render2.py oracle).
    std::map<uint8_t, int64_t> win_area;
    int64_t total_area = 0;
    for (const auto& wd : in.windows) {
        const int64_t area = static_cast<int64_t>(wd.width) * wd.height;
        win_area[wd.window_id] = area;
        total_area += area;
    }
    const int64_t wipe = copy_ticks(total_area);

    std::map<uint16_t, int64_t> obj_decode;
    for (const auto& [o_id, area] : in.ods)
        obj_decode[o_id] = ods_ticks(static_cast<int64_t>(area));

    int64_t decode = 0;
    if (in.state == pcs_c::composition_state_e::epoch_start) {
        decode = copy_ticks(static_cast<int64_t>(in.plane_area));
    } else {
        std::set<uint8_t> assigned;
        for (const auto& cobj : in.cobjects)
            assigned.insert(cobj.window_id);
        for (const auto& [wid, area] : win_area) {
            if (assigned.count(wid) == 0)
                decode += copy_ticks(area);
        }
    }

    if (!in.pal_flag) {
        const size_t n = in.cobjects.size();
        std::map<uint16_t, int64_t> pending = obj_decode;
        int64_t t_decoding = 0;
        for (size_t k = 0; k < n; k++) {
            const auto& cobj = in.cobjects[k];
            if (auto it = pending.find(cobj.o_id); it != pending.end()) {
                t_decoding += it->second;
                pending.erase(it);
            }
            // Two objects in the same window are copied together, once.
            if (k == 0 && n > 1 && in.cobjects[1].window_id == cobj.window_id)
                continue;
            int64_t warea = 0;
            if (auto it = win_area.find(cobj.window_id); it != win_area.end())
                warea = it->second;
            decode = std::max(decode, t_decoding) + copy_ticks(warea);
        }
    }

    // A window's DTS must not reach its PTS.
    int64_t all_win = 0;
    for (const auto& [wid, area] : win_area)
        all_win += copy_ticks(area);
    decode = std::max(decode, all_win + 1);

    const int64_t pts_ticks = static_cast<int64_t>(std::round(in.pts * pg_decoder_t::FREQ));
    ds_timing_t out;
    out.wipe_duration = static_cast<double>(wipe) / pg_decoder_t::FREQ;
    out.decode_duration = static_cast<double>(decode) / pg_decoder_t::FREQ;
    out.dts = static_cast<double>(pts_ticks - decode) / pg_decoder_t::FREQ;
    out.ods.reserve(in.ods.size());
    for (const auto& [o_id, area] : in.ods) {
        const double secs = static_cast<double>(obj_decode[o_id]) / pg_decoder_t::FREQ;
        out.ods.push_back({o_id, secs});
    }
    return out;
}

double
decode_base(const std::vector<window_definition_t>& windows, uint64_t container_area) noexcept
{
    int64_t duration = copy_ticks(static_cast<int64_t>(container_area));
    int64_t t_dec = 0;
    for (const auto& wd : windows) {
        const int64_t area = static_cast<int64_t>(wd.width) * wd.height;
        t_dec += ods_ticks(area);
        duration = std::max(t_dec, duration) + copy_ticks(area);
    }
    return static_cast<double>(duration) / pg_decoder_t::FREQ;
}

}  // namespace core
}  // namespace opensup
