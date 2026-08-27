// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// P4b: SSIM-fused event runs are co-quantized into one union ODS plus a chain
// of palette updates (the fade becomes visible between palette swaps).

#include <gtest/gtest.h>
#include <set>
#include <string>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/core/segments.h"

namespace opensup {
namespace core {
namespace {

TEST(GroupChain, FusedEventsGetPaletteChain) {
    // synth_similar.xml: 3 consecutive same-size events with a subtle text
    // fade. Before P4b, the fused events emitted PCS+WDS+ENDS only, so the
    // fade never reached the decoder. P4b replaces them with real palette
    // updates on a shared union bitmap.
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/synth_similar.xml";
    cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/group_chain.sup";
    cfg.overwrite = true;
    cfg.compression = 80;
    cfg.extra_acq = 0;  // isolate the P4b path from extra_acq inserts

    bdn_render_c r(cfg);
    const auto res = r.execute();
    ASSERT_TRUE(res.success) << res.error;

    int pds = 0, ods = 0;
    std::vector<size_t> pal_entry_counts;
    media::palette_t acc;  // accumulated palette across diffs
    for (const auto& s : r.segments()) {
        if (auto pd = dynamic_cast<pds_c*>(s.get())) {
            pds++;
            const auto pal = pd->to_palette();
            if (pal.empty()) continue;
            pal_entry_counts.push_back(pal.size());
            for (const auto& [idx, e] : pal.entries()) acc.set(idx, e);
        } else if (dynamic_cast<ods_c*>(s.get())) {
            ods++;
        }
    }

    // Every group event carries a palette update (1 full + (N-1) diffs).
    EXPECT_EQ(pds, res.events);
    // The diffs must be leaner than the full first palette.
    ASSERT_GE(pal_entry_counts.size(), 2u);
    EXPECT_GT(pal_entry_counts[0], pal_entry_counts[1]);
    // The fade is visible: the accumulated palette holds at least two
    // distinct colors (background + text fading levels).
    std::set<uint8_t> seen_y;
    for (const auto& [idx, e] : acc.entries()) {
        if (idx != 0 && e.alpha != 0) seen_y.insert(e.y);
    }
    EXPECT_GE(seen_y.size(), 2u);
    // Fewer (or equal) objects than events: the run shares a union bitmap.
    EXPECT_LE(ods, res.events);
    EXPECT_GE(ods, 1);
}

TEST(GroupChain, Deterministic) {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/synth_similar.xml";
    cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/group_chain_det1.sup";
    cfg.overwrite = true;
    cfg.extra_acq = 0;

    encode_config_t cfg2 = cfg;
    cfg2.output_path = OPENDSUP_TEST_OUTPUT_DIR "/group_chain_det2.sup";

    bdn_render_c r1(cfg);
    bdn_render_c r2(cfg2);
    ASSERT_TRUE(r1.execute().success);
    ASSERT_TRUE(r2.execute().success);
    const auto a = r1.segments();
    const auto b = r2.segments();
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); i++)
        EXPECT_EQ(a[i]->to_bytes(), b[i]->to_bytes());
}

}  // namespace
}  // namespace core
}  // namespace opensup