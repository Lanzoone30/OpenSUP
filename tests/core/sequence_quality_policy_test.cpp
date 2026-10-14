// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include <gtest/gtest.h>

#include "opensup/core/render/sequence_quality_policy.h"

namespace opensup {
namespace core {
namespace {

using policy_t = sequence_quality_policy_c;

constexpr auto kAcq = pcs_c::composition_state_e::acquisition;
constexpr auto kNormal = pcs_c::composition_state_e::normal;
constexpr auto kEpochStart = pcs_c::composition_state_e::epoch_start;

policy_t make_policy(double quality = 0.8, double refresh = 1.0,
                     double ssim_tol = 0.0, int extra_acq = 2, int height = 1080)
{
    policy_t::options_t o;
    o.quality_factor = quality;
    o.refresh_rate = refresh;
    o.ssim_tol = ssim_tol;
    o.insert_acquisitions = extra_acq;
    o.height = height;
    return policy_t(o);
}

} // namespace

TEST(SequenceQualityPolicy, CompressionZeroForcesAcquisition) {
    auto p = make_policy(0.0);
    const auto d = p.decide(false, false, false, false, false, 0.0);
    EXPECT_EQ(d.comp_state, kAcq);
    EXPECT_FALSE(d.reusable_refresh);
}

TEST(SequenceQualityPolicy, PlainEventIsEpochStart) {
    auto p = make_policy();
    const auto d = p.decide(false, false, false, false, false, 0.0);
    EXPECT_EQ(d.comp_state, kEpochStart);
}

TEST(SequenceQualityPolicy, ForcedEventIsAcquisition) {
    auto p = make_policy();
    const auto d = p.decide(false, false, /*forced=*/true, false, false, 0.0);
    EXPECT_EQ(d.comp_state, kAcq);
}

TEST(SequenceQualityPolicy, ReusableWithMarginRefreshes) {
    auto p = make_policy();
    const auto d = p.decide(false, /*reusable=*/true, false, false,
                            /*acq_valid=*/true, /*dtl=*/0.9);
    EXPECT_EQ(d.comp_state, kNormal);
    EXPECT_TRUE(d.reusable_refresh);
}

TEST(SequenceQualityPolicy, ReusableWithoutMarginDoesNotRefresh) {
    auto p = make_policy();
    const auto d = p.decide(false, true, false, false, true, /*dtl=*/0.1);
    EXPECT_EQ(d.comp_state, kNormal);
    EXPECT_FALSE(d.reusable_refresh);
}

TEST(SequenceQualityPolicy, DroughtLowersEffectiveThreshold) {
    // thresh=0.8, decay=0.035, refresh=1.0. A constant margin fails until the
    // accumulated drought pulls the threshold below it.
    auto p = make_policy();
    EXPECT_FALSE(p.decide(false, true, false, false, true, 0.75).reusable_refresh);
    EXPECT_FALSE(p.decide(false, true, false, false, true, 0.75).reusable_refresh);
    EXPECT_TRUE(p.decide(false, true, false, false, true, 0.75).reusable_refresh);
}

TEST(SequenceQualityPolicy, ExtraAcqFiresAfterEnoughNormalEvents) {
    auto p = make_policy(0.8, 1.0, 0.0, /*extra_acq=*/2);
    EXPECT_FALSE(p.acquisition_due(false));
    for (int i = 0; i < 3; ++i)
        (void)p.decide(false, true, false, false, true, 0.1);
    EXPECT_TRUE(p.acquisition_due(false));
    EXPECT_FALSE(p.acquisition_due(/*forced=*/true));
}

TEST(SequenceQualityPolicy, ExtraAcqDisabledNeverFires) {
    auto p = make_policy(0.8, 1.0, 0.0, /*extra_acq=*/0);
    for (int i = 0; i < 10; ++i)
        (void)p.decide(false, true, false, false, true, 0.1);
    EXPECT_FALSE(p.acquisition_due(false));
}

TEST(SequenceQualityPolicy, NoteAcquisitionResetsCounter) {
    auto p = make_policy(0.8, 1.0, 0.0, 2);
    for (int i = 0; i < 3; ++i)
        (void)p.decide(false, true, false, false, true, 0.1);
    ASSERT_TRUE(p.acquisition_due(false));
    p.note_acquisition();
    EXPECT_FALSE(p.acquisition_due(false));
}

TEST(SequenceQualityPolicy, BaseSsimTracksHeight) {
    EXPECT_NEAR(make_policy(0.8, 1.0, 0.0, 2, 1080).base_ssim_threshold(), 0.986, 1e-9);
    EXPECT_NEAR(make_policy(0.8, 1.0, 0.0, 2, 480).base_ssim_threshold(), 0.972, 1e-9);
}

TEST(SequenceQualityPolicy, SsimTolRaisesFuseThreshold) {
    const double base = make_policy(0.8, 1.0, 0.0, 2, 1080).fuse_threshold(0.5);
    const double high = make_policy(0.8, 1.0, 100.0, 2, 1080).fuse_threshold(0.5);
    EXPECT_GT(high, base);
}

} // namespace core
} // namespace opensup
