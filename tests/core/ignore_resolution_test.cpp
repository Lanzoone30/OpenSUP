// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// 041: non-standard BDN resolutions are rejected unless --ignore-resolution
// is set; with the flag the encode proceeds at the source resolution.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "opensup/common/logger.h"
#include "opensup/core/filestreams.h"
#include "opensup/core/interface.h"
#include "opensup/read/inspect.h"
#include "opensup/read/sup_reader.h"

namespace opensup {
namespace core {
namespace {

namespace fs = std::filesystem;

std::string
enigma_bdn()
{
    return std::string(OPENDSUP_FIXTURES_DIR) + "/../../temp/ENIGMA/BDN_Index.xml";
}

TEST(IgnoreResolution, RejectByDefault) {
    const std::string bdn = enigma_bdn();
    if (!fs::exists(bdn)) GTEST_SKIP() << "BDN local no presente: " << bdn;

    auto& log = common::logger_c::instance();
    std::vector<std::string> errors;
    log.set_callback([&](const std::string& msg, int level) {
        if (level == static_cast<int>(common::log_level_e::error)) errors.push_back(msg);
    });
    bdn_xml_c xml;
    const bool parsed = xml.parse(bdn);
    log.set_callback(nullptr);

    EXPECT_FALSE(parsed);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0], "Non-standard Blu-ray resolution 1350x1080.");
}

TEST(IgnoreResolution, AcceptWithFlag) {
    const std::string bdn = enigma_bdn();
    if (!fs::exists(bdn)) GTEST_SKIP() << "BDN local no presente: " << bdn;

    bdn_xml_c xml;
    ASSERT_TRUE(xml.parse(bdn, true));
    EXPECT_EQ(xml.width(), 1350);
    EXPECT_EQ(xml.height(), 1080);
    EXPECT_FALSE(xml.events().empty());

    fs::create_directories(OPENDSUP_TEST_OUTPUT_DIR);
    encode_config_t cfg;
    cfg.input_path = bdn;
    cfg.output_path = std::string(OPENDSUP_TEST_OUTPUT_DIR) + "/ignore_resolution.sup";
    cfg.overwrite = true;
    cfg.ignore_resolution = true;
    bdn_render_c render(cfg);
    const auto res = render.execute();
    ASSERT_TRUE(res.success) << res.error;
    EXPECT_GT(res.segments, 0);

    auto stream = read::load_sup(cfg.output_path);
    const auto rep = read::validate(stream);
    EXPECT_TRUE(rep.compliant)
        << (rep.findings.empty() ? "" : rep.findings.front().detail);
}

TEST(IgnoreResolution, RejectsNonPositiveDimensionsEvenWithFlag) {
    fs::create_directories(OPENDSUP_TEST_OUTPUT_DIR);
    const std::string xml_path =
        std::string(OPENDSUP_TEST_OUTPUT_DIR) + "/ignore_resolution_bad.xml";
    std::ofstream(xml_path)
        << "<BDN><Description><Format VideoFormat=\"0x1080\" FrameRate=\"25\" "
           "DropFrame=\"false\"/></Description><Events/></BDN>";

    bdn_xml_c xml;
    EXPECT_FALSE(xml.parse(xml_path, true));
}

}  // namespace
}  // namespace core
}  // namespace opensup
