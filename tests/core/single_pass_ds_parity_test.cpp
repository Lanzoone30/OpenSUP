// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Feature 028: a single-pass epoch that crosses a layout-window boundary must
// emit exactly one display set per event. A 200x60 object at (100,100) is cut
// by the y=131 gap between two layout windows, so both windows run an emit
// for the same event. The merge keeps the first PCS but lets the second
// window's trailing segments through, leaving a PCS-less display set that
// carries the window decode DTS instead of the event's tc_in.
#include <gtest/gtest.h>

#include <cstdint>
#include <cmath>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "opensup/common/bdvideo.h"
#include "opensup/common/timecode.h"
#include "opensup/core/filestreams.h"
#include "opensup/core/interface.h"
#include "opensup/core/render/epoch_encoder.h"
#include "opensup/core/segments.h"

namespace opensup {
namespace core {
namespace {

// One event (100,100,200x60) straddling the two windows, plus the fixture's
// unrelated second event. First event InTC = 00:00:01:00 @ 25 fps.
std::vector<std::shared_ptr<pg_segment_c>> encode_crossing() {
    bdn_xml_c xml;
    EXPECT_TRUE(xml.parse(std::string(OPENDSUP_FIXTURES_DIR) +
                          "/opensup_multiwindow.xml"));

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
    return enc.encode_epoch(events, redraw, common::fps_e::from_double(25.0),
                            palette_id_counter, windows);
}

// The stream contract: each display set opens at a PCS and closes at the next
// ENDS. Returns the sets in order; a trailing partial set is dropped.
std::vector<std::vector<std::shared_ptr<pg_segment_c>>>
display_sets(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    std::vector<std::vector<std::shared_ptr<pg_segment_c>>> sets;
    std::vector<std::shared_ptr<pg_segment_c>> cur;
    for (const auto& seg : segs) {
        cur.push_back(seg);
        if (dynamic_cast<ends_c*>(seg.get())) {
            sets.push_back(std::move(cur));
            cur.clear();
        }
    }
    return sets;
}

bool has_ods(const std::vector<std::shared_ptr<pg_segment_c>>& ds) {
    for (const auto& seg : ds)
        if (dynamic_cast<ods_c*>(seg.get())) return true;
    return false;
}

TEST(SinglePassDsParity, StructuralInvariantsAcrossWindowBoundary) {
    const auto segs = encode_crossing();

    int pcs = 0, ends = 0;
    for (const auto& seg : segs) {
        if (dynamic_cast<pcs_c*>(seg.get())) ++pcs;
        if (dynamic_cast<ends_c*>(seg.get())) ++ends;
    }
    EXPECT_EQ(pcs, ends) << "PCS and ENDS must pair one-to-one";

    int sets_without_pcs = 0;
    bool open = false;
    for (const auto& seg : segs) {
        if (dynamic_cast<pcs_c*>(seg.get())) {
            open = true;
            continue;
        }
        if (dynamic_cast<ends_c*>(seg.get())) {
            if (!open) ++sets_without_pcs;
            open = false;
            continue;
        }
        EXPECT_TRUE(open) << "segment between ENDS and the next PCS";
    }
    EXPECT_EQ(sets_without_pcs, 0) << "display set without PCS";
}

TEST(SinglePassDsParity, OneContentDisplaySetPerEvent) {
    int content_sets = 0;
    for (const auto& ds : display_sets(encode_crossing()))
        if (has_ods(ds)) ++content_sets;
    EXPECT_EQ(content_sets, 1)
        << "one crossing event must yield one content display set";
}

TEST(SinglePassDsParity, ContentDisplaySetPresentsAtTcIn) {
    const double tc_in = 1.0;  // 00:00:01:00 @ 25 fps
    for (const auto& ds : display_sets(encode_crossing())) {
        if (!has_ods(ds)) continue;
        EXPECT_NEAR(ds.front()->pts(), tc_in, 1e-6)
            << "content display set must present at tc_in, not the window DTS";
    }
}

// --- T020: end-to-end BDN parity against the SUPer oracle (counts, not byte
// parity: the oracle was rendered at 1920x1080 vs the BDN's 1350x1080). ---

struct sup_stats_t {
    int segments = 0;
    int pcs = 0, ends = 0, content = 0, clear = 0, without_pcs = 0;
    int content_ods = 0, acq_state = 0;
    std::vector<uint32_t> content_ods_pts;
};

// File-level segment reader. PCS fields sit at payload+7 (state) and
// payload+10 (object count). The in-memory display_sets() helper above cannot
// read the oracle's .sup bytes.
sup_stats_t scan_sup(const std::string& path) {
    sup_stats_t r;
    std::ifstream f(path, std::ios::binary);
    if (!f) return r;
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    size_t i = 0;
    bool open = false, ods_in_ds = false;
    uint32_t ds_pts = 0;
    while (i + 13 <= d.size() && d[i] == 'P' && d[i + 1] == 'G') {
        const uint32_t pts = (uint32_t(d[i + 2]) << 24) | (uint32_t(d[i + 3]) << 16) |
                             (uint32_t(d[i + 4]) << 8) | d[i + 5];
        const uint8_t type = d[i + 10];
        const uint16_t len = static_cast<uint16_t>((d[i + 11] << 8) | d[i + 12]);
        ++r.segments;
        if (type == 0x16) {
            const uint8_t state = d[i + 13 + 7];
            const uint8_t objs = d[i + 13 + 10];
            ++r.pcs;
            open = true;
            ods_in_ds = false;
            if (objs > 0) ++r.content;
            else ++r.clear;
            if (state == 0x40) ++r.acq_state;
            ds_pts = pts;
        } else if (type == 0x15) {
            ods_in_ds = true;
        } else if (type == 0x80) {
            ++r.ends;
            if (!open) ++r.without_pcs;
            if (open && ods_in_ds) {
                ++r.content_ods;
                r.content_ods_pts.push_back(ds_pts);
            }
            open = false;
        }
        i += 13u + len;
    }
    return r;
}

TEST(SinglePassDsParity, BdnSinglePassMatchesOracleOff) {
    const std::string repo = std::string(OPENDSUP_FIXTURES_DIR) + "/../..";
    const std::string bdn = repo + "/temp/ENIGMA/BDN_Index.xml";
    const std::string oracle = repo + "/oracle_sup_phase28/oracle_buffering_off.sup";
    if (!std::ifstream(bdn).good())
        GTEST_SKIP() << "BDN local no presente: " << bdn;
    if (!std::ifstream(oracle).good())
        GTEST_SKIP() << "oraculo local no presente: " << oracle;

    // Buffering OFF = single-pass: no --overlap, no --allow-normal (defaults
    // compression 80, acqrate 100, extra-acq 2), como temp/phase28_cli.
    encode_config_t cfg;
    cfg.input_path = bdn;
    cfg.output_path = std::string(OPENDSUP_TEST_OUTPUT_DIR) + "/bdn_single_pass.sup";
    cfg.overwrite = true;
    cfg.ignore_resolution = true;  // BDN 1350x1080
    bdn_render_c render(cfg);
    const auto res = render.execute();
    ASSERT_TRUE(res.success) << res.error;

    const sup_stats_t motor = scan_sup(cfg.output_path);
    const sup_stats_t orac = scan_sup(oracle);

    EXPECT_EQ(motor.segments, res.segments);
    EXPECT_EQ(motor.pcs, motor.ends) << "PCS and ENDS must pair one-to-one";
    EXPECT_EQ(motor.without_pcs, 0) << "no display set without PCS";
    EXPECT_EQ(orac.pcs, orac.ends);
    EXPECT_EQ(orac.without_pcs, 0);

    // One content display set (with ODS) per event, presented at its InTC.
    bdn_xml_c xml;
    ASSERT_TRUE(xml.parse(bdn, true /* ignore non-standard resolution */));
    std::set<long long> intc;
    for (const auto& e : xml.events())
        intc.insert(std::llround(e.tc_in() * 90000.0));
    EXPECT_EQ(motor.content_ods, static_cast<int>(xml.events().size()))
        << "one content display set per event";
    int at_in = 0;
    for (uint32_t p : motor.content_ods_pts)
        if (intc.count(p)) ++at_in;
    EXPECT_EQ(at_in, motor.content_ods) << "content groups must present at tc_in";

    // Full count parity with the oracle (resolutions differ, counts must not):
    // one content epoch per event (no palette-end wipe leftovers) and a
    // matching acquisition count.
    EXPECT_EQ(motor.content, static_cast<int>(xml.events().size()))
        << "no extra content epochs (multi-window wipes must be clears)";
    EXPECT_EQ(motor.content, orac.content);
    EXPECT_EQ(motor.acq_state, orac.acq_state);
}

}  // namespace
}  // namespace core
}  // namespace opensup