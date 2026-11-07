#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "opensup/core/filestreams.h"
#include "opensup/core/render/epoch_encoder.h"

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

// The forced flag must reach the wire as the PCS composition-object forced
// bit (0x80); the cropped bit (0x40) is a different flag.
TEST(BdnForced, WireFlagMatchesSpec)
{
    bdn_xml_c xml;
    ASSERT_TRUE(xml.parse(std::string(OPENDSUP_FIXTURES_DIR) + "/forced_flag.xml"));
    auto events = xml.events();
    ASSERT_EQ(events.size(), 2u);
    ASSERT_TRUE(events[0].forced());
    ASSERT_FALSE(events[1].forced());

    std::vector<window_definition_t> windows(1);
    windows[0].window_id = 0;
    windows[0].h_pos = 0;
    windows[0].v_pos = 0;
    windows[0].width = 600;
    windows[0].height = 120;

    epoch_render_options_t opts;
    epoch_encoder_c enc(opts);
    int palette_id_counter = 0;
    auto segs = enc.encode_epoch(events, std::vector<bool>(events.size(), true),
                                 common::fps_e::from_double(25.0),
                                 palette_id_counter, windows);

    std::vector<uint8_t> flags;
    for (const auto& seg : segs) {
        const auto* pcs = dynamic_cast<pcs_c*>(seg.get());
        if (!pcs || !pcs->cobjects.size()) continue;
        for (const auto& obj : pcs->cobjects)
            flags.push_back(obj.flags);
    }
    ASSERT_EQ(flags.size(), 2u);
    EXPECT_EQ(flags[0], c_object_t::forced);
    EXPECT_EQ(flags[1], c_object_t::standard);
}

} // namespace
} // namespace core
} // namespace opensup
