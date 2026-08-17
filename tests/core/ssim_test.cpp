// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include <gtest/gtest.h>
#include "opensup/common/ssim.h"

TEST(SSIM, IdenticalImagesReturnOne) {
    const int width = 28;   // multiple of 7
    const int height = 28;
    const int channels = 4;

    std::vector<uint8_t> img1(width * height * channels, 128);
    std::vector<uint8_t> img2 = img1;

    double score = opensup::common::ssim_c::compare(img1.data(), img2.data(),
                                                     width, height, channels);
    EXPECT_DOUBLE_EQ(score, 1.0);
}

TEST(SSIM, DifferentImagesReturnLessThanOne) {
    const int width = 28;
    const int height = 28;
    const int channels = 4;

    std::vector<uint8_t> img1(width * height * channels, 100);
    std::vector<uint8_t> img2(width * height * channels, 200);

    double score = opensup::common::ssim_c::compare(img1.data(), img2.data(),
                                                     width, height, channels);
    EXPECT_LT(score, 1.0);
    EXPECT_GT(score, 0.0);
}

TEST(SSIM, PartiallyDifferentImages) {
    const int width = 28;
    const int height = 28;
    const int channels = 4;

    std::vector<uint8_t> img1(width * height * channels, 128);
    std::vector<uint8_t> img2 = img1;
    // Change one quadrant
    for (int y = 0; y < 14; ++y) {
        for (int x = 0; x < 14; ++x) {
            for (int c = 0; c < channels; ++c) {
                img2[(y * width + x) * channels + c] = 200;
            }
        }
    }

    double score = opensup::common::ssim_c::compare(img1.data(), img2.data(),
                                                     width, height, channels);
    EXPECT_LT(score, 1.0);
    EXPECT_GT(score, 0.5);  // Still somewhat similar
}

TEST(SSIM, SingleChannel) {
    const int width = 28;
    const int height = 28;
    const int channels = 1;

    std::vector<uint8_t> img1(width * height * channels, 100);
    std::vector<uint8_t> img2(width * height * channels, 100);

    double score = opensup::common::ssim_c::compare(img1.data(), img2.data(),
                                                     width, height, channels);
    EXPECT_DOUBLE_EQ(score, 1.0);
}

TEST(SSIM, SmallImageBelowTileSize) {
    const int width = 5;
    const int height = 5;
    const int channels = 4;

    std::vector<uint8_t> img1(width * height * channels, 100);
    std::vector<uint8_t> img2(width * height * channels, 200);

    double score = opensup::common::ssim_c::compare(img1.data(), img2.data(),
                                                     width, height, channels);
    // Below tile size -> returns 1.0 (fallback)
    EXPECT_DOUBLE_EQ(score, 1.0);
}

TEST(SSIM, NonMultipleOfTileSize) {
    const int width = 30;  // not multiple of 7
    const int height = 30;
    const int channels = 4;

    std::vector<uint8_t> img1(width * height * channels, 128);
    std::vector<uint8_t> img2 = img1;

    double score = opensup::common::ssim_c::compare(img1.data(), img2.data(),
                                                     width, height, channels);
    EXPECT_DOUBLE_EQ(score, 1.0);
}