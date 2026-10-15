// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Epoch boundary by the layout-driven decode-base check: a gap inside the
// (decode_base, epoch-start] band splits into two epochs; a back-to-back pair
// stays as one.
#include <gtest/gtest.h>

#include <string>

#include "opensup/core/interface.h"

namespace opensup {
namespace core {
namespace {

int epoch_count(const char* fixture) {
    encode_config_t cfg;
    cfg.input_path = std::string(OPENDSUP_FIXTURES_DIR) + "/" + fixture;
    cfg.output_path = std::string(OPENDSUP_TEST_OUTPUT_DIR) + "/epoch_split.sup";
    cfg.overwrite = true;
    cfg.extra_acq = 0;

    bdn_render_c render(cfg);
    const auto res = render.execute();
    EXPECT_TRUE(res.success) << res.error;
    return res.epochs;
}

TEST(EpochSplit, BandGapSplits) {
    EXPECT_EQ(epoch_count("epoch_band_gap.xml"), 2);
}

TEST(EpochSplit, BackToBackStaysOneEpoch) {
    EXPECT_EQ(epoch_count("epoch_backtoback.xml"), 1);
}

}  // namespace
}  // namespace core
}  // namespace opensup
