// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Multi-window emission (Fase 1 / feature 003): with a split layout epoch,
// each object must be emitted against its own layout window (window_id from
// the window containing the object), not the hardcoded window 0.
#include <gtest/gtest.h>
#include <memory>
#include <set>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/core/segments.h"

namespace opensup {
namespace core {
namespace {

TEST(MultiWindow, SplitRegionsUseTheirLayoutWindows) {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/opensup_multiwindow.xml";
    cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/multiwindow.sup";
    cfg.overwrite = true;
    cfg.extra_acq = 0;

    bdn_render_c render(cfg);
    const auto res = render.execute();
    ASSERT_TRUE(res.success) << res.error;

    // Each display set with objects: its WDS (next segment) must define the
    // window the CObject targets, and that window must contain the object.
    std::set<uint8_t> seen_windows;
    const auto& segs = render.segments();
    for (size_t i = 0; i + 1 < segs.size(); i++) {
        auto pcs = std::dynamic_pointer_cast<pcs_c>(segs[i]);
        if (!pcs || pcs->cobjects.empty()) continue;
        auto wds = std::dynamic_pointer_cast<wds_c>(segs[i + 1]);
        ASSERT_TRUE(wds) << "PCS with objects must be followed by its WDS";
        ASSERT_FALSE(wds->windows.empty());
        const auto& wd = wds->windows[0];
        seen_windows.insert(wd.window_id);
        for (const auto& obj : pcs->cobjects) {
            EXPECT_EQ(obj.window_id, wd.window_id);
            EXPECT_GE(obj.h_pos, wd.h_pos);
            EXPECT_LT(obj.h_pos, static_cast<uint16_t>(wd.h_pos + wd.width));
            EXPECT_GE(obj.v_pos, wd.v_pos);
            EXPECT_LT(obj.v_pos, static_cast<uint16_t>(wd.v_pos + wd.height));
        }
    }

    // The epoch split into two regions: both layout windows must be used.
    EXPECT_TRUE(seen_windows.count(1) == 1);
    EXPECT_TRUE(seen_windows.count(0) == 1);
}

}  // namespace
}  // namespace core
}  // namespace opensup