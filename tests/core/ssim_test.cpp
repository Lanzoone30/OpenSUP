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

TEST(SSIM, EdgeSSIMIdentical) {
    // Fully opaque identical images: luminance and edge SSIM both 1.0.
    const int width = 28;
    const int height = 28;

    std::vector<uint8_t> img1(width * height * 4, 128);
    std::vector<uint8_t> img2 = img1;
    for (size_t i = 0; i < img1.size(); i += 4) {
        img1[i + 3] = 255;
        img2[i + 3] = 255;
    }

    double cross = 0.0;
    double score = opensup::common::ssim_c::compare_with_alpha(
        img1.data(), img2.data(), width, height, cross);
    EXPECT_DOUBLE_EQ(score, 1.0);
    EXPECT_DOUBLE_EQ(cross, 1.0);
}

TEST(SSIM, EdgeSSIMCatchesShiftedDetail) {
    // Two fully-opaque images differing only by a 1px vertical line (brightness
    // 200 over base 128) shifted one column. Same per-tile luminance histogram,
    // so the luminance SSIM alone stays high, while the Sobel edge maps of the
    // two lines are misaligned -> edge SSIM must pull the combined score down.
    // (SUPer fuses the two scores with min().)
    const int width = 28;
    const int height = 28;

    std::vector<uint8_t> img1(width * height * 4, 128);
    std::vector<uint8_t> img2(width * height * 4, 128);
    for (size_t i = 0; i < img1.size(); i += 4) {
        img1[i + 3] = 255;
        img2[i + 3] = 255;
    }

    const auto line = [&](std::vector<uint8_t>& img, int x) {
        for (int y = 0; y < height; ++y) {
            img[(y * width + x) * 4] = 200;
        }
    };
    line(img1, 10);
    line(img2, 11);

    double cross = 0.0;
    double score = opensup::common::ssim_c::compare_with_alpha(
        img1.data(), img2.data(), width, height, cross);
    EXPECT_LT(score, 0.99);       // structural change detected
    EXPECT_GT(score, 0.0);
    EXPECT_DOUBLE_EQ(cross, 1.0); // fully opaque -> full overlap
}