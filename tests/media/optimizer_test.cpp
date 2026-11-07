// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// P4b: group co-quantization (union bitmap + per-frame palette diffs).

#include <gtest/gtest.h>
#include <thread>
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

// Opaque gradient with a transparent border: enough distinct colors for the
// backends to disagree at the test's palette cap.
group_frame_t make_gradient_frame(int w, int h, int seed)
{
    group_frame_t f;
    f.width = w;
    f.height = h;
    f.rgba.assign(static_cast<size_t>(w) * h * 4, 0);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (x == 0 || y == 0 || x == w - 1 || y == h - 1) continue;
            size_t p = static_cast<size_t>(y) * w + x;
            f.rgba[p * 4 + 0] = static_cast<uint8_t>(x * 37 + y * 11 + seed * 13);
            f.rgba[p * 4 + 1] = static_cast<uint8_t>(x * 7 + y * 41 + seed * 23);
            f.rgba[p * 4 + 2] = static_cast<uint8_t>(x * 19 + y * 29 + seed * 31);
            f.rgba[p * 4 + 3] = 255;
        }
    }
    return f;
}

std::vector<uint8_t> make_pattern(int w, int h)
{
    std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t p = static_cast<size_t>(y) * w + x;
            rgba[p * 4 + 0] = static_cast<uint8_t>((x * 17 + y * 5) & 0xFF);
            rgba[p * 4 + 1] = static_cast<uint8_t>((x * 3 + y * 29) & 0xFF);
            rgba[p * 4 + 2] = static_cast<uint8_t>((x * 11 + y * 13) & 0xFF);
            rgba[p * 4 + 3] = 255;
        }
    }
    return rgba;
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
    ASSERT_TRUE(solve_group(frames, 255, sol, "bt709", {}, -1));
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
    ASSERT_TRUE(solve_group(frames, 255, sol, "bt709", {}, -1));
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
    ASSERT_TRUE(solve_group(frames, 255, sol, "bt709", {}, -1));
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
    ASSERT_TRUE(solve_group(frames, 255, a, "bt709", {}, -1));
    ASSERT_TRUE(solve_group(frames, 255, b, "bt709", {}, -1));
    EXPECT_EQ(frame_hash(a), frame_hash(b));
}

TEST(SolveGroup, HonorsSelectedQuantizer) {
    std::vector<group_frame_t> frames{
        make_gradient_frame(16, 16, 0),
        make_gradient_frame(16, 16, 37),
        make_gradient_frame(16, 16, 74),
    };
    const int liq_idx = optimiser_c::index_for(2);
    ASSERT_GE(liq_idx, 0);
    group_solution_t liq, liq2;
    ASSERT_TRUE(solve_group(frames, 64, liq, "bt709", {}, liq_idx));
    ASSERT_TRUE(solve_group(frames, 64, liq2, "bt709", {}, liq_idx));
    EXPECT_EQ(frame_hash(liq), frame_hash(liq2));

    for (int id : {0, 1, 3}) {
        SCOPED_TRACE("quantizer id " + std::to_string(id));
        const int idx = optimiser_c::index_for(id);
        ASSERT_GE(idx, 0);
        group_solution_t a, b;
        ASSERT_TRUE(solve_group(frames, 64, a, "bt709", {}, idx));
        ASSERT_TRUE(solve_group(frames, 64, b, "bt709", {}, idx));
        EXPECT_EQ(frame_hash(a), frame_hash(b));
        EXPECT_NE(frame_hash(a), frame_hash(liq));
    }
}

TEST(SolveGroup, SingleFrameWorks) {
    group_solution_t sol;
    ASSERT_TRUE(solve_group({make_frame(4, 4, 60, false)}, 255, sol, "bt709", {}, -1));
    ASSERT_EQ(sol.palettes.size(), 1u);
    EXPECT_FALSE(sol.palettes[0].empty());
}

TEST(SolveGroup, RejectsMismatchedFrames) {
    group_solution_t sol;
    std::vector<group_frame_t> frames{
        make_frame(4, 4, 20, false),
        make_frame(8, 4, 30, false),
    };
    EXPECT_FALSE(solve_group(frames, 255, sol, "bt709", {}, -1));
    EXPECT_FALSE(solve_group({}, 255, sol, "bt709", {}, -1));
}

TEST(LiqOptions, SequenceScalingParity) {
    EXPECT_EQ(liq_seq_quality(95), 93);
    EXPECT_NEAR(liq_seq_dither(95), 0.855f, 1e-6f);
    EXPECT_EQ(liq_seq_quality(100), 98);
    EXPECT_NEAR(liq_seq_dither(100), 0.9f, 1e-6f);
}

TEST(LiqOptions, QuantizeWithNonDefaults) {
    auto opt = optimiser_c::get_available();
    ASSERT_FALSE(opt.empty());
    liq_options_t liq;
    liq.quality = 70;
    liq.speed = 10;
    liq.dither_pct = 30;
    auto frame = make_frame(8, 8, 128, false);
    auto result = opt[0]->quantize(frame.rgba, frame.width, frame.height, 254,
                                   true, "bt709", liq);
    EXPECT_FALSE(result.palette.empty());
    EXPECT_EQ(result.indexed.size(), frame.rgba.size() / 4);
}

TEST(LiqOptions, SolveGroupForwardsOptions) {
    std::vector<group_frame_t> frames{
        make_frame(8, 4, 20, false),
        make_frame(8, 4, 30, false),
    };
    liq_options_t liq;
    liq.quality = 70;
    liq.speed = 10;
    liq.dither_pct = 30;
    group_solution_t sol;
    ASSERT_TRUE(solve_group(frames, 255, sol, "bt709", liq, -1));
    EXPECT_EQ(sol.palettes.size(), 2u);
}

namespace {

int qtzr_index()
{
    const int idx = optimiser_c::index_for(0);
    EXPECT_GE(idx, 0);
    return idx;
}

quantize_result_t quantize_qtzr(const std::vector<uint8_t>& rgba, int w, int h,
                                int max_colors, int dither_pct)
{
    auto opt = optimiser_c::get_available();
    const int idx = qtzr_index();
    if (idx < 0 || static_cast<size_t>(idx) >= opt.size()) return {};
    liq_options_t opts;
    opts.dither_pct = dither_pct;
    return opt[static_cast<size_t>(idx)]->quantize(rgba, w, h, max_colors, true,
                                                   "bt709", opts);
}

quantize_result_t quantize_wu(const std::vector<uint8_t>& rgba, int w, int h,
                              int max_colors, int dither_pct)
{
    auto opt = optimiser_c::get_available();
    const int idx = optimiser_c::index_for(3);
    if (idx < 0 || static_cast<size_t>(idx) >= opt.size()) return {};
    liq_options_t opts;
    opts.dither_pct = dither_pct;
    return opt[static_cast<size_t>(idx)]->quantize(rgba, w, h, max_colors, true,
                                                   "bt709", opts);
}

}  // namespace

TEST(QtzrUtc, PaletteAndIndexedShape) {
    const auto rgba = make_pattern(32, 16);
    for (int dither : {0, 95}) {
        const auto qr = quantize_qtzr(rgba, 32, 16, 254, dither);
        SCOPED_TRACE(dither);
        ASSERT_FALSE(qr.palette.empty());
        ASSERT_EQ(qr.indexed.size(), static_cast<size_t>(32 * 16));
        EXPECT_LE(qr.palette.size(), 254u);
        EXPECT_EQ(qr.palette.size(), 254u);  // pattern has more colors than the cap
        // 0xFF is the caller's reserved transparent slot.
        for (uint8_t idx : qr.indexed) EXPECT_NE(idx, 0xFF);
    }
}

TEST(QtzrUtc, Deterministic) {
    const auto rgba = make_pattern(24, 12);
    const auto a = quantize_qtzr(rgba, 24, 12, 64, 95);
    const auto b = quantize_qtzr(rgba, 24, 12, 64, 95);
    ASSERT_FALSE(a.palette.empty());
    EXPECT_EQ(a.palette, b.palette);
    EXPECT_EQ(a.indexed, b.indexed);
}

TEST(QtzrUtc, ConcurrentCallsMatchSingleThreaded) {
    const auto rgba = make_pattern(24, 12);
    const auto expected = quantize_qtzr(rgba, 24, 12, 64, 95);
    ASSERT_FALSE(expected.palette.empty());

    quantize_result_t r1, r2;
    std::thread t1([&] { r1 = quantize_qtzr(rgba, 24, 12, 64, 95); });
    std::thread t2([&] { r2 = quantize_qtzr(rgba, 24, 12, 64, 95); });
    t1.join();
    t2.join();

    EXPECT_EQ(r1.indexed, expected.indexed);
    EXPECT_EQ(r2.indexed, expected.indexed);
    EXPECT_EQ(r1.palette, expected.palette);
    EXPECT_EQ(r2.palette, expected.palette);
}

TEST(QtzrUtc, GroupFramesKeepCapAndTransparentSlot) {
    std::vector<group_frame_t> frames{
        make_frame(8, 8, 20, false),
        make_frame(8, 8, 60, false),
        make_frame(8, 8, 120, false),
    };
    group_solution_t sol;
    ASSERT_TRUE(solve_group(frames, 4, sol, "bt709", {}, -1));
    ASSERT_EQ(sol.bitmap.size(), 64u);
    EXPECT_EQ(sol.bitmap[0], 0xFF);          // transparent border keeps its slot
    EXPECT_LE(sol.palettes[0].size(), 4u);

    // Requested backend, per-frame: cap and reserved slot hold.
    for (const auto& f : frames) {
        const auto qr = quantize_qtzr(f.rgba, f.width, f.height, 254, 95);
        ASSERT_EQ(qr.indexed.size(), static_cast<size_t>(f.width) * f.height);
        EXPECT_LE(qr.palette.size(), 254u);
        for (uint8_t idx : qr.indexed) {
            EXPECT_NE(idx, 0xFF);
            ASSERT_TRUE(qr.palette.has(idx));
        }
        const auto border = qr.palette.get(qr.indexed[0]);
        ASSERT_TRUE(border.has_value());
        EXPECT_EQ(border->alpha, 0);
    }
}

TEST(QtzrUtc, RegistryAndIdMapping) {
    auto opt = optimiser_c::get_available();
    ASSERT_EQ(opt.size(), 4u);
    EXPECT_EQ(opt[0]->name(), "libimagequant");
    EXPECT_EQ(opt[1]->name(), "HexTree");
    EXPECT_EQ(opt[2]->name(), "QtzrUTC");
    EXPECT_EQ(opt[3]->name(), "Wu");
    // index_for maps engine ids onto that vector; unknown ids return -1.
    EXPECT_EQ(optimiser_c::index_for(2), 0);
    EXPECT_EQ(optimiser_c::index_for(1), 1);
    EXPECT_EQ(optimiser_c::index_for(0), 2);
    EXPECT_EQ(optimiser_c::index_for(3), 3);
    EXPECT_EQ(optimiser_c::index_for(4), -1);
    EXPECT_EQ(optimiser_c::index_for(5), -1);
}

TEST(Wu, PaletteAndIndexedShape) {
    const auto rgba = make_pattern(32, 16);
    for (int dither : {0, 95}) {
        const auto qr = quantize_wu(rgba, 32, 16, 254, dither);
        SCOPED_TRACE(dither);
        ASSERT_FALSE(qr.palette.empty());
        ASSERT_EQ(qr.indexed.size(), static_cast<size_t>(32 * 16));
        EXPECT_LE(qr.palette.size(), 254u);
        EXPECT_EQ(qr.palette.size(), 254u);  // pattern has more colors than the cap
        for (uint8_t idx : qr.indexed) {
            EXPECT_NE(idx, 0xFF);
            ASSERT_TRUE(qr.palette.has(idx));
        }
    }
}

TEST(Wu, ShortPaletteWhenFewerBoxes) {
    const int w = 32, h = 16;
    std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t p = static_cast<size_t>(y) * w + x;
            const uint8_t v = static_cast<uint8_t>((x / 8) * 64);
            rgba[p * 4 + 0] = rgba[p * 4 + 1] = rgba[p * 4 + 2] = v;
            rgba[p * 4 + 3] = 255;
        }
    }
    const auto qr = quantize_wu(rgba, w, h, 64, 0);
    ASSERT_EQ(qr.indexed.size(), static_cast<size_t>(w * h));
    EXPECT_EQ(qr.palette.size(), 4u);  // four gray levels, no invented entries
    for (uint8_t idx : qr.indexed) EXPECT_TRUE(qr.palette.has(idx));
}

TEST(Wu, Deterministic) {
    const auto rgba = make_pattern(24, 12);
    const auto a = quantize_wu(rgba, 24, 12, 64, 95);
    const auto b = quantize_wu(rgba, 24, 12, 64, 95);
    ASSERT_FALSE(a.palette.empty());
    EXPECT_EQ(a.palette, b.palette);
    EXPECT_EQ(a.indexed, b.indexed);
}

TEST(Wu, ConcurrentCallsMatchSingleThreaded) {
    const auto rgba = make_pattern(24, 12);
    const auto expected = quantize_wu(rgba, 24, 12, 64, 95);
    ASSERT_FALSE(expected.palette.empty());

    quantize_result_t r1, r2;
    std::thread t1([&] { r1 = quantize_wu(rgba, 24, 12, 64, 95); });
    std::thread t2([&] { r2 = quantize_wu(rgba, 24, 12, 64, 95); });
    t1.join();
    t2.join();

    EXPECT_EQ(r1.indexed, expected.indexed);
    EXPECT_EQ(r2.indexed, expected.indexed);
    EXPECT_EQ(r1.palette, expected.palette);
    EXPECT_EQ(r2.palette, expected.palette);
}

TEST(Wu, GroupFramesKeepCapAndTransparentSlot) {
    std::vector<group_frame_t> frames{
        make_frame(8, 8, 20, false),
        make_frame(8, 8, 60, false),
        make_frame(8, 8, 120, false),
    };
    group_solution_t sol;
    ASSERT_TRUE(solve_group(frames, 4, sol, "bt709", {}, -1));
    ASSERT_EQ(sol.bitmap.size(), 64u);
    EXPECT_EQ(sol.bitmap[0], 0xFF);          // transparent border keeps its slot
    EXPECT_LE(sol.palettes[0].size(), 4u);

    // Requested backend, per-frame: cap and reserved slot hold.
    for (const auto& f : frames) {
        const auto qr = quantize_wu(f.rgba, f.width, f.height, 254, 95);
        ASSERT_EQ(qr.indexed.size(), static_cast<size_t>(f.width) * f.height);
        EXPECT_LE(qr.palette.size(), 254u);
        for (uint8_t idx : qr.indexed) {
            EXPECT_NE(idx, 0xFF);
            ASSERT_TRUE(qr.palette.has(idx));
        }
        const auto border = qr.palette.get(qr.indexed[0]);
        ASSERT_TRUE(border.has_value());
        EXPECT_EQ(border->alpha, 0);
    }
}

TEST(Wu, FullyTransparentFrame) {
    group_frame_t f;
    f.width = 8;
    f.height = 8;
    f.rgba.assign(8 * 8 * 4, 0);
    // Alpha is the vehicle: RGB under a==0 must be treated as black.
    for (size_t p = 0; p < 64; p++) {
        f.rgba[p * 4 + 0] = 200;
        f.rgba[p * 4 + 1] = 100;
        f.rgba[p * 4 + 2] = 50;
    }
    const auto qr = quantize_wu(f.rgba, f.width, f.height, 254, 95);
    ASSERT_EQ(qr.indexed.size(), 64u);
    ASSERT_EQ(qr.palette.size(), 1u);  // single zero-variance point mass
    for (uint8_t idx : qr.indexed) EXPECT_EQ(idx, 0u);
    const auto only = qr.palette.get(0);
    ASSERT_TRUE(only.has_value());
    EXPECT_EQ(only->alpha, 0);
}

}  // namespace
}  // namespace media
}  // namespace opensup