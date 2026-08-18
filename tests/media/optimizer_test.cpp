// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// P4b: group co-quantization (union bitmap + per-frame palette diffs).

#include <gtest/gtest.h>
#include <vector>

#include "opensup/media/optimizer.h"

namespace opensup {
namespace media {
namespace {

group_frame_t make_frame(int w, int h, uint8_t gray, bool opaque_bg)
{
    group_frame_t f;
    f.width = w;
    f.height = h;
    f.rgba.assign(static_cast<size_t>(w) * h * 4, 0);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t p = static_cast<size_t>(y) * w + x;
            if (opaque_bg || (x >= 1 && x < w - 1 && y >= 1 && y < h - 1)) {
                // Opaque square covering everything but the border.
                f.rgba[p * 4 + 0] = gray;
                f.rgba[p * 4 + 1] = gray;
                f.rgba[p * 4 + 2] = gray;
                f.rgba[p * 4 + 3] = 255;
            } else {
                f.rgba[p * 4 + 3] = 0;  // transparent border
            }
        }
    }
    return f;
}

size_t frame_hash(const group_solution_t& s)
{
    size_t h = 14695981039346656037ull;
    for (uint8_t b : s.bitmap) {
        h ^= b;
        h *= 1099511628211ull;
    }
    for (const auto& pal : s.palettes) {
        for (uint8_t b : pal.to_bytes()) {
            h ^= b;
            h *= 1099511628211ull;
        }
    }
    return h;
}

TEST(SolveGroup, FadeChainPressureUnionBitmap) {
    // Three frames of a 2x2 fading square on a transparent border.
    std::vector<group_frame_t> frames{
        make_frame(4, 4, 20, false),
        make_frame(4, 4, 32, false),
        make_frame(4, 4, 44, false),
    };
    group_solution_t sol;
    ASSERT_TRUE(solve_group(frames, 255, sol));
    ASSERT_EQ(sol.bitmap.size(), 16u);
    ASSERT_EQ(sol.palettes.size(), 3u);

    // Transparent border maps to 0xFF.
    for (size_t p = 0; p < 16; p++) {
        bool border = (p < 4 || p >= 12 || p % 4 == 0 || p % 4 == 3);
        if (border) {
            EXPECT_EQ(sol.bitmap[p], 0xFF) << "border pixel " << p;
        } else {
            EXPECT_NE(sol.bitmap[p], 0xFF) << "square pixel " << p;
            EXPECT_GE(sol.bitmap[p], 1u);
        }
    }

    // The four square pixels share one sequence -> one bitmap index.
    EXPECT_EQ(sol.bitmap[5], sol.bitmap[6]);
    EXPECT_EQ(sol.bitmap[5], sol.bitmap[9]);
    EXPECT_EQ(sol.bitmap[5], sol.bitmap[10]);

    // Frame 0 carries the full palette; later frames emit the color diffs.
    EXPECT_EQ(sol.palettes[0].size(), 1u);
    EXPECT_NE(sol.palettes[0].get(sol.bitmap[5])->alpha, 0);
    EXPECT_FALSE(sol.palettes[1].empty());
    EXPECT_FALSE(sol.palettes[2].empty());
    EXPECT_NE(*sol.palettes[1].get(sol.bitmap[5]),
              *sol.palettes[0].get(sol.bitmap[5]));
}

TEST(SolveGroup, IdenticalFramesYieldNoDiffs) {
    std::vector<group_frame_t> frames{
        make_frame(4, 4, 40, false),
        make_frame(4, 4, 40, false),  // byte-identical
    };
    group_solution_t sol;
    ASSERT_TRUE(solve_group(frames, 255, sol));
    ASSERT_EQ(sol.palettes.size(), 2u);
    EXPECT_FALSE(sol.palettes[0].empty());
    EXPECT_TRUE(sol.palettes[1].empty()) << "no color change -> no palette diff";
}

TEST(SolveGroup, NoTransparencyKeepsAllIndices) {
    std::vector<group_frame_t> frames{
        make_frame(4, 4, 20, true),  // fully opaque
        make_frame(4, 4, 44, true),
    };
    group_solution_t sol;
    ASSERT_TRUE(solve_group(frames, 255, sol));
    ASSERT_FALSE(sol.bitmap.empty());
    bool saw_ff = false;
    for (uint8_t b : sol.bitmap) {
        EXPECT_GE(b, 1u);  // index 0 never used
        if (b == 0xFF) saw_ff = true;
    }
    EXPECT_FALSE(saw_ff) << "no transparent pixels -> 0xFF must not appear";
}

TEST(SolveGroup, Deterministic) {
    std::vector<group_frame_t> frames{
        make_frame(8, 4, 20, false),
        make_frame(8, 4, 30, false),
        make_frame(8, 4, 44, false),
    };
    group_solution_t a, b;
    ASSERT_TRUE(solve_group(frames, 255, a));
    ASSERT_TRUE(solve_group(frames, 255, b));
    EXPECT_EQ(frame_hash(a), frame_hash(b));
}

TEST(SolveGroup, SingleFrameWorks) {
    group_solution_t sol;
    ASSERT_TRUE(solve_group({make_frame(4, 4, 60, false)}, 255, sol));
    ASSERT_EQ(sol.palettes.size(), 1u);
    EXPECT_FALSE(sol.palettes[0].empty());
}

TEST(SolveGroup, RejectsMismatchedFrames) {
    group_solution_t sol;
    std::vector<group_frame_t> frames{
        make_frame(4, 4, 20, false),
        make_frame(8, 4, 30, false),
    };
    EXPECT_FALSE(solve_group(frames, 255, sol));
    EXPECT_FALSE(solve_group({}, 255, sol));
}

}  // namespace
}  // namespace media
}  // namespace opensup