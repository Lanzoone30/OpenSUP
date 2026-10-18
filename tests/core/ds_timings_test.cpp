// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Display-set timing model: numbers pinned against the reference encoder on a
// 1080p fixture with a 585x105 object window.
#include <gtest/gtest.h>

#include <cmath>

#include "opensup/core/render/ds_timings.h"

namespace opensup {
namespace core {
namespace {

constexpr int64_t kFreq = 90000;

int64_t ticks(double secs) { return std::llround(secs * kFreq); }

// Copy time of a plane region at the compose rate, in ticks (ceil).
int64_t copy_ticks(int64_t area) {
    return (area * kFreq + 32000000 - 1) / 32000000;
}

window_definition_t window(uint8_t id, uint16_t w, uint16_t h) {
    window_definition_t wd;
    wd.window_id = id;
    wd.width = w;
    wd.height = h;
    return wd;
}

c_object_t cobj(uint16_t o_id, uint8_t window_id) {
    c_object_t c;
    c.o_id = o_id;
    c.window_id = window_id;
    return c;
}

TEST(DsTiming, EpochStartAddsPlaneAndWindowCopy) {
    ds_timing_input_t in;
    in.state = pcs_c::composition_state_e::epoch_start;
    in.plane_area = 1920ull * 1080ull;
    in.pts = 90090.0 / kFreq;
    in.windows = {window(0, 585, 105)};
    in.cobjects = {cobj(0, 0)};
    in.ods = {{0, 585ull * 105ull}};

    const auto t = compute_ds_timing(in);
    EXPECT_EQ(ticks(t.decode_duration), 6005);  // 5832 plane + 173 window copy
    EXPECT_EQ(ticks(t.wipe_duration), 173);
    EXPECT_EQ(ticks(in.pts - t.dts), 6005);
    ASSERT_EQ(t.ods.size(), 1u);
    EXPECT_EQ(ticks(t.ods[0].obj_decode), 346);
}

TEST(DsTiming, AcquisitionIntoAssignedWindow) {
    ds_timing_input_t in;
    in.state = pcs_c::composition_state_e::acquisition;
    in.plane_area = 1920ull * 1080ull;
    in.pts = 450450.0 / kFreq;
    in.windows = {window(0, 585, 105)};
    in.cobjects = {cobj(1, 0)};
    in.ods = {{1, 585ull * 105ull}};

    const auto t = compute_ds_timing(in);
    EXPECT_EQ(ticks(t.decode_duration), 519);  // 346 object decode + 173 window copy
}

TEST(DsTiming, EmptyCompositionHitsWindowClamp) {
    ds_timing_input_t in;
    in.state = pcs_c::composition_state_e::normal;
    in.pts = 270270.0 / kFreq;
    in.windows = {window(0, 585, 105)};

    const auto t = compute_ds_timing(in);
    EXPECT_EQ(ticks(t.decode_duration), 174);  // window copy + 1
}

TEST(DsTiming, PaletteUpdateSkipsObjectLoop) {
    ds_timing_input_t in;
    in.state = pcs_c::composition_state_e::normal;
    in.pal_flag = true;
    in.pts = 180180.0 / kFreq;
    in.windows = {window(0, 585, 105)};
    in.cobjects = {cobj(0, 0)};
    in.ods = {{0, 585ull * 105ull}};

    const auto t = compute_ds_timing(in);
    EXPECT_EQ(ticks(t.decode_duration), 174);
}

TEST(DsTiming, TwoObjectsInSameWindowCopyOnce) {
    ds_timing_input_t in;
    in.state = pcs_c::composition_state_e::acquisition;
    in.pts = 90090.0 / kFreq;
    in.windows = {window(0, 585, 105)};
    in.cobjects = {cobj(0, 0), cobj(1, 0)};
    in.ods = {{0, 585ull * 105ull}, {1, 585ull * 105ull}};

    const auto t = compute_ds_timing(in);
    EXPECT_EQ(ticks(t.decode_duration), 865);  // 2*346 decode + 1*173 copy
}

TEST(DsTiming, DecodeBaseAggregatesWindowCopies) {
    const uint64_t container = 1920ull * 1080ull;
    EXPECT_EQ(ticks(decode_base({}, container)), 5832);  // container copy only
    EXPECT_EQ(ticks(decode_base({window(0, 585, 105)}, container)), 6005);
    EXPECT_EQ(ticks(decode_base({window(0, 585, 105), window(1, 585, 105)}, container)), 6178);
}

TEST(DsTimings, WipeIsCeilOfSum) {
    // 1919x1079 and 100x101 each give a fractional copy time; the sum-of-ceils
    // and the ceil-of-sum differ, so wipe must be copy_ticks(a0 + a2).
    const int64_t a0 = 1919ll * 1079;
    const int64_t a2 = 100ll * 101;
    const int64_t seq_sum = copy_ticks(a0) + copy_ticks(a2);

    ds_timing_input_t in;
    in.windows = {window(0, 1919, 1079), window(1, 100, 101)};

    const auto t = compute_ds_timing(in);
    EXPECT_DOUBLE_EQ(t.wipe_duration, static_cast<double>(copy_ticks(a0 + a2)) / kFreq);
    EXPECT_NE(seq_sum, copy_ticks(a0 + a2));
}

}  // namespace
}  // namespace core
}  // namespace opensup
