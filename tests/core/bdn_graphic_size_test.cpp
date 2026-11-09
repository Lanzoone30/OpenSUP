// Copyright (C) 2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// The single-<Graphic> path indexes the loaded image with the XML-declared
// box, so parse must reject a PNG whose real size differs from it.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "opensup/common/logger.h"
#include "opensup/core/filestreams.h"

namespace opensup {
namespace core {
namespace {

namespace fs = std::filesystem;

std::string
write_bdn(const std::string& folder, const std::string& graphics)
{
    fs::create_directories(folder);
    const std::string path = folder + "/graphic_size.xml";
    std::ofstream out(path);
    out << R"(<?xml version="1.0" encoding="UTF-8"?>
<BDN Version="0.9">
  <Description>
    <Name Title="graphic-size"/>
    <Language Code="eng"/>
    <Format VideoFormat="1080p" FrameRate="25" DropFrame="False"/>
    <Events Count="1"/>
  </Description>
  <Events>
    <Event InTC="00:00:01:00" OutTC="00:00:03:00" Forced="False">
)" << graphics << R"(    </Event>
  </Events>
</BDN>
)";
    return path;
}

std::vector<std::string>
parse_errors(bdn_xml_c& xml, const std::string& path, bool& parsed)
{
    auto& log = common::logger_c::instance();
    std::vector<std::string> errors;
    log.set_callback([&](const std::string& msg, int level) {
        if (level == static_cast<int>(common::log_level_e::error))
            errors.push_back(msg);
    });
    parsed = xml.parse(path);
    log.set_callback(nullptr);
    return errors;
}

TEST(BdnGraphicSize, DeclaredSizeMismatchFailsParse)
{
    const std::string folder =
        std::string(OPENDSUP_TEST_OUTPUT_DIR) + "/bdn_graphic_size";
    fs::create_directories(folder);
    const std::string image = folder + "/lie.png";
    fs::copy_file(std::string(OPENDSUP_FIXTURES_DIR) + "/00001.png", image,
                  fs::copy_options::overwrite_existing);
    const std::string xml_path = write_bdn(
        folder,
        R"(      <Graphic Width="1200" Height="240" X="0" Y="0">lie.png</Graphic>
)");

    bdn_xml_c xml;
    bool parsed = true;
    const auto errors = parse_errors(xml, xml_path, parsed);

    EXPECT_FALSE(parsed);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0], "Event 00:00:01:00: graphic " + image +
                             " is 600x120, XML declares 1200x240");
}

TEST(BdnGraphicSize, CorruptHeaderFailsParse)
{
    const std::string folder =
        std::string(OPENDSUP_TEST_OUTPUT_DIR) + "/bdn_graphic_size";
    fs::create_directories(folder);
    const std::string image = folder + "/corrupt.png";
    {
        std::ofstream out(image, std::ios::binary);
        out << "not a png";
    }
    const std::string xml_path = write_bdn(
        folder,
        R"(      <Graphic Width="64" Height="64" X="0" Y="0">corrupt.png</Graphic>
)");

    bdn_xml_c xml;
    bool parsed = true;
    const auto errors = parse_errors(xml, xml_path, parsed);

    EXPECT_FALSE(parsed);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0], "Failed to read image header: " + image);
}

TEST(BdnGraphicSize, MultiGraphicCanvasPastesWithoutCrash)
{
    const std::string folder =
        std::string(OPENDSUP_TEST_OUTPUT_DIR) + "/bdn_graphic_size";
    fs::create_directories(folder);
    const std::string source = std::string(OPENDSUP_FIXTURES_DIR) + "/00001.png";
    fs::copy_file(source, folder + "/left.png",
                  fs::copy_options::overwrite_existing);
    fs::copy_file(source, folder + "/right.png",
                  fs::copy_options::overwrite_existing);
    const std::string xml_path = write_bdn(
        folder,
        R"(      <Graphic Width="600" Height="120" X="0" Y="0">left.png</Graphic>
      <Graphic Width="600" Height="120" X="600" Y="0">right.png</Graphic>
)");

    bdn_xml_c xml;
    bool parsed = false;
    const auto errors = parse_errors(xml, xml_path, parsed);

    ASSERT_TRUE(parsed) << (errors.empty() ? "" : errors[0]);
    const auto& events = xml.events();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].width(), 1200);
    EXPECT_EQ(events[0].height(), 120);
    EXPECT_EQ(events[0].load_image().size(), 1200u * 120u * 4u);
}

} // namespace
} // namespace core
} // namespace opensup
