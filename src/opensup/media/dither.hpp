// SPDX-License-Identifier: GPL-3.0-or-later
//
// Atkinson error diffusion shared by the HexTree and Wu quantizers.
// Extracted from the HexTree port (MIT, (c) 2024 cubicibo).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace opensup {
namespace media {
namespace dither {

/// Error rows kept alive: Atkinson spreads over the current and next row.
constexpr int kErrorRows = 3;
/// Residual error below this energy skips diffusion (exact lookup instead).
constexpr float kErrThreshSkip = 2.0f;
/// Per-component clamp of the accumulated error before it is applied.
constexpr float kMaxErrorComponent = 16.0f;
/// Atkinson distributes one eighth of the residual per neighbour.
constexpr float kDiffusionDen = 8.0f;

inline long clipRound(float value)
{
    const long r = std::lrint(value);
    return r > 255 ? 255 : (r < 0 ? 0 : r);
}

/// Alpha is a vehicle: fully transparent pixels carry no RGB.
inline void unpackRgba(const uint8_t *value, float rgba[4])
{
    rgba[3] = static_cast<float>(value[3]);
    if (value[3] > 0) {
        rgba[2] = static_cast<float>(value[2]);
        rgba[1] = static_cast<float>(value[1]);
        rgba[0] = static_cast<float>(value[0]);
    } else {
        rgba[2] = rgba[1] = rgba[0] = 0.0f;
    }
}

/// Round and clamp to bytes; the integer copy feeds palette lookups.
inline void clipPack(const float rgba[4], int32_t irgba[4], uint8_t crgba[4])
{
    irgba[3] = crgba[3] = static_cast<uint8_t>(clipRound(rgba[3]));
    if (irgba[3] > 0) {
        irgba[2] = crgba[2] = static_cast<uint8_t>(clipRound(rgba[2]));
        irgba[1] = crgba[1] = static_cast<uint8_t>(clipRound(rgba[1]));
        irgba[0] = crgba[0] = static_cast<uint8_t>(clipRound(rgba[0]));
    } else {
        irgba[2] = crgba[2] = 0;
        irgba[1] = crgba[1] = 0;
        irgba[0] = crgba[0] = 0;
    }
}

inline void addAt(float *dst, const float err[4])
{
    for (int k = 0; k < 4; ++k) dst[k] += err[k];
}

/// Per-call diffusion state: serpentine cursor plus a ring of error rows.
struct atkinson_t {
    uint32_t width;
    uint32_t row_len;
    std::vector<float> errors;
    uint32_t line_id = 0;
    uint32_t x = 0;
    int direction = -1;

    explicit atkinson_t(uint32_t w)
        : width(w), row_len(w << 2),
          // Slack keeps the width<3 edge arithmetic inside the buffer.
          errors(static_cast<size_t>(row_len) * kErrorRows + 16, 0.0f) {}

    void begin_row()
    {
        direction = -direction;
        x = direction > 0 ? 0 : width - 1;
    }

    void end_row() { ++line_id; }

    void advance() { x = static_cast<uint32_t>(static_cast<int>(x) + direction); }

    float *current()
    {
        return &errors[static_cast<size_t>(line_id % kErrorRows) * row_len + (x << 2)];
    }

    /// Clamp the accumulated error and add it to the source pixel. Returns
    /// the energy of the (clamped) error.
    float load(const uint8_t *pixel, float org[4])
    {
        float *e = current();
        float sErr = 0.0f;
        for (int k = 0; k < 4; ++k) {
            e[k] = e[k] < 0 ? -std::min(kMaxErrorComponent, -e[k])
                            : std::min(kMaxErrorComponent, e[k]);
            sErr += e[k] * e[k];
        }
        unpackRgba(pixel, org);
        return sErr;
    }

    void add_error(float org[4])
    {
        const float *e = current();
        for (int k = 0; k < 4; ++k) org[k] += e[k];
    }

    void clear() { std::memset(current(), 0, 4 * sizeof(float)); }

    /// Distribute one eighth of (org - palette) over the Atkinson neighbours.
    void diffuse(const float org[4], const int32_t pal[4], size_t y, size_t len)
    {
        float err[4];
        for (int k = 0; k < 4; ++k)
            err[k] = (org[k] - static_cast<float>(pal[k])) / kDiffusionDen;

        float *e = current();
        if ((direction > 0 && x < width - 2u) || (direction < 0 && x > 1u)) {
            addAt(e + 4 * direction, err);
            addAt(e + 8 * direction, err);
        }
        if (y < len - width && x < width - 1u && x > 0u) {
            float *next =
                &errors[static_cast<size_t>((line_id + 1) % kErrorRows) * row_len + (x << 2)];
            addAt(next - 4, err);
            addAt(next, err);
            addAt(next + 4, err);
        }
    }
};

} // namespace dither
} // namespace media
} // namespace opensup
