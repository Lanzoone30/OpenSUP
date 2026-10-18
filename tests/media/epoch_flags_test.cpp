// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Regression tests for the boolean-flag parity work: PDS full/delta,
// per-window object id alternation and anchor re-send in the overlap path.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "opensup/common/bdvideo.h"
#include "opensup/common/timecode.h"
#include "opensup/core/filestreams.h"
#include "opensup/core/render/epoch_encoder.h"
#include "opensup/core/segments.h"

namespace opensup {
namespace core {

/// Grants the tests access to the private helpers under test.
struct epoch_flags_test_access_t {
    static std::shared_ptr<pds_c> make_pds(epoch_encoder_c& enc,
                                           const media::palette_t& pal,
                                           uint8_t p_id, double pts, double dts) {
        return enc.make_pds(pal, p_id, pts, dts);
    }
    static void shift_forward(epoch_encoder_c& enc,
                              std::vector<epoch_node_t>& nodes,
                              const std::vector<bool>& forced_acq) {
        enc.shift_forward_overlay(nodes, forced_acq);
    }
};

namespace {

media::palette_t two_entry_palette(uint8_t y1, uint8_t y2)
{
    media::palette_t pal;
    pal.set(1, media::palette_entry_t(y1, 62, 100, 255));
    pal.set(2, media::palette_entry_t(y2, 198, 112, 255));
    return pal;
}

std::vector<uint16_t> ods_ids(const std::vector<std::shared_ptr<pg_segment_c>>& segs)
{
    std::vector<uint16_t> ids;
    for (const auto& seg : segs) {
        if (auto ods = std::dynamic_pointer_cast<ods_c>(seg))
            ids.push_back(ods->o_id());
    }
    return ids;
}

std::vector<bdn_xml_event_c> two_left_block_events()
{
    const common::fps_e fps = common::fps_e::from_double(25.0);
    std::vector<bdn_xml_event_c> events;
    events.emplace_back(common::tc_t::from_seconds(1.0, fps),
                        common::tc_t::from_seconds(3.0, fps),
                        660, 480, 600, 120,
                        std::string(OPENDSUP_FIXTURES_DIR) + "/flags_parity_swin.png");
    events.emplace_back(common::tc_t::from_seconds(3.0, fps),
                        common::tc_t::from_seconds(5.0, fps),
                        660, 480, 600, 120,
                        std::string(OPENDSUP_FIXTURES_DIR) + "/flags_parity_swin2.png");
    return events;
}

std::vector<window_definition_t> left_right_windows()
{
    std::vector<window_definition_t> wds;
    window_definition_t left;
    left.window_id = 0;
    left.h_pos = 670; left.v_pos = 500; left.width = 71; left.height = 81;
    window_definition_t right;
    right.window_id = 1;
    right.h_pos = 1180; right.v_pos = 500; right.width = 71; right.height = 81;
    wds.push_back(left);
    wds.push_back(right);
    return wds;
}

}  // namespace

TEST(EpochFlags, PdsFullWhenPaletteIdChanges)
{
    epoch_render_options_t opts;
    epoch_encoder_c enc(opts);
    auto pds = epoch_flags_test_access_t::make_pds(enc, two_entry_palette(170, 97),
                                                   0, 0.0, 0.0);
    ASSERT_NE(pds, nullptr);
    EXPECT_EQ(pds->to_palette().size(), 2u);   // new id -> full

    media::palette_t changed;
    changed.set(1, media::palette_entry_t(118, 106, 176, 255));
    auto delta = epoch_flags_test_access_t::make_pds(enc, changed, 0, 0.0, 0.0);
    ASSERT_NE(delta, nullptr);
    EXPECT_EQ(delta->to_palette().size(), 1u); // same id -> only changed entry

    auto full = epoch_flags_test_access_t::make_pds(enc, changed, 1, 0.0, 0.0);
    ASSERT_NE(full, nullptr);
    EXPECT_EQ(full->to_palette().size(), 1u);  // new id -> full palette
}

TEST(EpochFlags, PdsFullWhenFullPaletteModeActive)
{
    epoch_render_options_t opts;
    opts.full_palette = true;
    epoch_encoder_c enc(opts);
    auto first = epoch_flags_test_access_t::make_pds(enc, two_entry_palette(170, 97),
                                                     0, 0.0, 0.0);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->to_palette().size(), 2u);

    media::palette_t changed;
    changed.set(1, media::palette_entry_t(118, 106, 176, 255));
    auto second = epoch_flags_test_access_t::make_pds(enc, changed, 0, 0.0, 0.0);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->to_palette().size(), 1u); // full palette of the new frame
}

TEST(EpochFlags, ObjectIdIsPerWindowByDefault)
{
    epoch_render_options_t opts;
    epoch_encoder_c enc(opts);
    int palette_id_counter = 0;
    auto events = two_left_block_events();
    std::vector<bool> no_redraw(events.size(), false);
    auto segs = enc.encode_epoch(events, no_redraw, common::fps_e::from_double(25.0),
                                 palette_id_counter, left_right_windows());
    // window 0 toggles nwin(2) -> 0, then 2 (wid + buf[wid]).
    EXPECT_EQ(ods_ids(segs), (std::vector<uint16_t>{0, 2}));
}

TEST(EpochFlags, AlternateOidsKeepsLegacyGlobalToggle)
{
    epoch_render_options_t opts;
    opts.alternate_oids = true;
    epoch_encoder_c enc(opts);
    int palette_id_counter = 0;
    auto events = two_left_block_events();
    std::vector<bool> no_redraw(events.size(), false);
    auto segs = enc.encode_epoch(events, no_redraw, common::fps_e::from_double(25.0),
                                 palette_id_counter, left_right_windows());
    EXPECT_EQ(ods_ids(segs), (std::vector<uint16_t>{0, 1}));
}

TEST(EpochFlags, AnchorNodeKeepsItsObject)
{
    epoch_render_options_t opts;
    epoch_encoder_c enc(opts);
    std::vector<epoch_node_t> nodes(2);
    nodes[0].has_object = true;
    nodes[0].window_id = 0;
    nodes[1].has_object = true;
    nodes[1].new_mask = true;
    nodes[1].absolute = true;
    nodes[1].window_id = 1;
    nodes[1].obj_id = 7;
    std::vector<bool> forced = {false, true};
    epoch_flags_test_access_t::shift_forward(enc, nodes, forced);
    EXPECT_TRUE(nodes[1].absolute);
    EXPECT_TRUE(nodes[1].has_object);
    EXPECT_EQ(nodes[1].obj_id, 7);
}

TEST(MultiwindowOds, CrossWindow)
{
    bdn_xml_c xml;
    ASSERT_TRUE(xml.parse(std::string(OPENDSUP_FIXTURES_DIR) + "/opensup_multiwindow.xml"));

    // Both windows overlap the first event (100,100,200x60); y=131 splits it
    // into a top part (window 0) and a bottom part (window 1).
    std::vector<window_definition_t> windows(2);
    windows[0].window_id = 0;
    windows[0].h_pos = 90;
    windows[0].v_pos = 90;
    windows[0].width = 220;
    windows[0].height = 40;
    windows[1].window_id = 1;
    windows[1].h_pos = 90;
    windows[1].v_pos = 131;
    windows[1].width = 220;
    windows[1].height = 40;

    auto events = xml.events();
    std::vector<bool> redraw(events.size(), true);
    epoch_render_options_t opts;
    epoch_encoder_c enc(opts);
    int palette_id_counter = 0;
    auto segs = enc.encode_epoch(events, redraw, common::fps_e::from_double(25.0),
                                 palette_id_counter, windows);

    // The crossing event is split into one ODS per window, never the 200x60
    // global bbox.
    std::vector<std::shared_ptr<ods_c>> ods_list;
    for (const auto& seg : segs)
        if (auto ods = std::dynamic_pointer_cast<ods_c>(seg))
            ods_list.push_back(ods);
    EXPECT_EQ(ods_list.size(), 2u);
    for (const auto& ods : ods_list) {
        EXPECT_GT(ods->width(), 0);
        EXPECT_GT(ods->height(), 0);
        EXPECT_TRUE(ods->width() < 200 || ods->height() < 60);
    }

    // Each display set carries one cobject per window; per-window oid slots
    // alternate independently ({0,1} in window 0, {2,3} in window 1).
    uint16_t oid_w0 = 0;
    uint16_t oid_w1 = 0;
    for (const auto& seg : segs) {
        if (auto pcs = std::dynamic_pointer_cast<pcs_c>(seg)) {
            EXPECT_EQ(pcs->cobjects.size(), 2u);
            bool has_w0 = false;
            bool has_w1 = false;
            for (const auto& cobj : pcs->cobjects) {
                if (cobj.window_id == 0) { has_w0 = true; oid_w0 = cobj.o_id; }
                if (cobj.window_id == 1) { has_w1 = true; oid_w1 = cobj.o_id; }
            }
            EXPECT_TRUE(has_w0);
            EXPECT_TRUE(has_w1);
        }
    }
    EXPECT_NE(oid_w0, oid_w1);

    // The WDS re-declares both layout windows.
    bool saw_wds = false;
    for (const auto& seg : segs) {
        if (auto wds = std::dynamic_pointer_cast<wds_c>(seg)) {
            saw_wds = true;
            EXPECT_EQ(wds->windows.size(), 2u);
        }
    }
    EXPECT_TRUE(saw_wds);
}

TEST(MultiwindowOds, SingleWindowNoCross)
{
    bdn_xml_c xml;
    ASSERT_TRUE(xml.parse(std::string(OPENDSUP_FIXTURES_DIR) + "/opensup_multiwindow.xml"));

    // Window 0 crops the first event (100,100,200x60); window 1 is far away
    // and must stay empty.
    std::vector<window_definition_t> windows(2);
    windows[0].window_id = 0;
    windows[0].h_pos = 90;
    windows[0].v_pos = 90;
    windows[0].width = 220;
    windows[0].height = 40;
    windows[1].window_id = 1;
    windows[1].h_pos = 900;
    windows[1].v_pos = 500;
    windows[1].width = 220;
    windows[1].height = 40;

    auto events = xml.events();
    std::vector<bool> redraw(events.size(), true);
    redraw[1] = false;  // second event (1000,400) is not redrawn
    epoch_render_options_t opts;
    epoch_encoder_c enc(opts);
    int palette_id_counter = 0;
    auto segs = enc.encode_epoch(events, redraw, common::fps_e::from_double(25.0),
                                 palette_id_counter, windows);

    size_t ods_count = 0;
    size_t cobjects_in_w1 = 0;
    for (const auto& seg : segs) {
        if (std::dynamic_pointer_cast<ods_c>(seg))
            ++ods_count;
        if (auto pcs = std::dynamic_pointer_cast<pcs_c>(seg))
            for (const auto& cobj : pcs->cobjects)
                if (cobj.window_id == 1)
                    ++cobjects_in_w1;
    }
    EXPECT_EQ(ods_count, 1u);
    EXPECT_EQ(cobjects_in_w1, 0u);
}

TEST(MultiwindowOds, BufferGuardDegrades)
{
    bdn_xml_c xml;
    ASSERT_TRUE(xml.parse(std::string(OPENDSUP_FIXTURES_DIR) + "/opensup_multiwindow.xml"));

    // Two full-frame windows, area >= (1<<20)-150: the buffer guard must
    // degrade the layout to a single window.
    std::vector<window_definition_t> windows(2);
    for (size_t i = 0; i < windows.size(); ++i) {
        windows[i].window_id = static_cast<uint8_t>(i);
        windows[i].h_pos = 0;
        windows[i].v_pos = 0;
        windows[i].width = 1919;
        windows[i].height = 1079;
    }

    auto events = xml.events();
    std::vector<bool> redraw(events.size(), true);
    epoch_render_options_t opts;
    epoch_encoder_c enc(opts);
    int palette_id_counter = 0;
    auto segs = enc.encode_epoch(events, redraw, common::fps_e::from_double(25.0),
                                 palette_id_counter, windows);

    std::shared_ptr<wds_c> first_wds;
    for (const auto& seg : segs) {
        if (auto wds = std::dynamic_pointer_cast<wds_c>(seg)) {
            first_wds = wds;
            break;
        }
    }
    EXPECT_TRUE(first_wds != nullptr);
    if (first_wds)
        EXPECT_EQ(first_wds->windows.size(), 1u);
}

}  // namespace core
}  // namespace opensup
