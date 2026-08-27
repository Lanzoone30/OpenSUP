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

}  // namespace
}  // namespace core
}  // namespace opensup