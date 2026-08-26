// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Parameter parity with SUPer: ssim_tol and acqrate (refresh rate).
// CTU parity: separated regions break like SUPer (D2).

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/core/segments.h"
namespace opensup {
namespace core {
namespace {

/// Segment payload hash (FNV-1a over raw bytes) — byte-exact equality check.
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

/// Concatenated hash of every segment's bytes (order matters).
static size_t stream_hash(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    size_t h = 14695981039346656037ull;
    for (const auto& s : segs) {
        h ^= SegmentHasher::hash(s);
        h *= 1099511628211ull;
    }
    return h;
}

TEST(SSIMTol, ExtremesDoNotShiftBaseThreshold) {
    // D1 regression: SUPer applies ssim_offset ONLY inside thr_score
    // (render2.py:1366: -0.008333*(1-ssim_offset)), never to the base SSIM
    // threshold. synth_ssimband.xml: flat gray 128 vs 155 scores 0.9820,
    // which fuses with the correct threshold (0.9778) but would break if
    // ssim_tol=+100 added its 0.014 offset to the base threshold (0.9918).
    // Both extremes must therefore produce byte-identical streams.
    encode_config_t cfg_lo;
    cfg_lo.input_path = OPENDSUP_FIXTURES_DIR "/synth_ssimband.xml";
    cfg_lo.output_path = "ssimtol_lo.sup";
    cfg_lo.overwrite = true;
    cfg_lo.extra_acq = 0;
    cfg_lo.ssim_tol = -100;

    encode_config_t cfg_hi = cfg_lo;
    cfg_hi.output_path = "ssimtol_hi.sup";
    cfg_hi.ssim_tol = 100;

    bdn_render_c lo(cfg_lo);
    bdn_render_c hi(cfg_hi);
    const auto res_lo = lo.execute();
    const auto res_hi = hi.execute();
    ASSERT_TRUE(res_lo.success) << res_lo.error;
    ASSERT_TRUE(res_hi.success) << res_hi.error;
    EXPECT_EQ(res_lo.segments, res_hi.segments)
        << "ssim_tol extremes must not change fuse/break decisions";
    EXPECT_EQ(stream_hash(lo.segments()), stream_hash(hi.segments()));
}

TEST(AcqRate, DroughtAccumulationEnablesLateRefresh) {
    // synth_drought.xml at compression=100 (thresh=1.0): event 3 (contiguous,
    // dtl ~0.997) cannot refresh, so the drought accumulates by refresh_rate.
    // Event 4 (identical to event 1, dtl=1.0) then refreshes only when the
    // drought has grown (1.0 > 1.0 - 0.035*drought), i.e. at acqrate=100.
    // With acqrate=0 the drought stays 0 and event 4 must NOT refresh.
    encode_config_t cfg_zero;
    cfg_zero.input_path = OPENDSUP_FIXTURES_DIR "/synth_drought.xml";
    cfg_zero.output_path = "acqrate_zero.sup";
    cfg_zero.overwrite = true;
    cfg_zero.extra_acq = 0;
    cfg_zero.compression = 100;
    cfg_zero.acqrate = 0;

    encode_config_t cfg_full = cfg_zero;
    cfg_full.output_path = "acqrate_full.sup";
    cfg_full.acqrate = 100;

    bdn_render_c zero(cfg_zero);
    bdn_render_c full(cfg_full);
    const auto res_zero = zero.execute();
    const auto res_full = full.execute();
    ASSERT_TRUE(res_zero.success) << res_zero.error;
    ASSERT_TRUE(res_full.success) << res_full.error;
    EXPECT_NE(stream_hash(zero.segments()), stream_hash(full.segments()))
        << "acqrate must scale drought and change emission pattern";
}

TEST(CTU, SeparatedRegionsBreakLikeSUPer) {
    // D2 regression: SUPer CTU (render2.py:1186-1245) discounts identical regions
    // by 0.325 so a diverging region is not diluted by unchanged ones.
    // Fixture: two spatially separated bands (top 60px identical gray=128,
    // bottom 60px noise delta=40). Flat SSIM fuses (score > thr), CTU breaks.
    // Expected: 2nd display set = ACQUISITION (not palette update).
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/ctu_bands_delta40.xml";
    cfg.output_path = "ctu_test.sup";
    cfg.overwrite = true;
    cfg.extra_acq = 0;
    cfg.ssim_tol = 0;
    cfg.compression = 80;
    cfg.acqrate = 0;

    bdn_render_c renderer(cfg);
    const auto res = renderer.execute();
    ASSERT_TRUE(res.success) << res.error;

    const auto& segs = renderer.segments();
    // Find PCS segments (composition states)
    int acquisition_count = 0;
    int normal_count = 0;
    for (const auto& seg : segs) {
        if (seg->type() == segment_type_e::pcs) {
            auto pcs = std::dynamic_pointer_cast<pcs_c>(seg);
            if (pcs->composition_state() == pcs_c::composition_state_e::acquisition) {
                acquisition_count++;
            } else if (pcs->composition_state() == pcs_c::composition_state_e::normal) {
                normal_count++;
            }
        }
    }
    // Event 1: EPOCH_START (counts as acquisition)
    // Event 2: must be ACQUISITION (CTU breaks due to delta=40 in bottom band)
    EXPECT_EQ(acquisition_count, 2) << "CTU must break for separated bands delta=40 (like SUPer)";
    EXPECT_EQ(normal_count, 0) << "No palette update (NORMAL) allowed for this delta";
}

}  // namespace
}  // namespace core
}  // namespace opensup