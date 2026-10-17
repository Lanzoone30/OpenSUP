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

}  // namespace core
}  // namespace opensup
