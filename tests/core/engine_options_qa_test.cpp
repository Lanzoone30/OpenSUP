// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// 039/T008: engine option flags change the stream as the oracle does.
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "opensup/core/interface.h"
#include "opensup/read/inspect.h"
#include "opensup/read/sup_reader.h"

namespace opensup {
namespace core {
namespace {

namespace fs = std::filesystem;

static encode_result_t
encode(const std::string& input, const std::string& output, bool overlap,
       bool full_palette, bool both_formats)
{
    encode_config_t cfg;
    cfg.input_path = input;
    cfg.output_path = output;
    cfg.overwrite = true;
    cfg.overlap = overlap;
    cfg.full_palette = full_palette;
    cfg.both_formats = both_formats;
    bdn_render_c render(cfg);
    const auto res = render.execute();
    EXPECT_TRUE(res.success) << res.error;
    return res;
}

TEST(EngineOptionsQa, OverlapNeverGrowsTheStreamAndStaysCompliant) {
    const std::string base = OPENDSUP_TEST_OUTPUT_DIR "/qa_overlap_base.sup";
    const std::string over = OPENDSUP_TEST_OUTPUT_DIR "/qa_overlap_on.sup";
    encode(OPENDSUP_FIXTURES_DIR "/ncbdn/BDN_Index.xml", base, false, false, false);
    encode(OPENDSUP_FIXTURES_DIR "/ncbdn/BDN_Index.xml", over, true, false, false);

    EXPECT_LE(fs::file_size(over), fs::file_size(base));
    auto stream = read::load_sup(over);
    const auto rep = read::validate(stream);
    EXPECT_TRUE(rep.compliant)
        << (rep.findings.empty() ? "" : rep.findings.front().detail);
}

TEST(EngineOptionsQa, FullPaletteNeverShrinksAndEachPuCarriesAPds) {
    const std::string base = OPENDSUP_TEST_OUTPUT_DIR "/qa_full_base.sup";
    const std::string full = OPENDSUP_TEST_OUTPUT_DIR "/qa_full_on.sup";
    encode(OPENDSUP_FIXTURES_DIR "/ncbdn/BDN_Index.xml", base, false, false, false);
    encode(OPENDSUP_FIXTURES_DIR "/ncbdn/BDN_Index.xml", full, false, true, false);

    EXPECT_GE(fs::file_size(full), fs::file_size(base));
    auto stream = read::load_sup(full);
    ASSERT_FALSE(stream.display_sets.empty());
    for (auto& ds : stream.display_sets) {
        if (ds.pcs().pal_flag()) EXPECT_FALSE(ds.pds().empty());
    }
    const auto rep = read::validate(stream);
    EXPECT_TRUE(rep.compliant)
        << (rep.findings.empty() ? "" : rep.findings.front().detail);
}

TEST(EngineOptionsQa, BothFormatsWritesSupPesAndMui) {
    const std::string sup = OPENDSUP_TEST_OUTPUT_DIR "/qa_both.sup";
    const std::string pes = OPENDSUP_TEST_OUTPUT_DIR "/qa_both.pes";
    const std::string mui = OPENDSUP_TEST_OUTPUT_DIR "/qa_both.pes.mui";
    encode(OPENDSUP_FIXTURES_DIR "/synth_similar.xml", sup, false, false, true);

    EXPECT_TRUE(fs::exists(sup));
    EXPECT_TRUE(fs::exists(pes));
    EXPECT_TRUE(fs::exists(mui));
}

}  // namespace
}  // namespace core
}  // namespace opensup
