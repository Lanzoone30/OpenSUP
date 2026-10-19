// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// FR-006 palette-wipe parity. When the cleared window's redraw cannot fit the
// frame budget the clear display set becomes a palette wipe (PCS with
// palette_update_flag + an all-transparent PDS + END) instead of a WDS wipe.
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include "opensup/core/interface.h"
#include "opensup/core/segments.h"
namespace opensup {
namespace core {
namespace {

struct wipe_counts_t {
    int wds = 0;                  // clears emitted as PCS + WDS + END
    int palette = 0;              // clears emitted as PCS(pal_flag) + PDS + END
    bool palette_no_cobjs = false;  // a palette wipe that dropped the composition
};

/// Classifies each clear display set: a WDS wipe (empty PCS followed by WDS) or
/// a palette wipe (PCS with palette_update_flag followed by PDS).
wipe_counts_t classify_clears(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    wipe_counts_t c;
    for (size_t i = 0; i + 1 < segs.size(); ++i) {
        auto pcs = std::dynamic_pointer_cast<pcs_c>(segs[i]);
        if (!pcs) continue;
        auto next = segs[i + 1];
        if (pcs->pal_flag() && std::dynamic_pointer_cast<pds_c>(next)) {
            c.palette++;
            if (pcs->cobjects.empty()) c.palette_no_cobjs = true;
        } else if (!pcs->pal_flag() && pcs->cobjects.empty() &&
                   std::dynamic_pointer_cast<wds_c>(next)) {
            c.wds++;
        }
    }
    return c;
}

bdn_render_c encode_fixture(const std::string& fixture, const std::string& out) {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/" + fixture;
    cfg.output_path = out;
    cfg.overwrite = true;
    cfg.extra_acq = 0;
    cfg.quantizer_id = 2;  // hextree: fast, keeps the test off the slow default

    bdn_render_c render(cfg);
    const auto res = render.execute();
    EXPECT_TRUE(res.success) << res.error;
    return render;
}

}  // namespace

TEST(PaletteWipe, LargeWindowClearsUsePaletteUpdate) {
    // 740x740 @60fps window: above (1/fps - 3/FREQ)*RC ~ 531K px. A 2-frame gap
    // before the last (1-frame) event forces a mid-stream clear; neither it nor
    // the end-of-epoch clear fits a WDS redraw, so both swap to palette wipes.
    auto render = encode_fixture("large_wipe.xml",
                                 OPENDSUP_TEST_OUTPUT_DIR "/large_wipe.sup");
    auto c = classify_clears(render.segments());
    EXPECT_GE(c.palette, 2) << "large-window clears must be palette wipes";
    EXPECT_FALSE(c.palette_no_cobjs)
        << "palette wipe PCS must keep the composition objects";
    EXPECT_GE(c.wds, 1)
        << "the end-of-epoch palette wipe must be followed by a WDS clear";
}

TEST(PaletteWipe, SmallWindowClearsKeepWds) {
    // 600x120 windows fit the frame budget: clears stay WDS wipes and never
    // switch to a palette wipe.
    auto render = encode_fixture("synth_similar.xml",
                                 OPENDSUP_TEST_OUTPUT_DIR "/synth_similar_wipe.sup");
    auto c = classify_clears(render.segments());
    EXPECT_GE(c.wds, 1) << "small-window clears keep the WDS wipe";
    EXPECT_EQ(c.palette, 0) << "small-window clears must not use a palette wipe";
}

}  // namespace core
}  // namespace opensup
