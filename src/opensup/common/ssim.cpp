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

// Convert RGBA to L (luminance) with PIL's exact fixed-point formula for 'L'
// mode (libImaging L24: (R*19595 + G*38470 + B*7471 + 0x8000) >> 16, with
// rounding). Verified bit-exact against PIL convert('L').
std::vector<uint8_t> rgba_to_luminance(const uint8_t* rgba, int width, int height) {
    const size_t np = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint8_t> luminance;
    luminance.reserve(np);
    for (size_t i = 0; i < np; ++i) {
        const size_t idx = i * 4;
        const uint32_t y = static_cast<uint32_t>(rgba[idx + 0]) * 19595u +
                           static_cast<uint32_t>(rgba[idx + 1]) * 38470u +
                           static_cast<uint32_t>(rgba[idx + 2]) * 7471u + 32768u;
        luminance.push_back(static_cast<uint8_t>(y >> 16));
    }
    return luminance;
}

// cv2.BORDER_REFLECT_101 (cv2 default border), valid for |offset| <= n-1;
// clamp fallback for degenerate sizes.
inline int reflect_101(int i, int n) {
    if (i < 0) {
        i = -i;
    }
    if (i >= n) {
        i = 2 * (n - 1) - i;
    }
    return std::clamp(i, 0, n - 1);
}

// Exact cv2.GaussianBlur(ksize=5, sigma=0) kernel: [1,4,6,4,1]/16
// (verified: cv2.getGaussianKernel(5, 0) returns exactly this binomial).
// Separable, BORDER_REFLECT_101, horizontal pass stays scaled (no rounding),
// single rounding descale by 16*16=256 at the end (OpenCV fixed-point math).
std::vector<uint8_t> gaussian_blur_5x5(const std::vector<uint8_t>& src, int width, int height) {
    const size_t np = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint32_t> tmp(np, 0);
    std::vector<uint8_t> dst(np, 0);

    const auto at = [width](int x, int y) {
        return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
    };

    // Horizontal pass (scaled by 16, exact)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t v = 0;
            for (int k = -2; k <= 2; ++k) {
                const int sx = reflect_101(x + k, width);
                const uint32_t weight = static_cast<uint32_t>(6 - std::abs(k) * 2);
                v += static_cast<uint32_t>(src[at(sx, y)]) * weight;
            }
            tmp[at(x, y)] = v;
        }
    }

    // Vertical pass + rounded descale by 256 (CV_DESCALE-style round half up)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t v = 0;
            for (int k = -2; k <= 2; ++k) {
                const int sy = reflect_101(y + k, height);
                const uint32_t weight = static_cast<uint32_t>(6 - std::abs(k) * 2);
                v += tmp[at(x, sy)] * weight;
            }
            dst[at(x, y)] = static_cast<uint8_t>(std::clamp((v + 128u) >> 8, 0u, 255u));
        }
    }
    return dst;
}

// Exact cv2.GaussianBlur(ksize=3, sigma=0) kernel: [1,2,1]/4
// (verified: cv2.getGaussianKernel(3, 0) returns exactly this binomial).
// Separable, BORDER_REFLECT_101, single rounding descale by 4*4=16.
std::vector<uint8_t> gaussian_blur_3x3(const std::vector<uint8_t>& src, int width, int height) {
    const size_t np = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint32_t> tmp(np, 0);
    std::vector<uint8_t> dst(np, 0);

    const auto at = [width](int x, int y) {
        return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
    };

    // Horizontal pass (scaled by 4, exact)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t v = 0;
            for (int k = -1; k <= 1; ++k) {
                const int sx = reflect_101(x + k, width);
                const uint32_t weight = static_cast<uint32_t>(2 - std::abs(k));
                v += static_cast<uint32_t>(src[at(sx, y)]) * weight;
            }
            tmp[at(x, y)] = v;
        }
    }

    // Vertical pass + rounded descale by 16 (CV_DESCALE-style round half up)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t v = 0;
            for (int k = -1; k <= 1; ++k) {
                const int sy = reflect_101(y + k, height);
                const uint32_t weight = static_cast<uint32_t>(2 - std::abs(k));
                v += tmp[at(x, sy)] * weight;
            }
            dst[at(x, y)] = static_cast<uint8_t>(std::clamp((v + 8u) >> 4, 0u, 255u));
        }
    }
    return dst;
}

// cv2.Sobel(src, ddepth=cv2.CV_8U, dx=1, dy=1, ksize=5) equivalent (SUPer
// render2.py:1291-1292): mixed second derivative, kernel
// K[i][j] = ky[i] * kx[j] with kx = ky = [-1,-2,0,2,1]
// (verified via cv2.getDerivKernels(1, 0, 5)), BORDER_REFLECT_101 (cv2
// default), exact integer convolution saturate_cast<uchar> (negatives clip
// to 0, values > 255 clip to 255).
std::vector<uint8_t> sobel_mixed_derivative(const std::vector<uint8_t>& src, int width, int height) {
    const size_t np = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint8_t> dst(np, 0);

    const auto at = [width](int x, int y) {
        return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
    };

    static constexpr int kK[5] = {-1, -2, 0, 2, 1};  // kx = ky, offsets -2..+2

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int32_t acc = 0;
            for (int dy = -2; dy <= 2; ++dy) {
                const int sy = reflect_101(y + dy, height);
                const int32_t wy = kK[dy + 2];
                for (int dx = -2; dx <= 2; ++dx) {
                    const int sx = reflect_101(x + dx, width);
                    acc += static_cast<int32_t>(src[at(sx, sy)]) * wy * kK[dx + 2];
                }
            }
            dst[at(x, y)] = static_cast<uint8_t>(std::clamp(acc, 0, 255));
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
    if (overlap_count == 0) {
        // SUPer render2.py:1297-1299: no alpha intersection → (score=1.0, cross=1.0)
        cross_percentage = 1.0;
        return 1.0;
    }

    // Apply Gaussian blur to the mask and binarize, as SUPer does.
    mask = gaussian_blur_5x5(mask, width, height);
    for (auto& v : mask) {
        if (v > 0) {
            v = 255;
        }
    }

    // SUPer render2.py:1284: cross_percentage is the dilated (blurred +
    // binarized) mask area over the total, computed AFTER the blur.
    size_t mask_count = 0;
    for (size_t i = 0; i < np; ++i) {
        if (mask[i] > 0) {
            ++mask_count;
        }
    }
    cross_percentage = static_cast<double>(mask_count) / static_cast<double>(np);

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
        e1 = sobel_mixed_derivative(e1, width, height);
        e2 = sobel_mixed_derivative(e2, width, height);
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
