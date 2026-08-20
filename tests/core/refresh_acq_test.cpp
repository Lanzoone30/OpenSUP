// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.
//
// P3: decode-margin model (SUPer render2.py find_acqs/shape_stream).
// Reusable (same-object) events with enough decode margin are upgraded to
// ACQUISITION (refresh) instead of staying NORMAL palette updates.

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "opensup/core/interface.h"

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

TEST(RefreshAcq, HigherCompressionSuppressesRefresh) {
    // synth_similar.xml: 3 SSIM-similar (score ~1.0 > 0.986 threshold) events,
    // 4.0s each. Events 1..3 fuse via SSIM -> reusable candidates for the
    // decode-margin (refresh) path; higher compression (thresh=1.0) suppresses
    // refreshes, so it must emit fewer segments than thresh=0.8.
    // Events 1 and 2 are reusable candidates. Decode-margin ratio dtl:
    //   k=1 (gap after event 0): ~1.24  -> fires at thresh 1.0 too
    //   k=2 (contiguous):         ~0.997 -> fires at thresh 0.8, not at 1.0
    // So compression=100 must emit fewer segments than compression=80.
    encode_config_t cfg_hi;
    cfg_hi.input_path = OPENDSUP_FIXTURES_DIR "/synth_similar.xml";
    cfg_hi.output_path = "refresh_acq_hi.sup";
    cfg_hi.overwrite = true;
    cfg_hi.compression = 100;  // thresh = 1.0
    cfg_hi.extra_acq = 0;      // isolate the refresh path from extra_acq

    encode_config_t cfg_lo = cfg_hi;
    cfg_lo.output_path = "refresh_acq_lo.sup";
    cfg_lo.compression = 80;   // thresh = 0.8 (default)

    bdn_render_c hi(cfg_hi);
    bdn_render_c lo(cfg_lo);
    const auto res_hi = hi.execute();
    const auto res_lo = lo.execute();
    ASSERT_TRUE(res_hi.success) << res_hi.error;
    ASSERT_TRUE(res_lo.success) << res_lo.error;
    EXPECT_GT(res_lo.segments, res_hi.segments)
        << "lower compression (thresh) must allow more refresh acquisitions";
    EXPECT_NE(stream_hash(hi.segments()), stream_hash(lo.segments()));
}

TEST(RefreshAcq, Deterministic) {
    // Same config twice -> byte-identical stream.
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/synth_bdn.xml";
    cfg.output_path = "refresh_acq_det1.sup";
    cfg.overwrite = true;
    cfg.extra_acq = 0;

    encode_config_t cfg2 = cfg;
    cfg2.output_path = "refresh_acq_det2.sup";

    bdn_render_c r1(cfg);
    bdn_render_c r2(cfg2);
    ASSERT_TRUE(r1.execute().success);
    ASSERT_TRUE(r2.execute().success);
    EXPECT_EQ(stream_hash(r1.segments()), stream_hash(r2.segments()));
}

}  // namespace
}  // namespace core
}  // namespace opensup
