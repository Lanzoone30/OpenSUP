// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Drought cadence parity: composition states must match the reference oracle
// for synth_drought at compression 80 and 100 (see oracle_refs.md).

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/core/segments.h"

namespace opensup {
namespace core {
namespace {

using state_t = pcs_c::composition_state_e;

struct ds_t {
    uint32_t pts;
    state_t state;
};

std::vector<ds_t> encode_states(const std::string& fixture, int compression,
                               int extra_acq = 0)
{
    encode_config_t cfg;
    cfg.input_path = std::string(OPENDSUP_FIXTURES_DIR) + "/" + fixture;
    cfg.output_path = std::string(OPENDSUP_TEST_OUTPUT_DIR) + "/drought_acq.sup";
    cfg.overwrite = true;
    cfg.compression = compression;
    cfg.extra_acq = extra_acq;

    bdn_render_c render(cfg);
    const auto res = render.execute();
    EXPECT_TRUE(res.success) << res.error;

    std::vector<ds_t> out;
    for (const auto& s : render.segments()) {
        if (auto p = dynamic_cast<pcs_c*>(s.get()))
            out.push_back({p->tpts(), p->composition_state()});
    }
    return out;
}

TEST(DroughtAcquisition, Compression80MatchesOracle) {
    const auto ds = encode_states("synth_drought.xml", 80);
    const std::vector<state_t> expected = {
        state_t::epoch_start, state_t::acquisition, state_t::acquisition,
        state_t::acquisition, state_t::normal};
    ASSERT_EQ(ds.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++)
        EXPECT_EQ(ds[i].state, expected[i]) << "display set " << i;
}

TEST(DroughtAcquisition, Compression100MatchesOracle) {
    const auto ds = encode_states("synth_drought.xml", 100);
    const std::vector<state_t> expected = {
        state_t::epoch_start, state_t::normal, state_t::acquisition,
        state_t::normal, state_t::normal};
    ASSERT_EQ(ds.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++)
        EXPECT_EQ(ds[i].state, expected[i]) << "display set " << i;
}

TEST(DroughtAcquisition, NoSpuriousDisplaySet) {
    // The 1173600 acquisition the pre-fix cadence inserted must be gone.
    const auto ds = encode_states("synth_drought.xml", 80);
    const std::vector<uint32_t> expected_pts = {
        90000, 450000, 810000, 1170000, 1530000};
    ASSERT_EQ(ds.size(), expected_pts.size());
    for (size_t i = 0; i < expected_pts.size(); i++)
        EXPECT_EQ(ds[i].pts, expected_pts[i]) << "display set " << i;
}

TEST(DroughtAcquisition, EpochStartOnlyFirstDisplaySet) {
    for (const char* fixture :
         {"synth_drought.xml", "synth_similar.xml", "synth_ssimband.xml"}) {
        const auto ds = encode_states(fixture, 80);
        ASSERT_FALSE(ds.empty()) << fixture;
        EXPECT_EQ(ds.front().state, state_t::epoch_start) << fixture;
        for (size_t i = 1; i < ds.size(); i++)
            EXPECT_NE(ds[i].state, state_t::epoch_start)
                << fixture << " display set " << i;
    }
}

}  // namespace
}  // namespace core
}  // namespace opensup
