// Copyright (C) 2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include <gtest/gtest.h>
#include <vector>

#include "opensup/core/layout_engine.h"
#include "opensup/common/geometry.h"

using opensup::core::layout_engine_c;
using opensup::common::box_t;

namespace opensup {
namespace core {
namespace {

TEST(LayoutEngine, SingleObjectSingleWindow) {
    layout_engine_c engine(1920, 1080);
    
    // Create a single subtitle at bottom center (like typical BDN)
    std::vector<uint8_t> alpha(600 * 120, 255);  // fully opaque
    engine.add(660, 480, alpha, 600, 120);
    
    auto [container, w0, w1, is_vertical] = engine.get_layout();
    
    EXPECT_EQ(w0.x, w1.x);
    EXPECT_EQ(w0.y, w1.y);
    EXPECT_EQ(w0.dx, w1.dx);
    EXPECT_EQ(w0.dy, w1.dy);
    EXPECT_FALSE(is_vertical);
    
    // Window should contain the subtitle area with margin
    EXPECT_GE(w0.x, 0);
    EXPECT_GE(w0.y, 0);
    EXPECT_LE(w0.x + w0.dx, 1920);
    EXPECT_LE(w0.y + w0.dy, 1080);
}

TEST(LayoutEngine, TwoObjectsVerticalSplit) {
    layout_engine_c engine(1920, 1080);
    
    std::vector<uint8_t> alpha_top(600 * 120, 255);
    engine.add(660, 100, alpha_top, 600, 120);
    
    std::vector<uint8_t> alpha_bot(600 * 120, 255);
    engine.add(660, 860, alpha_bot, 600, 120);
    
    auto [container, w0, w1, is_vertical] = engine.get_layout();
    
    bool windows_differ = (w0.x != w1.x) || (w0.y != w1.y) || (w0.dx != w1.dx) || (w0.dy != w1.dy);
    EXPECT_TRUE(windows_differ) << "Expected split windows: w0=(" << w0.x << "," << w0.y << "," << w0.dx << "," << w0.dy << ") w1=(" << w1.x << "," << w1.y << "," << w1.dx << "," << w1.dy << ")";
    EXPECT_TRUE(is_vertical) << "Expected horizontal split (top/bottom)";
    
    EXPECT_LT(w0.y + w0.dy, w1.y) << "Top window should be above bottom window";
}

TEST(LayoutPad, RegionPadVertical) {
    // Class C (DS 48-55): the lower window of a vertical split has dy < 8 and
    // reaches the raw-region bottom, not just the 8px minimum.
    layout_engine_c engine(1920, 1080);
    std::vector<uint8_t> top(694 * 53, 255);
    engine.add(328, 1002, top, 694, 53);
    std::vector<uint8_t> bottom(12 * 7, 255);
    engine.add(662, 1055, bottom, 12, 7);

    auto [container, w0, w1, is_vertical] = engine.get_layout();

    EXPECT_TRUE(is_vertical) << "Expected horizontal split (top/bottom)";
    EXPECT_EQ(w0.x, 328);
    EXPECT_EQ(w0.y, 1002);
    EXPECT_EQ(w0.dx, 694);
    EXPECT_EQ(w0.dy, 53);
    EXPECT_EQ(w1.x, 662);
    EXPECT_EQ(w1.y, 1055);
    EXPECT_EQ(w1.dx, 12);
    EXPECT_EQ(w1.dy, 14);
}

TEST(LayoutEngine, TwoObjectsHorizontalSplit) {
    layout_engine_c engine(1920, 1080);
    
    std::vector<uint8_t> alpha_left(600 * 120, 255);
    engine.add(100, 480, alpha_left, 600, 120);
    
    std::vector<uint8_t> alpha_right(600 * 120, 255);
    engine.add(1220, 480, alpha_right, 600, 120);
    
    auto [container, w0, w1, is_vertical] = engine.get_layout();
    
    bool windows_differ = (w0.x != w1.x) || (w0.y != w1.y) || (w0.dx != w1.dx) || (w0.dy != w1.dy);
    EXPECT_TRUE(windows_differ) << "Expected split windows: w0=(" << w0.x << "," << w0.y << "," << w0.dx << "," << w0.dy << ") w1=(" << w1.x << "," << w1.y << "," << w1.dx << "," << w1.dy << ")";
    EXPECT_FALSE(is_vertical) << "Expected vertical split (left/right)";
    
    EXPECT_LT(w0.x + w0.dx, w1.x) << "Left window should be left of right window";
}

TEST(LayoutEngine, TwoObjectsOverlappingNoSplit) {
    layout_engine_c engine(1920, 1080);
    
    std::vector<uint8_t> alpha1(600 * 120, 255);
    engine.add(660, 480, alpha1, 600, 120);
    
    std::vector<uint8_t> alpha2(600 * 120, 255);
    engine.add(660, 480, alpha2, 600, 120);
    
    auto [container, w0, w1, is_vertical] = engine.get_layout();
    
    EXPECT_EQ(w0.x, w1.x);
    EXPECT_EQ(w0.y, w1.y);
    EXPECT_EQ(w0.dx, w1.dx);
    EXPECT_EQ(w0.dy, w1.dy);
}

TEST(LayoutEngine, ResetClearsState) {
    layout_engine_c engine(1920, 1080);
    
    std::vector<uint8_t> alpha(600 * 120, 255);
    engine.add(660, 480, alpha, 600, 120);
    
    engine.reset();
    
    auto [container, w0, w1, is_vertical] = engine.get_layout();
    EXPECT_EQ(w0.x, 0);
    EXPECT_EQ(w0.y, 0);
    EXPECT_EQ(w0.dx, 1920);
    EXPECT_EQ(w0.dy, 1080);
    EXPECT_EQ(w1.x, 0);
    EXPECT_EQ(w1.y, 0);
    EXPECT_EQ(w1.dx, 1920);
    EXPECT_EQ(w1.dy, 1080);
}

TEST(LayoutEngine, EmptyLayout) {
    layout_engine_c engine(1920, 1080);
    
    auto [container, w0, w1, is_vertical] = engine.get_layout();
    
    EXPECT_EQ(w0.x, 0);
    EXPECT_EQ(w0.y, 0);
    EXPECT_EQ(w0.dx, 1920);
    EXPECT_EQ(w0.dy, 1080);
    EXPECT_EQ(w1.x, 0);
    EXPECT_EQ(w1.y, 0);
    EXPECT_EQ(w1.dx, 1920);
    EXPECT_EQ(w1.dy, 1080);
}

TEST(LayoutEngine, ObjectNearEdgeClamped) {
    layout_engine_c engine(1920, 1080);
    
    std::vector<uint8_t> alpha(600 * 120, 255);
    engine.add(0, 0, alpha, 600, 120);
    
    auto [container, w0, w1, is_vertical] = engine.get_layout();
    
    // Window should be clamped to valid screen bounds
    EXPECT_GE(w0.x, 0);
    EXPECT_GE(w0.y, 0);
    EXPECT_LE(w0.x + w0.dx, 1920);
    EXPECT_LE(w0.y + w0.dy, 1080);
}

}  // namespace
}  // namespace core
}  // namespace opensup