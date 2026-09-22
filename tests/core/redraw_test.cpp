// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// H3 close: --redraw-period used to compute redraw_flags but discard them
// (renderer did `(void)redraw_flags`). Since P3/P4a the flags flow to
// `forced_acq`, forcing an ACQUISITION at each redraw boundary. This test
// locks that behavior: splitting a long event must yield more PCS and more
// ACQUISITION display sets than the non-split baseline.

#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/core/segments.h"

namespace opensup {
namespace core {
namespace {

/// FNV-1a over a segment's serialized bytes.
struct SegmentHasher {
    static size_t hash(const std::shared_ptr<pg_segment_c>& seg) {
        const auto raw = seg->to_bytes();
        size_t h = 14695981039346656037ull;
        for (uint8_t b : raw) {
            h ^= b;
            h *= 1099511628211ull;
        }
        return h;
    }
};

/// Concatenated segment hash (order matters) — byte-exact stream equality.
static size_t stream_hash(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    size_t h = 14695981039346656037ull;
    for (const auto& s : segs) {
        h ^= SegmentHasher::hash(s);
        h *= 1099511628211ull;
    }
    return h;
}

/// Count PCS total and those in ACQUISITION state.
static std::pair<int, int> count_pcs(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    int total = 0, acq = 0;
    for (const auto& s : segs) {
        auto pc = dynamic_cast<pcs_c*>(s.get());
        if (!pc) continue;
        total++;
        if (pc->composition_state() == pcs_c::composition_state_e::acquisition) acq++;
    }
    return {total, acq};
}

TEST(RedrawPeriod, SplitsLongEventsIntoForcedAcquisitions) {
    // synth_bdn.xml: 25 fps, 3 events ~2.0s / 1.52s / 2.4s.
    // With redraw_period=1.0s (frame_period=25) the 2.0s and 2.4s events
    // split by 1 each; the extra copies carry redraw_flags=true -> forced
    // ACQUISITION. Baseline (redraw=0) yields one PCS per event.
    encode_config_t baseline;
    baseline.input_path = OPENDSUP_FIXTURES_DIR "/synth_bdn.xml";
    baseline.output_path = OPENDSUP_TEST_OUTPUT_DIR "/redraw_baseline.sup";
    baseline.overwrite = true;
    baseline.redraw_period = 0.0;

    bdn_render_c r_base(baseline);
    const auto res_base = r_base.execute();
    ASSERT_TRUE(res_base.success) << res_base.error;
    const auto [pcs_off, acq_off] = count_pcs(r_base.segments());

    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/synth_bdn.xml";
    cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/redraw_on.sup";
    cfg.overwrite = true;
    cfg.redraw_period = 1.0;

    bdn_render_c r(cfg);
    const auto res = r.execute();
    ASSERT_TRUE(res.success) << res.error;
    const auto [pcs_on, acq_on] = count_pcs(r.segments());

    // Redraw must split long events -> more display sets than baseline.
    EXPECT_GT(pcs_on, pcs_off);
    // The forced copies must surface as ACQUISITION, not NORMAL updates.
    EXPECT_GT(acq_on, acq_off);
}

TEST(RedrawPeriod, DeterministicAcrossRuns) {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/synth_bdn.xml";
    cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/redraw_det1.sup";
    cfg.overwrite = true;
    cfg.redraw_period = 1.5;

    bdn_render_c r1(cfg);
    ASSERT_TRUE(r1.execute().success);
    const size_t h1 = stream_hash(r1.segments());

    cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/redraw_det2.sup";
    bdn_render_c r2(cfg);
    ASSERT_TRUE(r2.execute().success);
    const size_t h2 = stream_hash(r2.segments());

    EXPECT_EQ(h1, h2);
}

} // namespace
} // namespace core
} // namespace opensup
