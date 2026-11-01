#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "opensup/core/filestreams.h"

namespace opensup {
namespace core {
namespace {

namespace fs = std::filesystem;

void
write_bdn(const std::string& path)
{
    std::ofstream out(path);
    out << R"(<?xml version="1.0" encoding="UTF-8"?>
<BDN Version="0.9">
  <Description>
    <Name Title="bdn-forced"/>
    <Language Code="eng"/>
    <Format VideoFormat="1080p" FrameRate="25" DropFrame="False"/>
    <Events Count="4"/>
  </Description>
  <Events>
    <Event InTC="00:00:01:00" OutTC="00:00:02:00" Forced="True">
      <Graphic Width="200" Height="60" X="0" Y="0">missing.png</Graphic>
    </Event>
    <Event InTC="00:00:02:00" OutTC="00:00:03:00" Forced="true">
      <Graphic Width="200" Height="60" X="0" Y="0">missing.png</Graphic>
    </Event>
    <Event InTC="00:00:03:00" OutTC="00:00:04:00" Forced="False">
      <Graphic Width="200" Height="60" X="0" Y="0">missing.png</Graphic>
    </Event>
    <Event InTC="00:00:04:00" OutTC="00:00:05:00">
      <Graphic Width="200" Height="60" X="0" Y="0">missing.png</Graphic>
    </Event>
  </Events>
</BDN>
)";
}

void
parse_forced()
{
    const std::string folder = std::string(OPENDSUP_TEST_OUTPUT_DIR) + "/bdn_forced";
    fs::create_directories(folder);
    const std::string path = folder + "/forced.xml";
    write_bdn(path);

    bdn_xml_c xml;
    ASSERT_TRUE(xml.parse(path));
    const auto& events = xml.events();
    ASSERT_EQ(events.size(), 4u);
    EXPECT_TRUE(events[0].forced());
    EXPECT_TRUE(events[1].forced());
    EXPECT_FALSE(events[2].forced());
    EXPECT_FALSE(events[3].forced());
}

TEST(BdnForced, StandardValue) { parse_forced(); }

} // namespace
} // namespace core
} // namespace opensup
