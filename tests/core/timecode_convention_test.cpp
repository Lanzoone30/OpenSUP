// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "opensup/common/logger.h"
#include "opensup/common/timecode.h"
#include "opensup/core/filestreams.h"

using namespace opensup;
using common::tc_t;

// Locks the SMPTE non-drop-frame (NDF) timecode convention used for BDN
// InTC/OutTC (issue #3 "Temporal drift in 23.976 fps content"): for fractional
// NTSC rates such as 24000/1001, one second of timecode is 24 nominal frames,
// which is 1.001 s of real time. Timecodes written in real time (1 TC second
// = 1 s of video) instead advance the encoded PTS by +0.1% (~6.6 s over 1h50m).
// No automatic detection is possible: both conventions produce identical
// HH:MM:SS:FF strings once parsed.

namespace {

namespace fs = std::filesystem;

int64_t pts_ticks(const tc_t& t) {
    return std::llround(t.to_pts() * common::MPEGTS_FREQ);
}

void
write_bdn(const std::string& path, const std::string& events,
          const std::string& rate = "24000/1001")
{
    std::ofstream out(path);
    out << R"(<?xml version="1.0" encoding="UTF-8"?>
<BDN Version="0.012" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="BD-03-006-006-01-xx-BDN.xml">
<Description>
<Format VideoFormat="1920x1080" FrameRate=")" << rate << R"(" DropFrame="false" Type="Graphic"/>
</Description>
<Events>
)" << events << R"(</Events>
</BDN>
)";
}

} // namespace

// The BDN parse path must use the NDF nominal convention: 00:00:01:00 at
// 24000/1001 is 24 frames = 90090 ticks (1.001 s), and 01:50:00:00 is
// 594594000 ticks (6606.6 s real). These are the values a nominal-convention
// exporter (SUPer, avs2bdnxml, Subtitle Edit >= 4.0.1) produces and consumes.
TEST(BdnNdfConvention, ParseUsesNominalRate)
{
    const std::string folder = std::string(OPENDSUP_TEST_OUTPUT_DIR) + "/bdn_ndf";
    fs::create_directories(folder);
    const std::string path = folder + "/ndf.xml";
    write_bdn(path,
        "<Event Forced=\"false\" InTC=\"00:00:01:00\" OutTC=\"00:00:04:00\">"
        "<Graphic Width=\"64\" Height=\"64\" X=\"10\" Y=\"10\">missing.png</Graphic></Event>\n"
        "<Event Forced=\"false\" InTC=\"01:50:00:00\" OutTC=\"01:50:04:00\">"
        "<Graphic Width=\"64\" Height=\"64\" X=\"10\" Y=\"10\">missing.png</Graphic></Event>\n");

    core::bdn_xml_c xml;
    ASSERT_TRUE(xml.parse(path));
    const auto& events = xml.events();
    ASSERT_EQ(events.size(), 2u);

    EXPECT_EQ(pts_ticks(events[0].in_tc()), 90090);
    EXPECT_EQ(pts_ticks(events[1].in_tc()), 594594000);

    // Durations of nominal-convention TCs are also 1.001× real time, hence a
    // 3s TC event is 3.003 s real.
    EXPECT_NEAR(events[0].tc_out() - events[0].tc_in(), 3.003, 1e-6);
}

// A conformant exporter (frame count n = round(real_s * 24000/1001) split at
// the nominal integer rate) must round-trip exactly within half a real frame
// (≈ 21 ms at 23.976) over feature-length spans.
TEST(BdnNdfConvention, NominalRoundTripIsExact)
{
    const common::fps_e ntsc(common::fps_e::film_ntsc);
    constexpr int64_t NOMINAL = 24; // nominal rate of 24000/1001

    for (const double real_s : {0.0, 60.0, 6600.0, 7440.0, 7200.0 + 37.0 / 24.0}) {
        const auto n = std::llround(real_s * NOMINAL * 1000.0 / 1001.0);
        const int f = static_cast<int>(n % NOMINAL);
        const int64_t nom_sec = n / NOMINAL;
        const auto tc = tc_t::from_hmsf(
            static_cast<int>(nom_sec / 3600),
            static_cast<int>((nom_sec / 60) % 60),
            static_cast<int>(nom_sec % 60), f, ntsc);

        const double pts = tc.to_pts();
        EXPECT_NEAR(pts - real_s, 0.0, 0.5 / 24.0 + 1e-6) << "real_s=" << real_s;
        // Exact integer tick arithmetic; no float regression like the
        // response would have with e.g. llround(pts) of a double drift.
        EXPECT_EQ(pts_ticks(tc), std::llround(pts * common::MPEGTS_FREQ));
    }
}

// Same total time, but written the way real-time exporters do (pre-2023
// Subtitle Edit and similar tools): seconds are floor(real seconds) and the
// frames field is the fractional fps remainder. Every one of those TC seconds
// stretches to 1.001 s of PTS: drift grows linearly as ~t * 0.001 (+6.6 s at
// 1h50m; +7.4 s at 2h07m). This is the behavior reported in issue #3.
TEST(BdnNdfConvention, RealTimeWrittenTcDriftsLinearly)
{
    const common::fps_e ntsc(common::fps_e::film_ntsc);

    for (const double real_s : {10.0, 60.0, 3600.0, 6600.0, 7440.0}) {
        const double frac = real_s - std::floor(real_s);
        const int f = static_cast<int>(std::min(std::round(frac * 23.976), 23.0));
        const int64_t sec = static_cast<int64_t>(std::floor(real_s));
        const auto tc = tc_t::from_hmsf(
            static_cast<int>(sec / 3600),
            static_cast<int>((sec / 60) % 60),
            static_cast<int>(sec % 60), f, ntsc);

        const double drift = tc.to_pts() - real_s;
        EXPECT_NEAR(drift, real_s * 0.001, 0.5 / 24.0 + 1e-6) << "real_s=" << real_s;
    }
}

// The same convention holds for the other fractional NTSC rates (29.97, 59.94):
// 1 TC second = 30 or 60 nominal frames = 1.001 s real.
TEST(BdnNdfConvention, NominalRateForAllNtscFractions)
{
    const common::fps_e ntsc_p(common::fps_e::ntsc_p);
    const common::fps_e ntsc_i(common::fps_e::ntsc_i);

    EXPECT_EQ(tc_t::nominal_rate(ntsc_p), 30);
    EXPECT_EQ(pts_ticks(tc_t::from_hmsf(0, 0, 1, 0, ntsc_p)), 90090);

    EXPECT_EQ(tc_t::nominal_rate(ntsc_i), 60);
    EXPECT_EQ(pts_ticks(tc_t::from_hmsf(0, 0, 1, 0, ntsc_i)), 90090);
}

// Fractional NTSC rates are parsed as NDF timecode; integer rates stay silent.
TEST(BdnNdfConvention, WarnsOnFractionalNtscRate)
{
    const std::string folder = std::string(OPENDSUP_TEST_OUTPUT_DIR) + "/bdn_ndf_warn";
    fs::create_directories(folder);
    const std::string events =
        "<Event Forced=\"false\" InTC=\"00:00:01:00\" OutTC=\"00:00:04:00\">"
        "<Graphic Width=\"64\" Height=\"64\" X=\"10\" Y=\"10\">missing.png</Graphic></Event>\n";

    auto& log = common::logger_c::instance();
    std::vector<std::string> warnings;
    log.set_callback([&](const std::string& msg, int level) {
        if (level == static_cast<int>(common::log_level_e::warn)) warnings.push_back(msg);
    });

    const std::string ntsc_path = folder + "/ntsc.xml";
    write_bdn(ntsc_path, events);
    core::bdn_xml_c ntsc;
    const bool ntsc_parsed = ntsc.parse(ntsc_path);
    const auto ntsc_warnings = warnings.size();

    const std::string pal_path = folder + "/pal.xml";
    write_bdn(pal_path, events, "25");
    core::bdn_xml_c pal;
    const bool pal_parsed = pal.parse(pal_path);
    log.set_callback(nullptr);

    ASSERT_TRUE(ntsc_parsed);
    ASSERT_TRUE(pal_parsed);
    ASSERT_EQ(ntsc_warnings, 1u);
    EXPECT_EQ(warnings.size(), ntsc_warnings);
    EXPECT_NE(warnings[0].find("non-drop-frame timecode"), std::string::npos);
    EXPECT_NE(warnings[0].find("1.001 s"), std::string::npos);
}
