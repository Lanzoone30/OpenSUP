// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Every non-NORMAL display set must carry a complete palette: decoders drop
// the CLUT before applying it, so a delta leaves omitted indices invisible.
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/core/segments.h"
#include "opensup/media/rle_decode.h"

namespace opensup {
namespace core {
namespace {

using ds_t = std::vector<std::shared_ptr<pg_segment_c>>;

std::vector<ds_t> split_ds(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    std::vector<ds_t> dss;
    for (const auto& seg : segs) {
        if (dss.empty() || std::dynamic_pointer_cast<pcs_c>(seg))
            dss.push_back({});
        dss.back().push_back(seg);
    }
    return dss;
}

/// Every non-NORMAL DS must define, with alpha > 0, each index its objects draw.
void expect_complete_palettes(const std::vector<std::shared_ptr<pg_segment_c>>& segs,
                              int& checked, int& reused_id) {
    std::set<uint8_t> seen_ids;
    for (const auto& ds : split_ds(segs)) {
        auto pcs = std::dynamic_pointer_cast<pcs_c>(ds.front());
        ASSERT_TRUE(pcs);
        if (pcs->composition_state() == pcs_c::composition_state_e::normal)
            continue;
        std::vector<std::vector<uint8_t>> images;
        std::map<uint8_t, media::palette_entry_t> entries;
        for (size_t i = 0; i < ds.size(); ++i) {
            if (auto pds = std::dynamic_pointer_cast<pds_c>(ds[i])) {
                const auto pal = pds->to_palette();
                for (const auto& [idx, e] : pal.entries()) entries[idx] = e;
                continue;
            }
            auto ods = std::dynamic_pointer_cast<ods_c>(ds[i]);
            if (!ods) continue;
            const auto flags = ods->seq_flags();
            if (flags != ods_c::sequence_flags_e::single &&
                flags != ods_c::sequence_flags_e::first)
                continue;
            auto rle = ods->data();
            for (size_t j = i + 1; j < ds.size(); ++j) {
                auto next = std::dynamic_pointer_cast<ods_c>(ds[j]);
                if (!next || next->o_id() != ods->o_id()) continue;
                if (next->seq_flags() == ods_c::sequence_flags_e::last)
                    rle.insert(rle.end(), next->data().begin(), next->data().end());
                break;
            }
            bool ok = false;
            auto idx = media::decode_rle_checked(rle, ods->width(), ods->height(), ok);
            ASSERT_TRUE(ok);
            images.push_back(std::move(idx));
        }
        if (images.empty()) continue;
        ASSERT_FALSE(entries.empty());

        for (const auto& idx : images) {
            for (uint8_t v : idx) {
                if (v == 0xFF) continue;
                auto it = entries.find(v);
                ASSERT_TRUE(it != entries.end())
                    << "acquisition PDS misses index " << static_cast<int>(v);
                EXPECT_NE(it->second.alpha, 0);
            }
        }
        if (!seen_ids.insert(pcs->pal_id()).second) reused_id++;
        checked++;
    }
}

TEST(AcquisitionPalette, PdsCoversEveryOdsIndex) {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/ncbdn/BDN_Index.xml";
    cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/acq_palette.sup";
    cfg.overwrite = true;
    cfg.extra_acq = 0;

    bdn_render_c render(cfg);
    const auto res = render.execute();
    ASSERT_TRUE(res.success) << res.error;

    int checked = 0, reused_id = 0;
    expect_complete_palettes(render.segments(), checked, reused_id);
    EXPECT_GT(checked, 0);
    // At least one acquisition reuses a palette id already sent this epoch,
    // which is the case a delta would break.
    EXPECT_GT(reused_id, 0);
}

// --ahead buffers palette updates ahead of the object; the palette of a
// non-NORMAL DS must still define every index its objects draw.
TEST(AheadPalette, PdsCoversEveryOdsIndex) {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/ncbdn/BDN_Index.xml";
    cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/ahead_palette.sup";
    cfg.overwrite = true;
    cfg.extra_acq = 0;
    cfg.overlap = true;

    bdn_render_c render(cfg);
    const auto res = render.execute();
    ASSERT_TRUE(res.success) << res.error;

    int checked = 0, reused_id = 0;
    expect_complete_palettes(render.segments(), checked, reused_id);
    EXPECT_GT(checked, 0);
}

}  // namespace
}  // namespace core
}  // namespace opensup
