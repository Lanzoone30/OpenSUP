// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

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

TEST(ExtraAcq, OffVsOnInsertsMidEventAcquisitions) {
    // Same input; extra_acq=1 must insert mid-event ACQUISITION display sets.
    // synth_similar.xml: 3 different-width events (non-reusable, NORMAL chain).
    encode_config_t cfg_off;
    cfg_off.input_path = OPENDSUP_FIXTURES_DIR "/synth_similar.xml";
    cfg_off.output_path = "extra_acq_off.sup";
    cfg_off.overwrite = true;
    cfg_off.extra_acq = 0;

    encode_config_t cfg_on = cfg_off;
    cfg_on.output_path = "extra_acq_on.sup";
    cfg_on.extra_acq = 1;

    bdn_render_c off(cfg_off);
    bdn_render_c on(cfg_on);
    const auto res_off = off.execute();
    const auto res_on = on.execute();

    ASSERT_TRUE(res_off.success) << res_off.error;
    ASSERT_TRUE(res_on.success) << res_on.error;
    EXPECT_GT(res_on.segments, res_off.segments)
        << "extra_acq=1 must add mid-event acquisition display sets";
    // Each inserted acquisition is a full display set: PCS+WDS+PDS+ODS+ENDS.
    EXPECT_EQ((res_on.segments - res_off.segments) % 5, 0);
    EXPECT_NE(stream_hash(off.segments()), stream_hash(on.segments()));
}

TEST(ExtraAcq, Deterministic) {
    // Same config twice → byte-identical stream.
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/synth_similar.xml";
    cfg.output_path = "extra_acq_det1.sup";
    cfg.overwrite = true;
    cfg.extra_acq = 1;

    encode_config_t cfg2 = cfg;
    cfg2.output_path = "extra_acq_det2.sup";

    bdn_render_c r1(cfg);
    bdn_render_c r2(cfg2);
    ASSERT_TRUE(r1.execute().success);
    ASSERT_TRUE(r2.execute().success);
    EXPECT_EQ(stream_hash(r1.segments()), stream_hash(r2.segments()));
}

}  // namespace
}  // namespace core
}  // namespace opensup
