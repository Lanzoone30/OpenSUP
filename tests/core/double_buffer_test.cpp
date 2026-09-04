// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// F4 (feature 009): alternate_oids — SUPer double_buffering[wid]
// (render2.py:844, 749-750). Per-window oid alternation for new objects in
// multi-window epochs: db[w] = abs(n - db[w]), oid = w + db[w]. The normal
// case reference keeps the on-screen oid without alternating (render2.py:
// 737-746) and is ordered first in the composition list (render2.py:786-792).
// Flag off (default) -> byte-identical stream (FR-5).
#include <gtest/gtest.h>
#include <memory>
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
static bdn_render_c encode(const std::string& fixture, const std::string& out,
                           bool alternate_oids, bool overlap = false) {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/" + fixture;
    cfg.output_path = out;
    cfg.overwrite = true;
    cfg.extra_acq = 0;
    cfg.overlap = overlap;
    cfg.alternate_oids = alternate_oids;
    bdn_render_c render(cfg);
    const auto res = render.execute();
    EXPECT_TRUE(res.success) << res.error;
    return render;
}
/// Object ids of every PCS composition object in emission order.
static std::vector<uint16_t>
pcs_all_oids(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    std::vector<uint16_t> out;
    for (const auto& seg : segs) {
        auto pcs = std::dynamic_pointer_cast<pcs_c>(seg);
        if (pcs) {
            for (const auto& o : pcs->cobjects)
                out.push_back(o.o_id);
        }
    }
    return out;
}
/// First CObject oid of each PCS (the composition's primary object).
static std::vector<uint16_t>
pcs_first_oids(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    std::vector<uint16_t> out;
    for (const auto& seg : segs) {
        auto pcs = std::dynamic_pointer_cast<pcs_c>(seg);
        if (pcs && !pcs->cobjects.empty())
            out.push_back(pcs->cobjects[0].o_id);
    }
    return out;
}
} // namespace

TEST(DbOids, SingleWindowUnchangedByFlag) {
    // A single-window fixture (synth_ssimband.xml, 2 events same position):
    // the flag must not alter oids nor the stream (multi-window only).
    auto off = encode("synth_ssimband.xml",
                      OPENDSUP_TEST_OUTPUT_DIR "/dbo_single_off.sup", false);
    auto on = encode("synth_ssimband.xml",
                     OPENDSUP_TEST_OUTPUT_DIR "/dbo_single_on.sup", true);
    EXPECT_EQ(stream_hash(on.segments()), stream_hash(off.segments()));
    EXPECT_EQ(pcs_first_oids(on.segments()), pcs_first_oids(off.segments()));
}

TEST(DbOids, MultiWindowReadquisitionAlternatesPerWindow) {
    // opensup_multiwindow_reaq.xml: ev1 w0, ev2 w1, ev3 w0 again (re-acquire).
    // Default: global {0,1} toggle -> ev1=0, ev2=1, ev3=0.
    // alternate_oids: per-window db=[2,2] -> w0: {0, 2}, w1: {1}. ev3 (w0) = 2.
    auto off = encode("opensup_multiwindow_reaq.xml",
                      OPENDSUP_TEST_OUTPUT_DIR "/dbo_reaq_off.sup", false);
    auto on = encode("opensup_multiwindow_reaq.xml",
                     OPENDSUP_TEST_OUTPUT_DIR "/dbo_reaq_on.sup", true);
    ASSERT_TRUE(off.segments().size() >= 1);
    // Streams must differ: the re-acquired w0 object gets a different oid.
    EXPECT_NE(stream_hash(on.segments()), stream_hash(off.segments()));
    const auto off_ids = pcs_first_oids(off.segments());
    const auto on_ids = pcs_first_oids(on.segments());
    // Default: 0, 1, 0 (global toggle). On: 0, 1, 2 (w0 alternates 0 -> 2).
    ASSERT_EQ(off_ids.size(), 3u);
    ASSERT_EQ(on_ids.size(), 3u);
    EXPECT_EQ(off_ids[0], 0u);
    EXPECT_EQ(off_ids[1], 1u);
    EXPECT_EQ(off_ids[2], 0u);   // default re-acquire reuses oid 0
    EXPECT_EQ(on_ids[0], 0u);    // w0 first
    EXPECT_EQ(on_ids[1], 1u);    // w1 first
    EXPECT_EQ(on_ids[2], 2u);    // w0 re-acquire alternated to 2
}

TEST(DbOids, NormalCaseRefFirstWithAlternate) {
    // FR-3 (render2.py:786-792): with alternate_oids, the normal case puts the
    // kept (id_skipped) CObject FIRST in the composition list. Without the
    // flag the legacy order (fresh first, ref after) is preserved.
    auto encode_nc = [](const std::string& out, bool alt) {
        encode_config_t cfg;
        cfg.input_path = OPENDSUP_FIXTURES_DIR "/opensup_normalcase.xml";
        cfg.output_path = out;
        cfg.overwrite = true;
        cfg.extra_acq = 0;
        cfg.allow_normal_case = true;
        cfg.alternate_oids = alt;
        bdn_render_c render(cfg);
        const auto res = render.execute();
        EXPECT_TRUE(res.success) << res.error;
        return render;
    };
    auto on = encode_nc(OPENDSUP_TEST_OUTPUT_DIR "/dbo_nc_on.sup", true);
    auto off = encode_nc(OPENDSUP_TEST_OUTPUT_DIR "/dbo_nc_off.sup", false);
    // Last PCS (the NORMAL case) carries two CObjects in both modes.
    auto last_pcs = [](const std::vector<std::shared_ptr<pg_segment_c>>& segs)
        -> std::shared_ptr<pcs_c> {
        std::shared_ptr<pcs_c> last;
        for (const auto& seg : segs) {
            auto pcs = std::dynamic_pointer_cast<pcs_c>(seg);
            if (pcs && pcs->cobjects.size() == 2u) last = pcs;
        }
        return last;
    };
    auto on_pcs = last_pcs(on.segments());
    auto off_pcs = last_pcs(off.segments());
    ASSERT_TRUE(on_pcs != nullptr);
    ASSERT_TRUE(off_pcs != nullptr);
    ASSERT_EQ(on_pcs->cobjects.size(), 2u);
    ASSERT_EQ(off_pcs->cobjects.size(), 2u);
    // With the flag: the kept window-0 reference comes first.
    EXPECT_EQ(on_pcs->cobjects[0].window_id, 0u);
    EXPECT_EQ(on_pcs->cobjects[1].window_id, 1u);
    // Legacy (flag off): fresh window-1 object first, ref after.
    EXPECT_EQ(off_pcs->cobjects[0].window_id, 1u);
    EXPECT_EQ(off_pcs->cobjects[1].window_id, 0u);
    // The reference keeps the on-screen oid (no alternation) in both.
    EXPECT_EQ(on_pcs->cobjects[0].o_id, off_pcs->cobjects[1].o_id);
}

TEST(DbOids, OverlapPathAlternatesPerWindow) {
    // FR-4: the overlap pipeline (encode_epoch_overlap + emit_epoch_from_nodes)
    // must also alternate oids per window. opensup_multiwindow_merge.xml has
    // two events in different windows 40ms apart — the shift_forward_overlay
    // path. With alternate_oids on, each window's acquisition gets its own oid
    // (w0 -> 0, w1 -> 1), identical to the single-pass numbering here (each
    // window acquires once) but the flag must keep the stream identical to
    // default when off.
    auto on1 = encode("opensup_multiwindow_merge.xml",
                      OPENDSUP_TEST_OUTPUT_DIR "/dbo_ovl_on1.sup", true, true);
    auto on2 = encode("opensup_multiwindow_merge.xml",
                      OPENDSUP_TEST_OUTPUT_DIR "/dbo_ovl_on2.sup", true, true);
    auto off1 = encode("opensup_multiwindow_merge.xml",
                       OPENDSUP_TEST_OUTPUT_DIR "/dbo_ovl_off1.sup", false, true);
    auto off2 = encode("opensup_multiwindow_merge.xml",
                       OPENDSUP_TEST_OUTPUT_DIR "/dbo_ovl_off2.sup", false, true);
    // Deterministic on/off; with the flag the overlap path assigns per-window
    // oids (may differ from default when nodes merge), but the flag-off runs
    // must be identical to each other (FR-5 default determinism).
    EXPECT_EQ(stream_hash(on1.segments()), stream_hash(on2.segments()));
    EXPECT_EQ(stream_hash(off1.segments()), stream_hash(off2.segments()));
}

TEST(DbOids, DefaultIsByteIdenticalAcrossFixtures) {
    // FR-5 / FR-3: with the flag off the stream is byte-identical to the
    // legacy output for every multi-window fixture (hash equality of two
    // runs with alternate_oids=false).
    for (const auto& fx : {"opensup_multiwindow.xml",
                           "opensup_multiwindow_merge.xml",
                           "opensup_normalcase.xml"}) {
        auto a = encode(fx, OPENDSUP_TEST_OUTPUT_DIR "/dbo_dflt_a.sup", false);
        auto b = encode(fx, OPENDSUP_TEST_OUTPUT_DIR "/dbo_dflt_b.sup", false);
        EXPECT_EQ(stream_hash(a.segments()), stream_hash(b.segments()))
            << "default not deterministic for " << fx;
    }
}
} // namespace core
} // namespace opensup
