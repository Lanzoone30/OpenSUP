// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// F2 (US1/US3): ahead/allow_overlaps pipeline (SUPer render2.py:286-313,
// :409-453). With --overlap the chain emission follows SUPer's undisplay
// rule (clear only on gap nodes, render2.py:859) and the emitted stream
// keeps a strictly monotonic DTS (align_palette_updates, render2.py:286-313).
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include "opensup/core/interface.h"
#include "opensup/core/segments.h"
namespace opensup {
namespace core {
namespace {
size_t stream_hash(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    size_t h = 14695981039346656037ull;
    for (const auto& s : segs) {
        for (uint8_t b : s->to_bytes()) {
            h ^= b;
            h *= 1099511628211ull;
        }
    }
    return h;
}
/// Number of undisplay (clear) display sets: PCS with no composition objects.
static int count_clear_ds(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    int clears = 0;
    for (const auto& seg : segs) {
        auto pcs = std::dynamic_pointer_cast<pcs_c>(seg);
        if (pcs && pcs->cobjects.empty())
            clears++;
    }
    return clears;
}
/// Encode synth_similar.xml (3 SSIM-fused contiguous events, 4s each) and
/// keep the renderer alive so its segments stay inspectable.
static bdn_render_c encode_similar(const std::string& out, bool overlap) {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/synth_similar.xml";
    cfg.output_path = out;
    cfg.overwrite = true;
    cfg.extra_acq = 0;
    cfg.overlap = overlap;
    bdn_render_c render(cfg);
    const auto res = render.execute();
    EXPECT_TRUE(res.success) << res.error;
    return render;
}
static bdn_render_c encode_multiwindow(const std::string& fixture,
                                   const std::string& out, bool overlap) {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/" + fixture;
    cfg.output_path = out;
    cfg.overwrite = true;
    cfg.extra_acq = 0;
    cfg.overlap = overlap;
    bdn_render_c render(cfg);
    const auto res = render.execute();
    EXPECT_TRUE(res.success) << res.error;
    return render;
}
/// Collect composition states of all PCS (with an object) in order.
static std::vector<pcs_c::composition_state_e>
pcs_states(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    std::vector<pcs_c::composition_state_e> out;
    for (const auto& seg : segs) {
        auto pcs = std::dynamic_pointer_cast<pcs_c>(seg);
        if (pcs && !pcs->cobjects.empty())
            out.push_back(pcs->composition_state());
    }
    return out;
}
TEST(Ahead, MonotonicDtsWithOverlap) {
    // align_palette_updates port (render2.py:286-313): with buffered palette
    // updates every segment decodes at or after the previous one — DTS never
    // goes backward, and PTS stays >= DTS on every segment.
    auto render = encode_similar(OPENDSUP_TEST_OUTPUT_DIR "/ahead_mono.sup", true);
    const auto& segs = render.segments();
    ASSERT_FALSE(segs.empty());
    double last_dts = -1.0;
    for (const auto& seg : segs) {
        EXPECT_GE(seg->dts(), last_dts)
            << "DTS went backward at pts=" << seg->pts();
        EXPECT_GE(seg->pts(), seg->dts())
            << "PTS before DTS (decode after presentation)";
        last_dts = seg->dts();
    }
}
TEST(Ahead, ContiguousChainEmitsNoClearWithOverlap) {
    // US1 (render2.py:859): the undisplay clear exists only for gap (wipe)
    // nodes. synth_similar.xml events are contiguous, so with overlap only
    // the end-of-epoch clear remains; without overlap the legacy clear
    // pattern emits one clear per chain event boundary.
    auto on = encode_similar(OPENDSUP_TEST_OUTPUT_DIR "/ahead_on.sup", true);
    auto off = encode_similar(OPENDSUP_TEST_OUTPUT_DIR "/ahead_off.sup", false);
    // Contiguous fused events: exactly one clear left (the final wipe).
    EXPECT_EQ(count_clear_ds(on.segments()), 1);
    // Legacy pattern: clears between every contiguous event pair + final.
    EXPECT_GT(count_clear_ds(off.segments()), count_clear_ds(on.segments()));
}
TEST(Ahead, OverlapChangesStreamButNotDefault) {
    // FR-6: default output is untouched by the overlap pipeline; the flag
    // must actually activate it (different stream) and stay deterministic.
    auto on1 = encode_similar(OPENDSUP_TEST_OUTPUT_DIR "/ahead_d1.sup", true);
    auto on2 = encode_similar(OPENDSUP_TEST_OUTPUT_DIR "/ahead_d2.sup", true);
    auto off1 = encode_similar(OPENDSUP_TEST_OUTPUT_DIR "/ahead_d3.sup", false);
    auto off2 = encode_similar(OPENDSUP_TEST_OUTPUT_DIR "/ahead_d4.sup", false);
    EXPECT_EQ(stream_hash(on1.segments()), stream_hash(on2.segments()));
    EXPECT_EQ(stream_hash(off1.segments()), stream_hash(off2.segments()));
    EXPECT_NE(stream_hash(on1.segments()), stream_hash(off1.segments()));
}
} // namespace

TEST(Ahead, ShiftForwardOverlayMergesAcq) {
    // US2 (render2.py:315-408): an acquisition that does not fit (gap < write
    // duration) is shifted forward into the best previous node of the other
    // window. opensup_multiwindow_merge.xml: ev1 window A, ev2 window B only
    // 40ms later — ev2's decode collides, so the acquisition moves to ev1 and
    // ev2 becomes NORMAL.
    auto on = encode_multiwindow("opensup_multiwindow_merge.xml",
                                 OPENDSUP_TEST_OUTPUT_DIR "/ahead_merge_on.sup", true);
    const auto st_on = pcs_states(on.segments());
    ASSERT_EQ(st_on.size(), 2u);
    EXPECT_EQ(st_on[0], pcs_c::composition_state_e::acquisition)
        << "acquisition should move to the earlier (promoted) node";
    EXPECT_EQ(st_on[1], pcs_c::composition_state_e::normal)
        << "the absorbed node must become NORMAL";
    // Without overlap the same events keep independent acquisitions
    // (a new object in a different window is an ACQUISITION, no shift).
    auto off = encode_multiwindow("opensup_multiwindow_merge.xml",
                                  OPENDSUP_TEST_OUTPUT_DIR "/ahead_merge_off.sup", false);
    const auto st_off = pcs_states(off.segments());
    ASSERT_EQ(st_off.size(), 2u);
    EXPECT_EQ(st_off[1], pcs_c::composition_state_e::acquisition)
        << "non-overlap keeps the independent acquisition (no shift)";
}
TEST(Ahead, ShiftForwardOverlayBlocked) {
    // US2 acceptance scenario 2: an acquisition that fits (gap large enough)
    // is NOT shifted — shift_forward_overlay only moves acquisitions whose
    // decode collides (absolutes && !acqs). opensup_multiwindow.xml has a
    // 2.48s gap, so ev2 stays an independent ACQUISITION.
    auto on = encode_multiwindow("opensup_multiwindow.xml",
                                 OPENDSUP_TEST_OUTPUT_DIR "/ahead_blocked.sup", true);
    const auto st = pcs_states(on.segments());
    ASSERT_EQ(st.size(), 2u);
    EXPECT_EQ(st[0], pcs_c::composition_state_e::acquisition);
    EXPECT_EQ(st[1], pcs_c::composition_state_e::acquisition)
        << "acquisition that fits must not be shifted";
}

} // namespace core
} // namespace opensup
