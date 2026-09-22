// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Normal case (Fase 2 / feature 003): with two layout windows and an
// overlapping event that takes over the other window, allow_normal_case
// redefines that window as NORMAL instead of restarting the whole
// composition (render2.py:513-544).
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/core/segments.h"

namespace opensup {
namespace core {
namespace {

size_t stream_hash(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    size_t h = 14695981039346656037ull;
    for (const auto& s : segs) {
        for (uint8_t b : s->to_bytes()) {
            h ^= b;
            h *= 1099511628211ull;
        }
    }
    return h;
}

/// State of the last display set that carries objects (the second event).
static std::optional<pcs_c::composition_state_e>
last_object_state(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    std::optional<pcs_c::composition_state_e> last;
    for (const auto& seg : segs) {
        auto pcs = std::dynamic_pointer_cast<pcs_c>(seg);
        if (pcs && !pcs->cobjects.empty())
            last = pcs->composition_state();
    }
    return last;
}

/// Last PCS (in stream order) that carries at least one CObject.
static std::shared_ptr<pcs_c>
last_object_pcs(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    std::shared_ptr<pcs_c> last;
    for (const auto& seg : segs) {
        auto pcs = std::dynamic_pointer_cast<pcs_c>(seg);
        if (pcs && !pcs->cobjects.empty())
            last = pcs;
    }
    return last;
}

TEST(NormalCase, OverlappingDifferentWindowBecomesNormalWithAllow) {
    encode_config_t base;
    base.input_path = OPENDSUP_FIXTURES_DIR "/opensup_normalcase.xml";
    base.output_path = OPENDSUP_TEST_OUTPUT_DIR "/normalcase.sup";
    base.overwrite = true;
    base.extra_acq = 0;

    bdn_render_c off(base);
    ASSERT_TRUE(off.execute().success);
    auto off_state = last_object_state(off.segments());
    ASSERT_TRUE(off_state.has_value());
    ASSERT_NE(*off_state, pcs_c::composition_state_e::normal)
        << "without allow_normal_case the second event must not be a normal case";

    encode_config_t on_cfg = base;
    on_cfg.allow_normal_case = true;
    on_cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/normalcase_on.sup";
    bdn_render_c on(on_cfg);
    ASSERT_TRUE(on.execute().success);
    auto on_state = last_object_state(on.segments());
    ASSERT_TRUE(on_state.has_value());

    // Over two windows with an overlapping event on the other window, the
    // second event redefines one window as NORMAL (not a full restart).
    EXPECT_EQ(*on_state, pcs_c::composition_state_e::normal);
}

TEST(NormalCase, RefreshKeepsOtherWindowObject) {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/opensup_normalcase.xml";
    cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/normalcase_refresh.sup";
    cfg.overwrite = true;
    cfg.extra_acq = 0;
    cfg.allow_normal_case = true;
    bdn_render_c on(cfg);
    ASSERT_TRUE(on.execute().success);

    // First DS carries only event A (window 0, A = (100,100)).
    std::shared_ptr<pcs_c> first;
    for (const auto& seg : on.segments()) {
        auto pcs = std::dynamic_pointer_cast<pcs_c>(seg);
        if (pcs && !pcs->cobjects.empty()) {
            first = pcs;
            break;
        }
    }
    ASSERT_TRUE(first != nullptr);
    ASSERT_EQ(first->cobjects.size(), 1u);
    ASSERT_EQ(first->cobjects[0].window_id, 0u);
    ASSERT_EQ(first->cobjects[0].h_pos, 100u);
    ASSERT_EQ(first->cobjects[0].v_pos, 100u);

    // Last DS is the NORMAL case for event B: it must carry TWO CObjects —
    // the new object on window 1 (B = (1000,400)) plus the reference to the
    // kept window-0 object (A, same id/position, no new ODS).
    auto last = last_object_pcs(on.segments());
    ASSERT_TRUE(last != nullptr);
    ASSERT_EQ(last->composition_state(), pcs_c::composition_state_e::normal);
    ASSERT_EQ(last->cobjects.size(), 2u);

    const c_object_t* ref = nullptr;
    const c_object_t* fresh = nullptr;
    for (const auto& o : last->cobjects) {
        if (o.window_id == 0u) ref = &o;
        if (o.window_id == 1u) fresh = &o;
    }
    ASSERT_TRUE(ref != nullptr && fresh != nullptr);
    EXPECT_EQ(ref->o_id, first->cobjects[0].o_id);   // same object as A's ODS
    EXPECT_EQ(ref->h_pos, 100u);
    EXPECT_EQ(ref->v_pos, 100u);
    EXPECT_EQ(fresh->h_pos, 1000u);
    EXPECT_EQ(fresh->v_pos, 400u);

    // R2 parity (render2.py:786-792, sorted f_is_first_cobj): the kept
    // window-0 reference CObject comes FIRST in the composition, also without
    // alternate_oids (default), not appended after the new object.
    EXPECT_EQ(last->cobjects[0].window_id, 0u);
    EXPECT_EQ(last->cobjects[0].o_id, ref->o_id);
    EXPECT_EQ(last->cobjects[1].window_id, 1u);

    // Same ref-first ordering with alternate_oids on (no-regression H3/009).
    encode_config_t alt_cfg = cfg;
    alt_cfg.alternate_oids = true;
    alt_cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/normalcase_refresh_alt.sup";
    bdn_render_c alt(alt_cfg);
    ASSERT_TRUE(alt.execute().success);
    auto alt_last = last_object_pcs(alt.segments());
    ASSERT_TRUE(alt_last != nullptr);
    ASSERT_EQ(alt_last->composition_state(), pcs_c::composition_state_e::normal);
    ASSERT_EQ(alt_last->cobjects.size(), 2u);
    EXPECT_EQ(alt_last->cobjects[0].window_id, 0u);
    EXPECT_EQ(alt_last->cobjects[0].o_id, first->cobjects[0].o_id);
}

TEST(NormalCase, RequiresTwoOverlappingWindows) {
    // The normal case requires two windows AND an overlap. Without either,
    // allow_normal_case must not alter the stream at all (byte-identical).

    // Two windows but events do NOT overlap.
    auto hash_with = [](const std::string& in, const std::string& out,
                        bool allow) {
        encode_config_t cfg;
        cfg.input_path = OPENDSUP_FIXTURES_DIR "/" + in;
        cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/" + out;
        cfg.overwrite = true;
        cfg.extra_acq = 0;
        cfg.allow_normal_case = allow;
        bdn_render_c r(cfg);
        EXPECT_TRUE(r.execute().success);
        return stream_hash(r.segments());
    };

    EXPECT_EQ(hash_with("opensup_multiwindow.xml", "normalcase_seq_on.sup", true),
              hash_with("opensup_multiwindow.xml", "normalcase_seq_off.sup", false));

    // Single window (same position).
    EXPECT_EQ(hash_with("synth_ssimband.xml", "normalcase_single_on.sup", true),
              hash_with("synth_ssimband.xml", "normalcase_single_off.sup", false));
}

TEST(NormalCase, OverlapRefreshKeepsOtherWindowObject) {
    // FR-3 diagnostic (010 R3): overlap + allow_normal_case + 2 windows. The
    // overlap pipeline (encode_epoch_overlap + emit_epoch_from_nodes) builds
    // epoch_node_t without the normal_case_ref fields, so the question is
    // whether the NORMAL display set still conserves the other window's object
    // (2nd CObject reference) exactly like the single-pass path does.
    auto encode = [](bool overlap, const std::string& out) {
        encode_config_t cfg;
        cfg.input_path = OPENDSUP_FIXTURES_DIR "/opensup_normalcase.xml";
        cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/" + out;
        cfg.overwrite = true;
        cfg.extra_acq = 0;
        cfg.allow_normal_case = true;
        cfg.overlap = overlap;
        bdn_render_c r(cfg);
        EXPECT_TRUE(r.execute().success);
        return r;
    };
    auto single = encode(false, "nc_ovl_single.sup");
    auto ovl = encode(true, "nc_ovl_overlap.sup");

    // Reference behaviour of the single-pass path: the NORMAL PCS (last one
    // carrying objects) keeps the window-0 reference as a 2nd CObject.
    auto last_obj = [](const bdn_render_c& r) {
        std::shared_ptr<pcs_c> last;
        for (const auto& seg : r.segments()) {
            auto pcs = std::dynamic_pointer_cast<pcs_c>(seg);
            if (pcs && !pcs->cobjects.empty()) last = pcs;
        }
        return last;
    };
    auto sp = last_obj(single);
    ASSERT_TRUE(sp != nullptr);
    ASSERT_EQ(sp->composition_state(), pcs_c::composition_state_e::normal);
    ASSERT_EQ(sp->cobjects.size(), 2u);   // window-0 kept ref + window-1 new

    // Diagnostic dump: every PCS with objects, both pipelines.
    auto dump_pcs = [](const bdn_render_c& r, const char* tag) {
        for (const auto& seg : r.segments()) {
            auto pcs = std::dynamic_pointer_cast<pcs_c>(seg);
            if (!pcs) continue;
            std::cout << "[diag][" << tag << "] state=0x"
                      << std::hex
                      << static_cast<int>(pcs->composition_state()) << std::dec
                      << " cobjects=" << pcs->cobjects.size();
            for (const auto& o : pcs->cobjects)
                std::cout << " w" << static_cast<int>(o.window_id)
                          << ":oid" << o.o_id;
            std::cout << "\n";
        }
    };
    dump_pcs(single, "single");
    dump_pcs(ovl, "overlap");

    // Parity expectation (010 R3): the overlap pipeline must conserve the
    // still-displayed window-0 object (2nd CObject reference) on the NORMAL
    // display set, exactly like the single-pass path. (Byte-level parity with
    // the non-overlap stream is NOT expected — ahead/overlap legitimately
    // changes clears/segments, see Ahead.OverlapChangesStreamButNotDefault.)
    auto ov = last_obj(ovl);
    ASSERT_TRUE(ov != nullptr);
    EXPECT_EQ(ov->composition_state(), pcs_c::composition_state_e::normal);
    ASSERT_EQ(ov->cobjects.size(), 2u);
    bool has_w0 = false, has_w1 = false;
    for (const auto& o : ov->cobjects) {
        if (o.window_id == 0u) has_w0 = true;
        if (o.window_id == 1u) has_w1 = true;
    }
    EXPECT_TRUE(has_w0) << "overlap path dropped the kept window-0 CObject";
    EXPECT_TRUE(has_w1);
}

}  // namespace
}  // namespace core
}  // namespace opensup