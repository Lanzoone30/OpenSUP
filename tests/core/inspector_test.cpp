// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// 034/T004-T006: validate / inspect / stats over the versioned .sup fixture.
// GREEN on the known-good stream, RED once the test poisons the bytes.
// The same counters must come out of the read path and the encode path.

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "opensup/core/pgstats.h"
#include "opensup/read/inspect.h"
#include "opensup/read/sup_reader.h"

namespace opensup {
namespace read {
namespace {

std::string fixture_path(const char* name) {
    return std::string(OPENDSUP_FIXTURES_DIR) + "/inspector/" + name;
}

std::vector<uint8_t> fixture_bytes() {
    std::ifstream in(fixture_path("mini.sup"), std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
}

TEST(InspectorTest, ValidateAcceptsFixture) {
    auto stream = load_sup(fixture_path("mini.sup"));
    const auto rep = validate(stream);
    EXPECT_TRUE(rep.compliant) << (rep.findings.empty() ? "" : rep.findings.front().detail);
    EXPECT_EQ(rep.stats.display_sets, 2);
    EXPECT_EQ(rep.stats.epochs, 1);
    for (auto& f : rep.findings) EXPECT_FALSE(f.hard) << f.rule;
}

TEST(InspectorTest, ValidateRejectsOutOfRangePalette) {
    auto stream = load_sup(fixture_path("mini.sup"));
    auto pds = stream.display_sets[0].pds().front();
    media::palette_t pal = pds->to_palette();
    pal.set(0x01, media::palette_entry_t{255, 128, 128, 255}); // luma above 235
    pds->set_palette(pal);

    const auto rep = validate(stream);
    EXPECT_FALSE(rep.compliant);
    bool range = false;
    for (auto& f : rep.findings) range |= f.rule == "palette/range" && f.hard;
    EXPECT_TRUE(range);
}

TEST(InspectorTest, ValidateRejectsEpochStartMidEpoch) {
    auto stream = load_sup(fixture_path("mini.sup"));
    auto* pcs = static_cast<core::pcs_c*>(stream.display_sets[1].segments[0].get());
    pcs->set_composition_state(core::pcs_c::composition_state_e::epoch_start);
    pcs->update();

    const auto rep = validate(stream);
    EXPECT_FALSE(rep.compliant);
    bool mid_epoch = false;
    for (auto& f : rep.findings)
        mid_epoch |= f.rule == "epoch/start" && f.ds == 1;
    EXPECT_TRUE(mid_epoch);
}

TEST(InspectorTest, ValidateRejectsWindowOutsideScreen) {
    auto stream = load_sup(fixture_path("mini.sup"));
    auto* wds = static_cast<core::wds_c*>(stream.display_sets[0].segments[1].get());
    auto windows = wds->windows;
    windows[0].h_pos = 1900; // 1900 + 64 > 1920
    *wds = core::wds_c::from_scratch(windows);

    const auto rep = validate(stream);
    EXPECT_FALSE(rep.compliant);
    bool out = false;
    for (auto& f : rep.findings) out |= f.rule == "epoch/window" && f.hard;
    EXPECT_TRUE(out);
}

TEST(InspectorTest, RxBitrateCheckIsOptIn) {
    auto stream = load_sup(fixture_path("mini.sup"));
    // The fixture fits in the 1 MiB decoder buffer, so no underflow: the check
    // is wired but silent. Underflow itself is covered by interface_bitrate_test.
    const auto rep = validate(stream, 1000);
    EXPECT_EQ(rep.max_kbps, 1000);
    EXPECT_TRUE(rep.compliant);
    for (auto& f : rep.findings) EXPECT_NE(f.rule, "rx_bitrate");
}

TEST(InspectorTest, StatsMatchEncodeSideCounters) {
    auto stream = load_sup(fixture_path("mini.sup"));
    const auto st = stream_stats(stream);

    // Flatten what the reader produced and count it again: the encode side
    // (core::count_pg_segments over a flat list) must agree with the reader.
    std::vector<std::shared_ptr<core::pg_segment_c>> flat;
    for (auto& ds : stream.display_sets)
        for (auto& seg : ds.segments) flat.push_back(seg);
    const auto c = core::count_pg_segments(flat);

    EXPECT_EQ(c.epochs, st.epochs);
    EXPECT_EQ(c.display_sets, st.display_sets);
    EXPECT_EQ(c.segments, st.segments);
    EXPECT_EQ(c.es, st.es);
    EXPECT_EQ(c.acq, st.acq);
    EXPECT_EQ(c.nc, st.nc);
    EXPECT_EQ(c.nc_ods, st.nc_ods);
    EXPECT_EQ(c.pu, st.pu);
    EXPECT_EQ(c.pu_buffered, st.pu_buffered);
}

TEST(InspectorTest, StatsCensusOfFixture) {
    auto stream = load_sup(fixture_path("mini.sup"));
    const auto st = stream_stats(stream);
    EXPECT_EQ(st.es, 1);
    EXPECT_EQ(st.acq, 1);
    EXPECT_EQ(st.nc, 0);
    EXPECT_EQ(st.pds, 1);
    EXPECT_EQ(st.ods, 2);
    EXPECT_EQ(st.ends, 2);
    EXPECT_EQ(st.bytes, fixture_bytes().size());
}

TEST(InspectorTest, InspectExposesSegmentLabels) {
    auto stream = load_sup(fixture_path("mini.sup"));
    ASSERT_EQ(stream.display_sets.size(), 2u);
    const auto* pcs = static_cast<const core::pcs_c*>(stream.display_sets[1].segments[0].get());
    EXPECT_EQ(static_cast<uint8_t>(pcs->composition_state()), 0x40);
    EXPECT_EQ(pcs->composition_n(), 1);
}

}
}
}
