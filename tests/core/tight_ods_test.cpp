// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// 042/D-042-6: --tight-ods sizes each ODS to its event content.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/media/rle_decode.h"
#include "opensup/read/sup_reader.h"

namespace opensup {
namespace core {
namespace {

namespace fs = std::filesystem;

static void
encode(const std::string& input, const std::string& output, bool tight)
{
    encode_config_t cfg;
    cfg.input_path = input;
    cfg.output_path = output;
    cfg.overwrite = true;
    cfg.threads = 1;
    cfg.tight_ods = tight;
    bdn_render_c render(cfg);
    const auto res = render.execute();
    ASSERT_TRUE(res.success) << res.error;
}

static std::vector<uint8_t>
read_bytes(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// The transparent index is 0xFF, so padding shows up as a fully 0xFF edge.
static bool
edge_has_content(const std::vector<uint8_t>& px, int w, int h)
{
    for (int x = 0; x < w; ++x)
        if (px[x] != 0xFF || px[static_cast<size_t>(h - 1) * w + x] != 0xFF)
            return true;
    for (int y = 0; y < h; ++y)
        if (px[static_cast<size_t>(y) * w] != 0xFF ||
            px[static_cast<size_t>(y) * w + w - 1] != 0xFF)
            return true;
    return false;
}

TEST(TightOds, OptionCropsEveryOdsToItsContent) {
    const std::string repo = std::string(OPENDSUP_FIXTURES_DIR) + "/../..";
    const std::string bdn = repo + "/temp/ENIGMA-1080p/BDN_Index_1920.xml";
    if (!fs::exists(bdn)) GTEST_SKIP() << "ENIGMA-1080p fixture no local";

    const std::string out = OPENDSUP_TEST_OUTPUT_DIR "/tight_ods.sup";
    encode(bdn, out, true);

    auto stream = read::load_sup(out);
    ASSERT_FALSE(stream.display_sets.empty());
    size_t checked = 0;
    for (auto& ds : stream.display_sets) {
        for (const auto& ods : ds.ods()) {
            if (ods->seq_flags() != ods_c::sequence_flags_e::single) continue;
            const int w = ods->width(), h = ods->height();
            const auto px = media::decode_rle(ods->data(), w, h);
            ASSERT_EQ(px.size(), static_cast<size_t>(w) * static_cast<size_t>(h));
            EXPECT_TRUE(edge_has_content(px, w, h))
                << "ODS " << ods->o_id() << " keeps padding around its content";
            // Content starts at the PCS position: the CObject must point at
            // the box origin, so its on-screen rect is the content rect.
            const auto cobjs = ds.pcs().cobjects;
            const auto it = std::find_if(cobjs.begin(), cobjs.end(),
                [&](const c_object_t& c) { return c.o_id == ods->o_id(); });
            if (it != cobjs.end()) {
                EXPECT_LE(static_cast<int>(it->h_pos) + w, 1920);
                EXPECT_LE(static_cast<int>(it->v_pos) + h, 1080);
            }
            ++checked;
        }
    }
    EXPECT_GT(checked, 0u);
}

TEST(TightOds, DefaultKeepsSlotAndDisabledFlagIsByteIdentical) {
    const std::string fixture = OPENDSUP_FIXTURES_DIR "/ncbdn/BDN_Index.xml";
    const std::string base = OPENDSUP_TEST_OUTPUT_DIR "/tight_off_base.sup";
    const std::string off = OPENDSUP_TEST_OUTPUT_DIR "/tight_off_flag.sup";
    const std::string tight = OPENDSUP_TEST_OUTPUT_DIR "/tight_on_flag.sup";

    encode(fixture, base, false);
    encode(fixture, off, false);
    encode(fixture, tight, true);

    EXPECT_EQ(read_bytes(base), read_bytes(off)) << "default output changed";

    const auto a = read::load_sup(base);
    const auto b = read::load_sup(tight);
    ASSERT_EQ(a.display_sets.size(), b.display_sets.size());
    int larger = 0;
    for (size_t k = 0; k < a.display_sets.size(); ++k) {
        const auto oa = a.display_sets[k].ods();
        const auto ob = b.display_sets[k].ods();
        ASSERT_EQ(oa.size(), ob.size());
        for (size_t i = 0; i < oa.size(); ++i) {
            EXPECT_GE(oa[i]->width(), ob[i]->width());
            EXPECT_GE(oa[i]->height(), ob[i]->height());
            if (oa[i]->width() > ob[i]->width() || oa[i]->height() > ob[i]->height())
                ++larger;
        }
    }
    EXPECT_GT(larger, 0) << "fixture does not exercise the slot padding";
}

}  // namespace
}  // namespace core
}  // namespace opensup
