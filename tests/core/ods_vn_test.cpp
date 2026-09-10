// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include <gtest/gtest.h>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/core/segments.h"

namespace opensup {
namespace core {
namespace {

/// An object re-emitted within the same epoch must carry an incremented
/// object_version_number (SUPer ods_reg, render2.py:702-704, :783-784, :845):
/// a mid-event acquisition reuses the current object id, so its ODS is the
/// second emission of that oid in the epoch and must carry version 1.
TEST(OdsVn, ReemittedObjectVersionIncrements) {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/synth_similar.xml";
    cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/ods_vn.sup";
    cfg.overwrite = true;
    cfg.extra_acq = 1;  // force a mid-event acquisition (same oid re-emission)

    bdn_render_c enc(cfg);
    const auto res = enc.execute();
    ASSERT_TRUE(res.success) << res.error;

    std::map<uint16_t, uint8_t> next_vn;  // next expected version per oid
    bool reemitted = false;
    for (const auto& seg : enc.segments()) {
        auto ods = std::dynamic_pointer_cast<ods_c>(seg);
        if (!ods)
            continue;
        const auto flags = ods->seq_flags();
        if (flags != ods_c::sequence_flags_e::single &&
            flags != ods_c::sequence_flags_e::first)
            continue;  // continuation fragment of a split bitmap
        const uint16_t oid = ods->o_id();
        auto& expected = next_vn[oid];
        if (expected > 0)
            reemitted = true;
        EXPECT_EQ(ods->o_vn(), expected) << "oid " << oid;
        ++expected;
    }
    EXPECT_TRUE(reemitted) << "fixture must re-emit at least one object id";
}

}  // namespace
}  // namespace core
}  // namespace opensup
