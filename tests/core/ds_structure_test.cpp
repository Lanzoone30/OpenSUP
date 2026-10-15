// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// FR-007: every display set is exactly one PCS followed by its payload and a
// closing END. A refresh re-send used to leak a bare `ODS, END` pair (its own
// display set without a PCS), which no decoder can parse.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/core/segments.h"

namespace opensup {
namespace core {
namespace {

void expect_well_formed(const char* fixture) {
    encode_config_t cfg;
    cfg.input_path = std::string(OPENDSUP_FIXTURES_DIR) + "/" + fixture;
    cfg.output_path = std::string(OPENDSUP_TEST_OUTPUT_DIR) + "/ds_structure.sup";
    cfg.overwrite = true;
    cfg.compression = 80;
    cfg.extra_acq = 0;

    bdn_render_c render(cfg);
    const auto res = render.execute();
    ASSERT_TRUE(res.success) << res.error;
    const auto& segs = render.segments();
    ASSERT_FALSE(segs.empty());

    int pcs_in_ds = 0;
    bool open = false;
    for (size_t i = 0; i < segs.size(); i++) {
        const bool first_of_ds = !open;
        if (dynamic_cast<pcs_c*>(segs[i].get())) {
            EXPECT_TRUE(first_of_ds) << "PCS after payload at segment " << i;
            pcs_in_ds = 1;
            open = true;
        } else if (dynamic_cast<ends_c*>(segs[i].get())) {
            EXPECT_EQ(pcs_in_ds, 1) << "display set without a PCS at segment " << i;
            pcs_in_ds = 0;
            open = false;
        } else if (dynamic_cast<ods_c*>(segs[i].get()) ||
                   dynamic_cast<wds_c*>(segs[i].get()) ||
                   dynamic_cast<pds_c*>(segs[i].get())) {
            EXPECT_TRUE(open) << "payload before its PCS at segment " << i;
        }
    }
    EXPECT_FALSE(open) << "stream does not end with END";
}

TEST(DsStructure, RefreshResendKeepsOnePcsPerDisplaySet) {
    expect_well_formed("synth_similar.xml");
}

TEST(DsStructure, RedrawKeepsOnePcsPerDisplaySet) {
    expect_well_formed("synth_bdn.xml");
}

}  // namespace
}  // namespace core
}  // namespace opensup
