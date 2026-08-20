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

// Deterministic LCG mirroring the Python generator used to build the SUPer
// parity reference vectors (/tmp parity_vectors in the reference session).
static uint32_t lcg_next(uint32_t& state) {
    state = (state * 1103515245u + 12345u) & 0x7FFFFFFFu;
    return state;
}

// Rebuild one RGBA pair (composite, current) exactly as Python did, with a 4-blob
// layout, a 2px horizontal roll and a 50-pixel noise band. Expected score/cross
// are hard-coded from the SUPer render2.py _compare_f reference (SSIM_PIL tiles,
// cv2 3x3 blur + cv2.Sobel(ksize=5, dx=1, dy=1), min() fusion).
struct ParityCase {
    int w;
    int h;
    uint32_t seed;
    double score;
    double cross;
};

TEST(SSIM, MatchesSUPerReference) {
    const ParityCase cases[] = {
        {35, 28, 11, 0.24567014216752292, 0.6520408163265307},
        {49, 21, 22, 0.5448118943843079, 0.5140913508260447},
        {14, 14, 33, 0.15768977473250165, 0.7959183673469388},
        {28, 42, 44, 0.6693661651589864, 0.33163265306122447},
    };

    for (const auto& tc : cases) {
        const int W = tc.w;
        const int H = tc.h;
        std::vector<uint8_t> compo(static_cast<size_t>(W) * H * 4, 0);
        std::vector<uint8_t> cur(compo.size(), 0);

        const auto set_px = [W](std::vector<uint8_t>& img, int x, int y,
                                int r, int g, int b, int a) {
            size_t idx = (static_cast<size_t>(y) * W + static_cast<size_t>(x)) * 4;
            img[idx + 0] = static_cast<uint8_t>(r);
            img[idx + 1] = static_cast<uint8_t>(g);
            img[idx + 2] = static_cast<uint8_t>(b);
            img[idx + 3] = static_cast<uint8_t>(a);
        };

        uint32_t state = tc.seed;
        for (int blob = 0; blob < 4; ++blob) {
            const int x = static_cast<int>(lcg_next(state)) % (W - 10);
            const int y = static_cast<int>(lcg_next(state)) % (H - 10);
            const int w = 6 + static_cast<int>(lcg_next(state)) % 8;
            const int h = 6 + static_cast<int>(lcg_next(state)) % 8;
            const int r = static_cast<int>(lcg_next(state)) % 256;
            const int g = static_cast<int>(lcg_next(state)) % 256;
            const int b = static_cast<int>(lcg_next(state)) % 256;
            const int a = 128 + static_cast<int>(lcg_next(state)) % 128;
            // numpy slice-assignment clips to the array bounds (broadcast)
            const int x_end = (x + w < W) ? x + w : W;
            const int y_end = (y + h < H) ? y + h : H;
            for (int dy = y; dy < y_end; ++dy) {
                for (int dx = x; dx < x_end; ++dx) {
                    set_px(compo, dx, dy, r, g, b, a);
                }
            }
        }

        // cur = numpy roll(compo, 2, axis=1): cur[.., j] = compo[.., (j-2) mod W]
        for (int y = 0; y < H; ++y) {
            for (int j = 0; j < W; ++j) {
                const int src_x = (j - 2 + W) % W;
                size_t dst = (static_cast<size_t>(y) * W + static_cast<size_t>(j)) * 4;
                size_t src = (static_cast<size_t>(y) * W + static_cast<size_t>(src_x)) * 4;
                for (int c = 0; c < 4; ++c) cur[dst + c] = compo[src + c];
            }
        }

        // 50-pixel noise band on the red channel: yk=(k*7)%H, xk=(k*5)%W
        for (int k = 0; k < 50; ++k) {
            const int yk = (k * 7) % H;
            const int xk = (k * 5) % W;
            size_t idx = (static_cast<size_t>(yk) * W + static_cast<size_t>(xk)) * 4;
            cur[idx + 0] = static_cast<uint8_t>((static_cast<int>(cur[idx + 0]) + 100) % 256);
        }

        double cross = 0.0;
        const double score = opensup::common::ssim_c::compare_with_alpha(
            compo.data(), cur.data(), W, H, cross);

        EXPECT_NEAR(score, tc.score, 1e-6) << "case " << W << "x" << H;
        EXPECT_NEAR(cross, tc.cross, 1e-6) << "case " << W << "x" << H;
    }
}