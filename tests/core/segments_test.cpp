#include <gtest/gtest.h>
#include "opensup/core/segments.h"

namespace opensup {
namespace core {
namespace {

TEST(Segments, BigEndianHelpers) {
    uint8_t buf[4] = {};
    write_u16_be(buf, 0x1234);
    EXPECT_EQ(read_u16_be(buf), 0x1234);
    EXPECT_EQ(buf[0], 0x12);
    EXPECT_EQ(buf[1], 0x34);

    write_u24_be(buf, 0x123456);
    EXPECT_EQ(read_u24_be(buf), 0x123456);

    write_u32_be(buf, 0x12345678);
    EXPECT_EQ(read_u32_be(buf), 0x12345678);
}

TEST(Segments, PGSegmentCreate) {
    auto hdr = pg_segment_c::make_header(segment_type_e::pcs);
    EXPECT_EQ(hdr.size(), PGS_HEADER_LEN);
    EXPECT_EQ(hdr[0], 'P');
    EXPECT_EQ(hdr[1], 'G');
    EXPECT_EQ(hdr[10], 0x16);
}

TEST(Segments, PGSegmentPts) {
    std::vector<uint8_t> data = { 'P', 'G', 0,0,0,0, 0,0,0,0, 0x16, 0,0 };
    pg_segment_c seg(data);
    seg.set_pts(1.0);
    EXPECT_NEAR(seg.pts(), 1.0, 0.001);
}

TEST(Segments, WindowDefinition) {
    window_definition_t wd;
    wd.window_id = 1;
    wd.h_pos = 100;
    wd.v_pos = 200;
    wd.width = 720;
    wd.height = 576;

    auto bytes = wd.to_bytes();
    EXPECT_EQ(bytes.size(), 9);

    auto wd2 = window_definition_t::from_bytes(bytes.data());
    EXPECT_EQ(wd2.window_id, 1);
    EXPECT_EQ(wd2.h_pos, 100);
    EXPECT_EQ(wd2.v_pos, 200);
    EXPECT_EQ(wd2.width, 720);
    EXPECT_EQ(wd2.height, 576);
}

TEST(Segments, CObjectStandard) {
    c_object_t obj;
    obj.o_id = 42;
    obj.window_id = 1;
    obj.h_pos = 50;
    obj.v_pos = 60;

    auto bytes = obj.to_bytes();
    EXPECT_EQ(bytes.size(), 8);

    auto obj2 = c_object_t::from_bytes(bytes.data(), false);
    EXPECT_EQ(obj2.o_id, 42);
    EXPECT_EQ(obj2.window_id, 1);
    EXPECT_EQ(obj2.h_pos, 50);
    EXPECT_EQ(obj2.v_pos, 60);
    EXPECT_FALSE(obj2.is_cropped());
    EXPECT_FALSE(obj2.is_forced());
}

TEST(Segments, CObjectCropped) {
    c_object_t obj;
    obj.o_id = 1;
    obj.window_id = 0;
    obj.flags = c_object_t::cropped;
    obj.h_pos = 0;
    obj.v_pos = 0;
    obj.hc_pos = 10;
    obj.vc_pos = 20;
    obj.c_w = 100;
    obj.c_h = 200;

    auto bytes = obj.to_bytes();
    EXPECT_EQ(bytes.size(), 16);

    auto obj2 = c_object_t::from_bytes(bytes.data(), true);
    EXPECT_TRUE(obj2.is_cropped());
    EXPECT_EQ(obj2.hc_pos, 10);
    EXPECT_EQ(obj2.vc_pos, 20);
    EXPECT_EQ(obj2.c_w, 100);
    EXPECT_EQ(obj2.c_h, 200);
}

TEST(Segments, PCSFromScratch) {
    std::vector<c_object_t> cobs;
    c_object_t obj;
    obj.o_id = 1;
    obj.window_id = 0;
    obj.h_pos = 0;
    obj.v_pos = 0;
    cobs.push_back(obj);

    auto pcs = pcs_c::from_scratch(1920, 1080, 0x10, 1,
                                     pcs_c::composition_state_e::epoch_start,
                                     false, 0, cobs, 1.0, 1.0);
    EXPECT_EQ(pcs.video_width(), 1920);
    EXPECT_EQ(pcs.video_height(), 1080);
    EXPECT_EQ(pcs.composition_n(), 1);
    EXPECT_EQ(pcs.composition_state(), pcs_c::composition_state_e::epoch_start);
    EXPECT_EQ(pcs.n_objects(), 1);
    EXPECT_NEAR(pcs.pts(), 1.0, 0.001);
}

TEST(Segments, WDSFromScratch) {
    std::vector<window_definition_t> windows;
    window_definition_t wd;
    wd.window_id = 0;
    wd.h_pos = 0;
    wd.v_pos = 0;
    wd.width = 1920;
    wd.height = 1080;
    windows.push_back(wd);

    auto wds = wds_c::from_scratch(windows, 1.0, 1.0);
    EXPECT_EQ(wds.n_windows(), 1);
    EXPECT_EQ(wds.windows[0].width, 1920);
    EXPECT_EQ(wds.windows[0].height, 1080);
}

TEST(Segments, PDSFromScratch) {
    media::palette_t pal;
    pal.set(0, media::palette_entry_t(16, 128, 128, 255));
    pal.set(1, media::palette_entry_t(100, 200, 50, 255));

    auto pds = pds_c::from_scratch(pal, 1, 0, 1.0, 1.0);
    EXPECT_EQ(pds.p_id(), 0);
    EXPECT_EQ(pds.p_vn(), 1);

    auto pal2 = pds.to_palette();
    EXPECT_EQ(pal2.size(), 2);
    EXPECT_TRUE(pal2.get(0).has_value());
    EXPECT_EQ(pal2.get(0)->y, 16);
}

TEST(Segments, ODSFromScratch) {
    std::vector<uint8_t> rle_data = {1, 2, 3, 4, 5};
    auto ods_list = ods_c::from_scratch(1, 0, 100, 50, rle_data, 1.0, 1.0);
    EXPECT_EQ(ods_list.size(), 1);
    EXPECT_EQ(ods_list[0].o_id(), 1);
    EXPECT_EQ(ods_list[0].width(), 100);
    EXPECT_EQ(ods_list[0].height(), 50);
}

TEST(Segments, ODSMultiSeq) {
    std::vector<uint8_t> big_data(0x10000, 0x42); // > MAX_FIRST (0xFFE4)
    auto ods_list = ods_c::from_scratch(1, 0, 640, 480, big_data, 1.0, 1.0);
    EXPECT_GT(ods_list.size(), 1);
    EXPECT_EQ(ods_list[0].seq_flags(), ods_c::sequence_flags_e::first);
    EXPECT_EQ(ods_list.back().seq_flags(), ods_c::sequence_flags_e::last);
}

TEST(Segments, ENDSFromScratch) {
    auto end = ends_c::from_scratch(1.0, 1.0);
    EXPECT_EQ(end.type(), segment_type_e::end);
    EXPECT_EQ(end.payload().size(), 0);
}

TEST(Segments, DisplaySetFromBytes) {
    // Build a minimal display set: PCS + WDS + PDS + ODS + ENDS
    std::vector<uint8_t> all_bytes;

    auto pcs = pcs_c::from_scratch(1920, 1080, 0x10, 1,
                                     pcs_c::composition_state_e::epoch_start,
                                     false, 0, {}, 1.0, 0.9);
    auto pcs_bytes = pcs.to_bytes();
    all_bytes.insert(all_bytes.end(), pcs_bytes.begin(), pcs_bytes.end());

    std::vector<window_definition_t> windows;
    window_definition_t wd;
    wd.window_id = 0;
    wd.h_pos = 0;
    wd.v_pos = 0;
    wd.width = 1920;
    wd.height = 1080;
    windows.push_back(wd);

    auto wds = wds_c::from_scratch(windows, 1.0, 0.9);
    auto wds_bytes = wds.to_bytes();
    all_bytes.insert(all_bytes.end(), wds_bytes.begin(), wds_bytes.end());

    auto end = ends_c::from_scratch(1.0, 0.9);
    auto end_bytes = end.to_bytes();
    all_bytes.insert(all_bytes.end(), end_bytes.begin(), end_bytes.end());

    auto ds = display_set_t::from_bytes(all_bytes);
    EXPECT_EQ(ds.segments.size(), 3);
    EXPECT_EQ(ds.segments[0]->type(), segment_type_e::pcs);
    EXPECT_EQ(ds.segments[1]->type(), segment_type_e::wds);
    EXPECT_EQ(ds.segments[2]->type(), segment_type_e::end);
}

} // namespace
} // namespace core
} // namespace opensup
