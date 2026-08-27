// Copyright (C) 2024-2026 Lanzoone30
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
    // Create a layout engine for 1920x1080
    layout_engine_c engine(1920, 1080);
    
    // Create a single subtitle at bottom center (like typical BDN)
    // 600x120 at X=660, Y=480 (1080p, bottom area)
    std::vector<uint8_t> alpha(600 * 120, 255);  // fully opaque
    engine.add(660, 480, alpha, 600, 120);
    
    auto [container, w0, w1, is_vertical] = engine.get_layout();
    
    // Should be single window (w0 == w1)
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
    // Two objects far apart vertically should trigger horizontal split (top/bottom)
    layout_engine_c engine(1920, 1080);
    
    // Top object: 600x120 at Y=100 (top of screen)
    std::vector<uint8_t> alpha_top(600 * 120, 255);
    engine.add(660, 100, alpha_top, 600, 120);
    
    // Bottom object: 600x120 at Y=860 (bottom of screen)
    std::vector<uint8_t> alpha_bot(600 * 120, 255);
    engine.add(660, 860, alpha_bot, 600, 120);
    
    auto [container, w0, w1, is_vertical] = engine.get_layout();
    
    // Should split into 2 windows (horizontal split = is_vertical = true)
    // Windows may share x/dx but have different y/dy for vertical split
    bool windows_differ = (w0.x != w1.x) || (w0.y != w1.y) || (w0.dx != w1.dx) || (w0.dy != w1.dy);
    EXPECT_TRUE(windows_differ) << "Expected split windows: w0=(" << w0.x << "," << w0.y << "," << w0.dx << "," << w0.dy << ") w1=(" << w1.x << "," << w1.y << "," << w1.dx << "," << w1.dy << ")";
    EXPECT_TRUE(is_vertical) << "Expected horizontal split (top/bottom)";
    
    // One window should be at top, one at bottom
    // w0 is top window, w1 is bottom window
    EXPECT_LT(w0.y + w0.dy, w1.y) << "Top window should be above bottom window";
}

TEST(LayoutEngine, TwoObjectsHorizontalSplit) {
    // Two objects far apart horizontally should trigger vertical split (left/right)
    layout_engine_c engine(1920, 1080);
    
    // Left object: 600x120 at X=100
    std::vector<uint8_t> alpha_left(600 * 120, 255);
    engine.add(100, 480, alpha_left, 600, 120);
    
    // Right object: 600x120 at X=1220
    std::vector<uint8_t> alpha_right(600 * 120, 255);
    engine.add(1220, 480, alpha_right, 600, 120);
    
    auto [container, w0, w1, is_vertical] = engine.get_layout();
    
    // Should split into 2 windows (vertical split = is_vertical = false)
    // Windows may share y/dy but have different x/dx for horizontal split
    bool windows_differ = (w0.x != w1.x) || (w0.y != w1.y) || (w0.dx != w1.dx) || (w0.dy != w1.dy);
    EXPECT_TRUE(windows_differ) << "Expected split windows: w0=(" << w0.x << "," << w0.y << "," << w0.dx << "," << w0.dy << ") w1=(" << w1.x << "," << w1.y << "," << w1.dx << "," << w1.dy << ")";
    EXPECT_FALSE(is_vertical) << "Expected vertical split (left/right)";
    
    // One window should be at left, one at right
    EXPECT_LT(w0.x + w0.dx, w1.x) << "Left window should be left of right window";
}

TEST(LayoutEngine, TwoObjectsOverlappingNoSplit) {
    // Two overlapping objects should NOT trigger split
    layout_engine_c engine(1920, 1080);
    
    // First object at center
    std::vector<uint8_t> alpha1(600 * 120, 255);
    engine.add(660, 480, alpha1, 600, 120);
    
    // Second object overlapping the first
    std::vector<uint8_t> alpha2(600 * 120, 255);
    engine.add(660, 480, alpha2, 600, 120);
    
    auto [container, w0, w1, is_vertical] = engine.get_layout();
    
    // Should remain single window
    EXPECT_EQ(w0.x, w1.x);
    EXPECT_EQ(w0.y, w1.y);
    EXPECT_EQ(w0.dx, w1.dx);
    EXPECT_EQ(w0.dy, w1.dy);
}

TEST(LayoutEngine, ResetClearsState) {
    layout_engine_c engine(1920, 1080);
    
    // Add object
    std::vector<uint8_t> alpha(600 * 120, 255);
    engine.add(660, 480, alpha, 600, 120);
    
    // Reset
    engine.reset();
    
    // Should be back to full container
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
    // No objects added - should return full container as single window
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
    // Object at edge should be clamped with margin
    layout_engine_c engine(1920, 1080);
    
    // Object at top-left corner (X=0, Y=0)
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