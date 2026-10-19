// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// 034/T001: the .sup reader maps bytes back to the existing segment structs.

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "opensup/core/segments.h"
#include "opensup/read/sup_reader.h"

namespace opensup {
namespace read {
namespace {

std::string fixture_path(const char* name) {
    return std::string(OPENDSUP_FIXTURES_DIR) + "/inspector/" + name;
}

TEST(SupReaderTest, ParsesMiniFixture) {
    const auto stream = load_sup(fixture_path("mini.sup"));
    ASSERT_TRUE(stream.errors.empty()) << stream.errors.front();
    EXPECT_TRUE(stream.well_formed());
    EXPECT_EQ(stream.display_sets.size(), 2u);
    EXPECT_EQ(stream.epochs(), 1u);
    EXPECT_EQ(stream.epoch_starts, std::vector<size_t>{0u});
}

TEST(SupReaderTest, MapsEverySegmentType) {
    const auto stream = load_sup(fixture_path("mini.sup"));
    ASSERT_EQ(stream.display_sets.size(), 2u);

    const auto& ds0 = stream.display_sets[0];
    ASSERT_EQ(ds0.segments.size(), 5u);
    EXPECT_EQ(ds0.segments[0]->type(), core::segment_type_e::pcs);
    EXPECT_EQ(ds0.segments[1]->type(), core::segment_type_e::wds);
    EXPECT_EQ(ds0.segments[2]->type(), core::segment_type_e::pds);
    EXPECT_EQ(ds0.segments[3]->type(), core::segment_type_e::ods);
    EXPECT_EQ(ds0.segments[4]->type(), core::segment_type_e::end);

    const auto* pcs = static_cast<const core::pcs_c*>(ds0.segments[0].get());
    EXPECT_EQ(pcs->video_width(), 1920);
    EXPECT_EQ(pcs->video_height(), 1080);
    EXPECT_EQ(static_cast<uint8_t>(pcs->composition_state()), 0x80);
    EXPECT_EQ(pcs->cobjects.size(), 1u);
    EXPECT_EQ(pcs->cobjects[0].o_id, 1);

    const auto* wds = static_cast<const core::wds_c*>(ds0.segments[1].get());
    ASSERT_EQ(wds->windows.size(), 1u);
    EXPECT_EQ(wds->windows[0].width, 64);
    EXPECT_EQ(wds->windows[0].height, 32);

    const auto* ods = static_cast<const core::ods_c*>(ds0.segments[3].get());
    EXPECT_EQ(ods->width(), 64);
    EXPECT_EQ(ods->height(), 32);
    EXPECT_EQ(static_cast<uint8_t>(ods->seq_flags()), 0xC0);
    EXPECT_FALSE(ods->data().empty());
}

TEST(SupReaderTest, ReportsUnreadableFile) {
    const auto stream = load_sup(fixture_path("does-not-exist.sup"));
    EXPECT_FALSE(stream.errors.empty());
    EXPECT_TRUE(stream.display_sets.empty());
    EXPECT_FALSE(stream.well_formed());
}

TEST(SupReaderTest, ReportsTrailingGarbage) {
    auto stream = load_sup(fixture_path("mini.sup"));
    ASSERT_TRUE(stream.errors.empty());

    // Re-parse with two bytes appended: they cannot start a segment.
    std::ifstream in(fixture_path("mini.sup"), std::ios::binary);
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    raw.push_back(0x00);
    raw.push_back(0x00);
    const auto trailing = parse_sup(raw);
    EXPECT_EQ(trailing.display_sets.size(), stream.display_sets.size());
    EXPECT_EQ(trailing.trailing_bytes, 2u);
    EXPECT_FALSE(trailing.well_formed());
}

}
}
}
