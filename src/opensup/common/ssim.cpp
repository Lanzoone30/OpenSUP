// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/common/ssim.h"

namespace opensup {
namespace common {

namespace {

constexpr int kTileSize = 7;
constexpr double kDynamicRange = 255.0;
constexpr double kC1 = (kDynamicRange * 0.01) * (kDynamicRange * 0.01);
constexpr double kC2 = (kDynamicRange * 0.03) * (kDynamicRange * 0.03);

inline double variance(const uint8_t* pixels, int count, double mean) noexcept {
    double sum = 0.0;
    for (int i = 0; i < count; ++i) {
        double diff = static_cast<double>(pixels[i]) - mean;
        sum += diff * diff;
    }
    return sum / count;
}

inline double covariance(const uint8_t* a, const uint8_t* b, int count, double mean_a, double mean_b) noexcept {
    double sum = 0.0;
    for (int i = 0; i < count; ++i) {
        sum += (static_cast<double>(a[i]) - mean_a) * (static_cast<double>(b[i]) - mean_b);
    }
    return sum / count;
}

} // namespace

double
ssim_c::compare(const uint8_t* img1, const uint8_t* img2,
                int width, int height, int channels)
{
    if (width < kTileSize || height < kTileSize) {
        return 1.0;
    }

    const int tile_w = width / kTileSize * kTileSize;
    const int tile_h = height / kTileSize * kTileSize;
    const int pixel_len = kTileSize * kTileSize;

    double ssim_sum = 0.0;

    for (int y = 0; y < tile_h; y += kTileSize) {
        for (int x = 0; x < tile_w; x += kTileSize) {
            for (int c = 0; c < channels; ++c) {
                double sum1 = 0.0, sum2 = 0.0;

                for (int ty = 0; ty < kTileSize; ++ty) {
                    const int row = (y + ty) * width + x;
                    const int offset = row * channels + c;
                    for (int tx = 0; tx < kTileSize; ++tx) {
                        const int idx = offset + tx * channels;
                        sum1 += img1[idx];
                        sum2 += img2[idx];
                    }
                }

                const double mean1 = sum1 / pixel_len;
                const double mean2 = sum2 / pixel_len;

                double var1 = 0.0, var2 = 0.0, covar = 0.0;

                for (int ty = 0; ty < kTileSize; ++ty) {
                    const int row = (y + ty) * width + x;
                    const int offset = row * channels + c;
                    for (int tx = 0; tx < kTileSize; ++tx) {
                        const int idx = offset + tx * channels;
                        const double v1 = static_cast<double>(img1[idx]);
                        const double v2 = static_cast<double>(img2[idx]);
                        const double d1 = v1 - mean1;
                        const double d2 = v2 - mean2;
                        var1 += d1 * d1;
                        var2 += d2 * d2;
                        covar += d1 * d2;
                    }
                }

                var1 /= pixel_len;
                var2 /= pixel_len;
                covar /= pixel_len;

                const double numerator = (2.0 * mean1 * mean2 + kC1) * (2.0 * covar + kC2);
                const double denominator = (mean1 * mean1 + mean2 * mean2 + kC1) * (var1 + var2 + kC2);
                ssim_sum += numerator / denominator;
            }
        }
    }

    const int num_tiles = (tile_w / kTileSize) * (tile_h / kTileSize);
    const double total_pixels = static_cast<double>(channels) * num_tiles * pixel_len;
    return ssim_sum * pixel_len / total_pixels;
}

} // namespace common
} // namespace opensup