// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Transparency contract in solve_group: semi-alpha entries are real
// palette rows, alpha==0 maps to 0xFF with no palette row, opaque-only
// keeps index 0 free, output is deterministic.

#include <gtest/gtest.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "opensup/media/optimizer.h"

namespace opensup {
namespace media {
namespace {

group_frame_t make_frame(int w, int h)
{
    group_frame_t f;
    f.width = w;
    f.height = h;
    f.rgba.assign(static_cast<size_t>(w) * h * 4, 0);
    return f;
}

void set_px(group_frame_t& f, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    size_t p = (static_cast<size_t>(y) * f.width + x) * 4;
    f.rgba[p + 0] = r;
    f.rgba[p + 1] = g;
    f.rgba[p + 2] = b;
    f.rgba[p + 3] = a;
}

bool has_alpha(const palette_t& pal, uint8_t alpha)
{
    for (const auto& [idx, e] : pal.entries()) {
        (void)idx;
        if (e.alpha == alpha) return true;
    }
    return false;
}

}  // namespace

TEST(Transparency, SemiAlpha64IsRealPaletteEntry) {
    group_frame_t f = make_frame(4, 4);
    for (int y = 0; y < 4; y++) {
        set_px(f, 0, y, 40, 120, 240, 64);
        set_px(f, 1, y, 40, 120, 240, 64);
        set_px(f, 2, y, 200, 60, 60, 255);
        set_px(f, 3, y, 200, 60, 60, 255);
    }
    group_solution_t sol;
    ASSERT_TRUE(solve_group({f}, 255, sol));
    ASSERT_EQ(sol.palettes.size(), 1u);
    EXPECT_TRUE(has_alpha(sol.palettes[0], 64));
    EXPECT_TRUE(has_alpha(sol.palettes[0], 255));

    uint8_t semi = sol.bitmap[0];  // (0,0) is alpha 64
    EXPECT_NE(semi, 0xFF);
    auto e = sol.palettes[0].get(semi);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->alpha, 64);  // kept as-is, not collapsed to opaque/transparent

    auto eo = sol.palettes[0].get(sol.bitmap[3]);  // (3,0) is opaque
    ASSERT_TRUE(eo.has_value());
    EXPECT_EQ(eo->alpha, 255);
}

TEST(Transparency, AlphaZeroUses0xFFAndOmitsPaletteRow) {
    group_frame_t f = make_frame(4, 4);  // zero-initialised: border is transparent
    for (int y = 1; y <= 2; y++) {
        for (int x = 1; x <= 2; x++) set_px(f, x, y, 200, 60, 60, 255);
    }
    group_solution_t sol;
    ASSERT_TRUE(solve_group({f}, 255, sol));
    ASSERT_EQ(sol.palettes.size(), 1u);
    EXPECT_EQ(sol.bitmap[0], 0xFF);               // transparent -> reserved index
    EXPECT_FALSE(sol.palettes[0].has(0));         // index 0 unused
    EXPECT_FALSE(has_alpha(sol.palettes[0], 0));  // transparent row omitted
    EXPECT_TRUE(has_alpha(sol.palettes[0], 255));

    uint8_t c = sol.bitmap[5];  // (1,1) is opaque
    EXPECT_NE(c, 0xFF);
    auto e = sol.palettes[0].get(c);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->alpha, 255);
}

TEST(Transparency, OpaqueOnlyKeepsIndexZeroFree) {
    group_frame_t f = make_frame(4, 4);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (x < 2) {
                set_px(f, x, y, 40, 120, 240, 255);
            } else {
                set_px(f, x, y, 200, 60, 60, 255);
            }
        }
    }
    group_solution_t sol;
    ASSERT_TRUE(solve_group({f}, 255, sol));
    ASSERT_EQ(sol.palettes.size(), 1u);
    EXPECT_FALSE(sol.palettes[0].has(0));
    EXPECT_FALSE(has_alpha(sol.palettes[0], 0));
    for (uint8_t b : sol.bitmap) {
        EXPECT_NE(b, 0xFF);  // no reserved transparent index
        EXPECT_GE(b, 1u);
    }
}

TEST(Transparency, Deterministic) {
    std::vector<group_frame_t> frames;
    group_frame_t f0 = make_frame(8, 4);
    group_frame_t f1 = make_frame(8, 4);
    for (int y = 1; y <= 2; y++) {
        for (int x = 1; x <= 6; x++) {
            set_px(f0, x, y, 40, 120, 240, 64);
            set_px(f1, x, y, 200, 60, 60, 255);
        }
    }
    frames.push_back(std::move(f0));
    frames.push_back(std::move(f1));

    group_solution_t a, b;
    ASSERT_TRUE(solve_group(frames, 255, a));
    ASSERT_TRUE(solve_group(frames, 255, b));
    EXPECT_EQ(a.bitmap, b.bitmap);
    ASSERT_EQ(a.palettes.size(), b.palettes.size());
    for (size_t i = 0; i < a.palettes.size(); i++) {
        EXPECT_EQ(a.palettes[i].to_bytes(), b.palettes[i].to_bytes());
    }
}

}  // namespace media
}  // namespace opensup
