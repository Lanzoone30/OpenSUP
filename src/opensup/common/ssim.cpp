// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/common/ssim.h"

#include <vector>

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

// Convert RGBA to L (luminance) using the same weights PIL uses for 'L' mode.
// https://pillow.readthedocs.io/en/stable/reference/Image.html#PIL.Image.Image.convert
std::vector<uint8_t> rgba_to_luminance(const uint8_t* rgba, int width, int height) {
    const size_t np = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint8_t> luminance;
    luminance.reserve(np);
    for (size_t i = 0; i < np; ++i) {
        const size_t idx = i * 4;
        const double r = static_cast<double>(rgba[idx + 0]);
        const double g = static_cast<double>(rgba[idx + 1]);
        const double b = static_cast<double>(rgba[idx + 2]);
        const double y = 0.299 * r + 0.587 * g + 0.114 * b;
        luminance.push_back(static_cast<uint8_t>(std::clamp(y, 0.0, 255.0)));
    }
    return luminance;
}

// Approximate 5x5 Gaussian blur (separable, sigma ~1.0) matching cv2.GaussianBlur(ksize=5).
// Kernel: [1, 4, 6, 4, 1] / 16.
std::vector<uint8_t> gaussian_blur_5x5(const std::vector<uint8_t>& src, int width, int height) {
    const size_t np = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint16_t> tmp(np, 0);
    std::vector<uint8_t> dst(np, 0);

    const auto at = [width](int x, int y) {
        return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
    };

    // Horizontal pass
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t v = 0;
            for (int k = -2; k <= 2; ++k) {
                const int sx = std::clamp(x + k, 0, width - 1);
                const uint32_t weight = static_cast<uint32_t>(6 - std::abs(k) * 2);
                v += static_cast<uint32_t>(src[at(sx, y)]) * weight;
            }
            tmp[at(x, y)] = static_cast<uint16_t>(v);
        }
    }

    // Vertical pass + normalize by 16*16=256
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t v = 0;
            for (int k = -2; k <= 2; ++k) {
                const int sy = std::clamp(y + k, 0, height - 1);
                const uint32_t weight = static_cast<uint32_t>(6 - std::abs(k) * 2);
                v += static_cast<uint32_t>(tmp[at(x, sy)]) * weight;
            }
            dst[at(x, y)] = static_cast<uint8_t>(std::clamp(v / 256u, 0u, 255u));
        }
    }
    return dst;
}

// Approximate 3x3 Gaussian blur (separable), matching cv2.GaussianBlur(ksize=3).
// Kernel: [1, 2, 1] / 4 in each axis (equivalent to the 3x3 [1,2,1;2,4,2;1,2,1]/16).
std::vector<uint8_t> gaussian_blur_3x3(const std::vector<uint8_t>& src, int width, int height) {
    const size_t np = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint16_t> tmp(np, 0);
    std::vector<uint8_t> dst(np, 0);

    const auto at = [width](int x, int y) {
        return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
    };

    // Horizontal pass
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t v = 0;
            for (int k = -1; k <= 1; ++k) {
                const int sx = std::clamp(x + k, 0, width - 1);
                const uint32_t weight = static_cast<uint32_t>(2 - std::abs(k));
                v += static_cast<uint32_t>(src[at(sx, y)]) * weight;
            }
            tmp[at(x, y)] = static_cast<uint16_t>(v);
        }
    }

    // Vertical pass + normalize by 4*4=16
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t v = 0;
            for (int k = -1; k <= 1; ++k) {
                const int sy = std::clamp(y + k, 0, height - 1);
                const uint32_t weight = static_cast<uint32_t>(2 - std::abs(k));
                v += static_cast<uint32_t>(tmp[at(x, sy)]) * weight;
            }
            dst[at(x, y)] = static_cast<uint8_t>(std::clamp(v / 16u, 0u, 255u));
        }
    }
    return dst;
}

// Edge magnitude via 3x3 Sobel operators (Gx, Gy) followed by sqrt(Gx^2 + Gy^2).
// ponytail: SUPer uses cv2.Sobel(dx=1, dy=1, ksize=5) — the mixed diagonal
// derivative. Gradient magnitude is the canonical edge metric and is cheaper;
// behavioral parity is what matters (SUPer itself runs a different SSIM, SSIM_PIL).
// Upgrade path: hand-roll the ksize=5 mixed kernel if real-dataset decisions diverge.
std::vector<uint8_t> sobel_magnitude(const std::vector<uint8_t>& src, int width, int height) {
    const size_t np = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint8_t> dst(np, 0);

    const auto at = [width](int x, int y) {
        return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
    };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Read the 3x3 neighborhood with clamped borders (OpenCV default).
            const int xm = x > 0 ? x - 1 : 0;
            const int xp = x < width - 1 ? x + 1 : width - 1;
            const int ym = y > 0 ? y - 1 : 0;
            const int yp = y < height - 1 ? y + 1 : height - 1;

            const uint32_t tl = src[at(xm, ym)];
            const uint32_t tc = src[at(x,  ym)];
            const uint32_t tr = src[at(xp, ym)];
            const uint32_t ml = src[at(xm, y)];
            const uint32_t mr = src[at(xp, y)];
            const uint32_t bl = src[at(xm, yp)];
            const uint32_t bc = src[at(x,  yp)];
            const uint32_t br = src[at(xp, yp)];

            const int32_t gx = static_cast<int32_t>(tr + 2 * mr + br) -
                               static_cast<int32_t>(tl + 2 * ml + bl);
            const int32_t gy = static_cast<int32_t>(bl + 2 * bc + br) -
                               static_cast<int32_t>(tl + 2 * tc + tr);

            const double mag = std::sqrt(static_cast<double>(gx) * gx +
                                         static_cast<double>(gy) * gy);
            dst[at(x, y)] = static_cast<uint8_t>(std::clamp(mag, 0.0, 255.0));
        }
    }
    return dst;
}

} // namespace

double
ssim_c::compare_with_alpha(const uint8_t* img1, const uint8_t* img2,
                           int width, int height, double& cross_percentage)
{
    cross_percentage = 0.0;

    const size_t np = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (np == 0) {
        return 1.0;
    }

    // Intersect alpha planes: both alphas > 0.
    std::vector<uint8_t> mask(np, 0);
    size_t overlap_count = 0;
    for (size_t i = 0; i < np; ++i) {
        const bool a1 = img1[i * 4 + 3] > 0;
        const bool a2 = img2[i * 4 + 3] > 0;
        if (a1 && a2) {
            mask[i] = 255;
            ++overlap_count;
        }
    }
    cross_percentage = static_cast<double>(overlap_count) / static_cast<double>(np);

    if (overlap_count == 0) {
        return 0.0;  // SUPer: alpha intersection empty → score 0 (no similarity)
    }

    // Apply Gaussian blur to the mask and binarize, as SUPer does.
    mask = gaussian_blur_5x5(mask, width, height);
    for (auto& v : mask) {
        if (v > 0) {
            v = 255;
        }
    }

    // Convert to luminance and apply mask (a_bitmap & mask[:, :, None]).
    std::vector<uint8_t> l1 = rgba_to_luminance(img1, width, height);
    std::vector<uint8_t> l2 = rgba_to_luminance(img2, width, height);
    for (size_t i = 0; i < np; ++i) {
        if (mask[i] == 0) {
            l1[i] = 0;
            l2[i] = 0;
        }
    }

    double score = compare(l1.data(), l2.data(), width, height, 1);

    // Edge SSIM (SUPer _compare_f): blur the FULL luminance, take the gradient,
    // mask it, compare, and fuse with min() — the worse of the two decides.
    {
        std::vector<uint8_t> e1 = rgba_to_luminance(img1, width, height);
        std::vector<uint8_t> e2 = rgba_to_luminance(img2, width, height);
        e1 = gaussian_blur_3x3(e1, width, height);
        e2 = gaussian_blur_3x3(e2, width, height);
        e1 = sobel_magnitude(e1, width, height);
        e2 = sobel_magnitude(e2, width, height);
        for (size_t i = 0; i < np; ++i) {
            if (mask[i] == 0) {
                e1[i] = 0;
                e2[i] = 0;
            }
        }
        const double score_edge = compare(e1.data(), e2.data(), width, height, 1);
        score = std::min(score, score_edge);
    }

    return score;
}

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
