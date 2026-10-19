// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Spec 037: cada display set multi-ventana lleva exactamente UN PDS con la
// paleta fusionada de las ventanas (p_id == pal_id del PCS, p_vn secuencial
// por stream, DTS no-regresivo en el stream).
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/core/segments.h"
#include "opensup/media/rle_decode.h"

namespace opensup {
namespace core {
namespace {

constexpr char kNcbdXml[] = OPENDSUP_FIXTURES_DIR "/ncbdn/BDN_Index.xml";

std::vector<std::shared_ptr<pg_segment_c>> render_nc(bool overlap) {
    encode_config_t cfg;
    cfg.input_path = kNcbdXml;
    cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/multiwindow_pds.sup";
    cfg.overwrite = true;
    cfg.extra_acq = 0;
    cfg.overlap = overlap;
    bdn_render_c render(cfg);
    const auto res = render.execute();
    EXPECT_TRUE(res.success) << res.error;
    if (!res.success) return {};
    return render.segments();
}

// Un display set empieza en cada PCS del stream.
std::vector<std::vector<std::shared_ptr<pg_segment_c>>> split_ds(
    const std::vector<std::shared_ptr<pg_segment_c>>& segs)
{
    std::vector<std::vector<std::shared_ptr<pg_segment_c>>> dss;
    for (auto& seg : segs) {
        if (dss.empty() || std::dynamic_pointer_cast<pcs_c>(seg))
            dss.push_back({});
        dss.back().push_back(seg);
    }
    return dss;
}

void check_invariant(const std::vector<std::shared_ptr<pg_segment_c>>& segs,
                     bool overlap)
{
    const auto dss = split_ds(segs);
    ASSERT_FALSE(dss.empty());
    const pds_c* prev = nullptr;
    for (size_t i = 0; i < dss.size(); ++i) {
        const auto& ds = dss[i];
        auto pcs = std::dynamic_pointer_cast<pcs_c>(ds.front());
        ASSERT_TRUE(pcs);
        if (pcs->cobjects.empty()) continue;  // clear/wipe DS carries no PDS
        for (size_t k = 1; k < ds.size(); ++k)
            EXPECT_GE(ds[k]->dts(), ds[k - 1]->dts())
                << "DS " << i << " segmento " << k << ": DTS regresivo";
        std::vector<const pds_c*> pdss;
        for (auto& seg : ds)
            if (auto pd = std::dynamic_pointer_cast<pds_c>(seg))
                pdss.push_back(pd.get());
        ASSERT_FALSE(pdss.empty()) << "DS " << i << " sin PDS";
        ASSERT_EQ(pdss.size(), 1u)
            << "DS " << i << ": " << pdss.size() << " PDS (esperado 1)";
        const pds_c* pd = pdss.front();
        EXPECT_EQ(pd->p_id(), pcs->pal_id());
        if (prev)
            EXPECT_EQ(pd->p_vn(),
                static_cast<uint8_t>((prev->p_vn() + 1) & 0xFF))
                << "DS " << i << ": p_vn no secuencial";
        EXPECT_GE(pd->dts(), prev ? prev->dts() : 0.0)
            << "DS " << i << ": DTS regresivo en el stream";
        if (!overlap) {
            EXPECT_EQ(pd->pts(), pd->dts());
            EXPECT_EQ(pd->dts(), pcs->dts());
        }
        prev = pd;
    }
}

TEST(MultiwindowPdsParity, OnePdsPerDsDefaultPath) {
    if (!std::filesystem::exists(kNcbdXml)) GTEST_SKIP() << "ncbdn fixture no local";
    const auto segs = render_nc(/*overlap=*/false);
    ASSERT_FALSE(segs.empty());
    check_invariant(segs, /*overlap=*/false);
}

TEST(MultiwindowPdsParity, OnePdsPerDsOverlapPath) {
    if (!std::filesystem::exists(kNcbdXml)) GTEST_SKIP() << "ncbdn fixture no local";
    const auto segs = render_nc(/*overlap=*/true);
    ASSERT_FALSE(segs.empty());
    check_invariant(segs, /*overlap=*/true);
}

TEST(MultiwindowPdsParity, OdsIndicesResolveAgainstFusedPalette) {
    if (!std::filesystem::exists(kNcbdXml)) GTEST_SKIP() << "ncbdn fixture no local";
    const auto dss = split_ds(render_nc(false));
    for (size_t i = 0; i < dss.size(); ++i) {
        const auto& ds = dss[i];
        const pds_c* pd = nullptr;
        for (auto& seg : ds)
            if (auto p = std::dynamic_pointer_cast<pds_c>(seg)) pd = p.get();
        for (auto& seg : ds) {
            auto o = std::dynamic_pointer_cast<ods_c>(seg);
            if (!o || !pd) continue;
            auto pal = pd->to_palette().entries();
            const auto idx = media::decode_rle(o->data(), o->width(), o->height());
            for (uint8_t v : idx)
                if (v != 0x00 && v != 0xFF)
                    EXPECT_TRUE(pal.count(v))
                        << "DS " << i << " ODS o_id " << o->o_id()
                        << ": indice " << static_cast<int>(v)
                        << " ausente en la paleta del DS";
        }
    }
}

}  // namespace
}  // namespace core
}  // namespace opensup
