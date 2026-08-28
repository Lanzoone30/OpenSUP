// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// F1 (D4): find_acqs decode-margin signals (SUPer render2.py:1073-1135).
// Unit tests pin the ported formula: valid gate (render2.py:1132), dtl
// normalized by the previous node duration (margin = prev_dt/fps, :1125,
// :1133). The stream-level no-regression is gated by the full suite plus
// byte-identical fixture hashes (specs/006 tasks T001/T013).
#include <gtest/gtest.h>
#include "opensup/core/renderer.h"
namespace opensup {
namespace core {
namespace {
// Nominal PGS decoder rates (media/pgraphics.h) used by realistic timings.
constexpr double kWriteDur = 0.0648;   // 1920x1080 full-screen compose (RC)
constexpr double kObjDecode = 0.0068;  // 600x120 object decode+write (RD+RC)
TEST(FindAcqs, ValidComputesDtlOverMargin) {
    // Contiguous events: margin = previous event duration (render2.py:1125).
    // prev: pts=1.0; cur: pts=5.0, dts = 5.0-0.0068, prev_dts_end = 1.0-0.0068.
    const double slack = 4.0; // pts gap equals the slack here by construction
    const auto sig = find_acqs_signals(5.0 - kObjDecode, 1.0 - kObjDecode,
                                       5.0, 1.0, kWriteDur, 4.0);
    EXPECT_TRUE(sig.valid);
    EXPECT_DOUBLE_EQ(sig.dtl, slack / 4.0);
}
TEST(FindAcqs, InvalidDecodeStartGivesMinusOne) {
    // dts <= prev_dts_end: the object cannot decode after the previous one.
    const auto sig = find_acqs_signals(1.0, 1.0, 5.0, 1.0, kWriteDur, 4.0);
    EXPECT_FALSE(sig.valid);
    EXPECT_DOUBLE_EQ(sig.dtl, -1.0);
}
TEST(FindAcqs, InvalidPtsGapGivesMinusOne) {
    // pts - prev_pts <= write_duration (render2.py:1132, second clause).
    const auto sig = find_acqs_signals(5.0 - kObjDecode, 1.0 - kObjDecode,
                                       1.05, 1.0, kWriteDur, 4.0);
    EXPECT_FALSE(sig.valid);
    EXPECT_DOUBLE_EQ(sig.dtl, -1.0);
}
TEST(FindAcqs, MarginIsPreviousNodeDuration) {
    // render2.py:1122-1125: margin = prev_dt/fps where prev_dt is the previous
    // NODE duration — the inter-event gap when a wipe node exists, else the
    // previous event duration. Same slack, different normalizer: dtl scales
    // inversely (this pins the frame/duration semantics; dividing by the
    // previous object's wipe-duration instead would produce different ratios).
    const double dts = 5.0 - kObjDecode;
    const double prev_end = 1.0 - kObjDecode;
    const auto gap_margin = find_acqs_signals(dts, prev_end, 5.0, 1.0,
                                              kWriteDur, 0.32);  // gap node
    const auto dur_margin = find_acqs_signals(dts, prev_end, 5.0, 1.0,
                                              kWriteDur, 2.0);   // event node
    ASSERT_TRUE(gap_margin.valid);
    ASSERT_TRUE(dur_margin.valid);
    EXPECT_DOUBLE_EQ(gap_margin.dtl / dur_margin.dtl, 2.0 / 0.32);
}
TEST(FindAcqs, DefaultsMatchInvalidSignal) {
    // Zero-initialized acq_signals_t carries the invalid-marker defaults.
    const acq_signals_t sig;
    EXPECT_FALSE(sig.valid);
    EXPECT_DOUBLE_EQ(sig.dtl, -1.0);
    EXPECT_FALSE(sig.absolute);
}
} // namespace
} // namespace core
} // namespace opensup
