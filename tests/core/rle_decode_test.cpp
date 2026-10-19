// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// 034/T002 (US4): decode_rle must invert encode_rle for every payload shape.

#include <gtest/gtest.h>

#include <vector>

#include "opensup/media/pgraphics.h"
#include "opensup/media/rle_decode.h"

namespace opensup {
namespace media {
namespace {

std::vector<uint8_t> roundtrip(const std::vector<uint8_t>& bitmap, int w, int h) {
    return decode_rle(encode_rle(bitmap, w, h), w, h);
}

TEST(RleDecodeTest, RoundtripsFlatImage) {
    const std::vector<uint8_t> bitmap(64 * 32, 1);
    EXPECT_EQ(roundtrip(bitmap, 64, 32), bitmap);
}

TEST(RleDecodeTest, RoundtripsTransparentImage) {
    const std::vector<uint8_t> bitmap(64 * 32, 0);
    EXPECT_EQ(roundtrip(bitmap, 64, 32), bitmap);
}

TEST(RleDecodeTest, RoundtripsBandedImage) {
    std::vector<uint8_t> bitmap(64 * 32, 0);
    for (int y = 8; y < 24; ++y)
        for (int x = 0; x < 64; ++x) bitmap[static_cast<size_t>(y) * 64 + x] = 1;
    EXPECT_EQ(roundtrip(bitmap, 64, 32), bitmap);
}

TEST(RleDecodeTest, RoundtripsLongRunsBeyondSixBitLength) {
    // 300 pixels needs the extended 14-bit run length.
    const std::vector<uint8_t> bitmap(300, 7);
    EXPECT_EQ(roundtrip(bitmap, 300, 1), bitmap);
}

TEST(RleDecodeTest, RoundtripsShortRuns) {
    const std::vector<uint8_t> bitmap{1, 1, 2, 3, 3, 3, 0, 0, 4};
    EXPECT_EQ(roundtrip(bitmap, 9, 1), bitmap);
}

TEST(RleDecodeTest, StatsCensusMatchesBitmap) {
    std::vector<uint8_t> bitmap(16 * 4, 0);
    for (int i = 0; i < 6; ++i) bitmap[static_cast<size_t>(i)] = 9;
    const auto st = rle_stats(encode_rle(bitmap, 16, 4), 16, 4);
    EXPECT_EQ(st.pixels, 16u * 4u);
    EXPECT_EQ(st.non_empty, 6u);
}

TEST(RleDecodeTest, CheckedRejectsTruncatedRun) {
    // A run header promising more pixels than the line holds.
    const std::vector<uint8_t> data{0x00, 0xC0, 0x40, 0x01, 0x00, 0x00};
    bool ok = true;
    decode_rle_checked(data, 8, 1, ok);
    EXPECT_FALSE(ok);
}

TEST(RleDecodeTest, CheckedAcceptsWellFormedPayload) {
    bool ok = false;
    const auto bitmap = decode_rle_checked(encode_rle(std::vector<uint8_t>(8, 3), 8, 1), 8, 1, ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(bitmap.size(), 8u);
}

}
}
}