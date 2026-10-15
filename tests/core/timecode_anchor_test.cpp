// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <thread>
#include <vector>

#include "opensup/common/bdvideo.h"
#include "opensup/common/timecode.h"
#include "opensup/core/filestreams.h"
#include "opensup/core/interface.h"

using namespace opensup;
using common::tc_t;

namespace opensup {
namespace core {
// Defined in interface.cpp.
int physical_core_count();
}
}

namespace {
core::bdn_xml_event_c make_event(tc_t in, tc_t out, const std::string& img) {
    return core::bdn_xml_event_c(in, out, 0, 0, 10, 10, img);
}

// to_pts() yields seconds; recover the exact integer 90 kHz tick count.
int64_t pts_ticks(const tc_t& t) {
    return std::llround(t.to_pts() * common::MPEGTS_FREQ);
}
} // namespace

// PTS must be exact integer ticks; fractional rates (24000/1001) must not
// round through a float seconds value.
TEST(Timecode, FractionalFpsExactPts) {
    const common::fps_e ntsc(common::fps_e::film_ntsc);
    EXPECT_EQ(pts_ticks(tc_t::from_hmsf(0, 0, 1, 0, ntsc)), 90090);
    EXPECT_EQ(pts_ticks(tc_t::from_hmsf(0, 0, 3, 0, ntsc)), 270270);
    EXPECT_EQ(pts_ticks(tc_t::from_hmsf(0, 0, 3, 12, ntsc)), 315315);
}

TEST(Timecode, IntegerFpsPts) {
    const common::fps_e pal(common::fps_e::pal_p);
    EXPECT_EQ(pts_ticks(tc_t::from_hmsf(0, 0, 1, 0, pal)), 90000);
    EXPECT_EQ(pts_ticks(tc_t::from_hmsf(0, 0, 3, 0, pal)), 270000);
}

TEST(Timecode, NominalRate) {
    EXPECT_EQ(tc_t::nominal_rate(common::fps_e(common::fps_e::film_ntsc)), 24);
    EXPECT_EQ(tc_t::nominal_rate(common::fps_e(common::fps_e::ntsc_p)), 30);
    EXPECT_EQ(tc_t::nominal_rate(common::fps_e(common::fps_e::ntsc_i)), 60);
    EXPECT_EQ(tc_t::nominal_rate(common::fps_e(common::fps_e::pal_p)), 25);
}

TEST(AnchorInterval, SplitsByExactFrameCount) {
    const common::fps_e pal(common::fps_e::pal_p); // 25 fps
    // [1s, 9s) = 200 frames, period 1s -> frame period 25 -> 7 refreshes.
    std::vector<core::bdn_xml_event_c> ev{
        make_event(tc_t::from_hmsf(0, 0, 1, 0, pal), tc_t::from_hmsf(0, 0, 9, 0, pal), "")};
    auto [out, flags] = core::add_periodic_refreshes(ev, pal, 1.0);

    ASSERT_EQ(out.size(), 8u);
    int redraws = 0;
    for (bool f : flags) redraws += f ? 1 : 0;
    EXPECT_EQ(redraws, 7);
    EXPECT_EQ(out.front().in_tc().frames(), tc_t::from_hmsf(0, 0, 1, 0, pal).frames());
    EXPECT_EQ(out.back().out_tc().frames(), tc_t::from_hmsf(0, 0, 9, 0, pal).frames());
    for (size_t i = 1; i < out.size(); i++)
        EXPECT_EQ(out[i - 1].out_tc().frames(), out[i].in_tc().frames());
}

TEST(AnchorInterval, ExactBoundaryAtFractionalFps) {
    const common::fps_e ntsc(common::fps_e::film_ntsc); // 23.976
    // [1s, 3s) = 48 frames == 2 * frame period (24) -> exactly one refresh.
    std::vector<core::bdn_xml_event_c> ev{
        make_event(tc_t::from_hmsf(0, 0, 1, 0, ntsc), tc_t::from_hmsf(0, 0, 3, 0, ntsc), "")};
    auto [out, flags] = core::add_periodic_refreshes(ev, ntsc, 1.0);

    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].out_tc().frames(), out[1].in_tc().frames());
    EXPECT_TRUE(flags[1]);
}

TEST(AnchorInterval, AllFpsKeepContiguousBoundaries) {
    const common::fps_e rates[] = {
        common::fps_e(common::fps_e::film_ntsc), common::fps_e(common::fps_e::film),
        common::fps_e(common::fps_e::pal_p), common::fps_e(common::fps_e::ntsc_p),
        common::fps_e(common::fps_e::pal_i), common::fps_e(common::fps_e::ntsc_i),
        common::fps_e(common::fps_e::hfr_60)};
    for (const auto& fps : rates) {
        std::vector<core::bdn_xml_event_c> ev{
            make_event(tc_t::from_hmsf(0, 0, 1, 0, fps), tc_t::from_hmsf(0, 0, 11, 0, fps), "")};
        auto [out, flags] = core::add_periodic_refreshes(ev, fps, 1.0);
        EXPECT_EQ(out.size(), flags.size());
        EXPECT_EQ(out.front().in_tc().frames(), tc_t::from_hmsf(0, 0, 1, 0, fps).frames());
        EXPECT_EQ(out.back().out_tc().frames(), tc_t::from_hmsf(0, 0, 11, 0, fps).frames());
        EXPECT_GT(out.size(), 1u); // 10s splits at every supported rate
        for (size_t i = 1; i < out.size(); i++)
            EXPECT_EQ(out[i - 1].out_tc().frames(), out[i].in_tc().frames());
    }
}

TEST(Deduplication, FoldsContiguousIdenticalEvents) {
    const common::fps_e pal(common::fps_e::pal_p);
    const std::string a = OPENDSUP_FIXTURES_DIR "/00001.png";
    const std::string b = OPENDSUP_FIXTURES_DIR "/00002.png";

    std::vector<core::bdn_xml_event_c> ev{
        make_event(tc_t::from_hmsf(0, 0, 1, 0, pal), tc_t::from_hmsf(0, 0, 2, 0, pal), a),
        make_event(tc_t::from_hmsf(0, 0, 2, 0, pal), tc_t::from_hmsf(0, 0, 3, 0, pal), a),
        make_event(tc_t::from_hmsf(0, 0, 3, 0, pal), tc_t::from_hmsf(0, 0, 4, 0, pal), b),
        make_event(tc_t::from_hmsf(0, 0, 5, 0, pal), tc_t::from_hmsf(0, 0, 6, 0, pal), b),
    };
    auto out = core::remove_dupes(ev);

    // e0+e1 merge (identical, contiguous); e2 differs; e3 is identical but gapped.
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0].out_tc().frames(), tc_t::from_hmsf(0, 0, 3, 0, pal).frames());
    EXPECT_EQ(out[1].in_tc().frames(), tc_t::from_hmsf(0, 0, 3, 0, pal).frames());
    EXPECT_EQ(out[2].in_tc().frames(), tc_t::from_hmsf(0, 0, 5, 0, pal).frames());
}

TEST(Threads, AutoCountsPhysicalCores) {
    const int cores = core::physical_core_count();
    EXPECT_GE(cores, 1);
    EXPECT_LE(cores, static_cast<int>(std::thread::hardware_concurrency()));
}
