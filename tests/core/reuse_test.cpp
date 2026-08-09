#include <gtest/gtest.h>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/core/segments.h"
#include "opensup/core/filestreams.h"
#include "opensup/media/pgraphics.h"
#include "opensup/media/pgstream.h"

namespace opensup {
namespace core {
namespace {

std::string fixture_dir() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().string() + "/fixtures";
}

// Encode a fixture BDN and emit a temp .sup; return {ods_count, reuse_candidates}.
struct encode_stats_t {
    int ods = 0;
    int reuse = 0;
};

encode_stats_t encode_fixture(const std::string& xml) {
    auto out = std::filesystem::temp_directory_path() / "opensup_reuse_test.sup";
    encode_config_t cfg;
    cfg.input_path = fixture_dir() + "/" + xml;
    cfg.output_path = out.string();
    cfg.overwrite = true;
    cfg.ignore_resolution = true;
    bdn_render_c renderer(cfg);
    auto result = renderer.execute();
    encode_stats_t stats;
    if (!result.success) return stats;
    for (auto& s : renderer.segments())
        if (s->type() == segment_type_e::ods) stats.ods++;
    stats.reuse = renderer.reuse_candidates();
    return stats;
}

// With the same image repeated, events 2 and 3 are detected as reusable.
TEST(ReuseDetection, IdenticalImagesAreDetected) {
    auto s = encode_fixture("reuse_identical.bdn.xml");
    EXPECT_EQ(s.reuse, 2);
}

// Control: alternating images are never detected as reusable.
TEST(ReuseDetection, AlternateImagesNotDetected) {
    auto s = encode_fixture("reuse_alternate.bdn.xml");
    EXPECT_EQ(s.reuse, 0);
    EXPECT_EQ(s.ods, 3);
}

// A reusable event must NOT emit a new ODS — the PCS references the object
// already in the decoder buffer. 3 identical events → 1 ODS.
TEST(ReuseEmission, IdenticalImagesEmitOneOds) {
    auto s = encode_fixture("reuse_identical.bdn.xml");
    EXPECT_EQ(s.ods, 1);
}

// Control: alternating images can never reuse → still 3 ODS.
TEST(ReuseEmission, AlternateImagesStillEmitThreeOds) {
    auto s = encode_fixture("reuse_alternate.bdn.xml");
    EXPECT_EQ(s.ods, 3);
}

// Structural: all PCS in the identical-image case reference the SAME object
// id (events 2 and 3 reuse the object decoded by event 1).
TEST(ReuseEmission, PcsReferencesSameObjectId) {
    auto out = std::filesystem::temp_directory_path() / "opensup_reuse_test.sup";
    encode_config_t cfg;
    cfg.input_path = fixture_dir() + "/reuse_identical.bdn.xml";
    cfg.output_path = out.string();
    cfg.overwrite = true;
    cfg.ignore_resolution = true;
    bdn_render_c renderer(cfg);
    ASSERT_TRUE(renderer.execute().success);

    std::vector<uint16_t> obj_ids;
    std::vector<pcs_c::composition_state_e> states;
    for (auto& s : renderer.segments()) {
        if (auto pcs = std::dynamic_pointer_cast<pcs_c>(s)) {
            if (pcs->cobjects.empty()) continue;  // clear DS (no objects)
            obj_ids.push_back(pcs->cobjects[0].o_id);
            states.push_back(pcs->composition_state());
        }
    }
    // 3 events → 3 PCS (plus clear DS PCS are filtered out: they have no cobjects).
    ASSERT_EQ(obj_ids.size(), 3u);
    // All three reference the same object (reused, not re-encoded).
    EXPECT_EQ(obj_ids[0], obj_ids[1]);
    EXPECT_EQ(obj_ids[1], obj_ids[2]);
    // First event is epoch_start; reused events are normal (buffer not reset).
    EXPECT_EQ(states[0], pcs_c::composition_state_e::epoch_start);
    EXPECT_EQ(states[1], pcs_c::composition_state_e::normal);
    EXPECT_EQ(states[2], pcs_c::composition_state_e::normal);
}

// the reused stream must pass the decoder buffer model, compliance
// checks, and PTS/DTS sanity. Encode → write .sup → re-read as display sets.
TEST(ReuseTiming, ReusedStreamPassesValidation) {
    auto out = std::filesystem::temp_directory_path() / "opensup_reuse_test.sup";
    encode_config_t cfg;
    cfg.input_path = fixture_dir() + "/reuse_identical.bdn.xml";
    cfg.output_path = out.string();
    cfg.overwrite = true;
    cfg.ignore_resolution = true;
    bdn_render_c renderer(cfg);
    ASSERT_TRUE(renderer.execute().success);

    sup_file_c sup(out.string());
    auto dss = sup.read_displaysets();
    ASSERT_FALSE(dss.empty());
    std::vector<std::shared_ptr<display_set_t>> epochs;
    for (auto& ds : dss)
        epochs.push_back(std::make_shared<display_set_t>(std::move(ds)));

    int warnings = 0;
    EXPECT_TRUE(media::is_compliant(epochs, 25.0, warnings));
    EXPECT_TRUE(media::check_pts_dts_sanity(epochs, 25.0));
    EXPECT_TRUE(media::test_rx_bitrate(epochs, media::pg_decoder_t::RX, 25.0));
}

// Control: the alternate (non-reused) stream must also pass.
TEST(ReuseTiming, AlternateStreamPassesValidation) {
    auto out = std::filesystem::temp_directory_path() / "opensup_reuse_test.sup";
    encode_config_t cfg;
    cfg.input_path = fixture_dir() + "/reuse_alternate.bdn.xml";
    cfg.output_path = out.string();
    cfg.overwrite = true;
    cfg.ignore_resolution = true;
    bdn_render_c renderer(cfg);
    ASSERT_TRUE(renderer.execute().success);

    sup_file_c sup(out.string());
    auto dss = sup.read_displaysets();
    std::vector<std::shared_ptr<display_set_t>> epochs;
    for (auto& ds : dss)
        epochs.push_back(std::make_shared<display_set_t>(std::move(ds)));

    int warnings = 0;
    EXPECT_TRUE(media::is_compliant(epochs, 25.0, warnings));
    EXPECT_TRUE(media::check_pts_dts_sanity(epochs, 25.0));
    EXPECT_TRUE(media::test_rx_bitrate(epochs, media::pg_decoder_t::RX, 25.0));
}

// Palette stability: our reuse detection compares exact RGBA bytes, so a
// reused object always has the SAME palette as its source (same quantizer →
// same palette). The invariant to verify: every PCS in the reused stream
// references the palette id of an actually-emitted PDS.
TEST(ReusePalette, ReusedPcsReferencesEmittedPalette) {
    auto out = std::filesystem::temp_directory_path() / "opensup_reuse_test.sup";
    encode_config_t cfg;
    cfg.input_path = fixture_dir() + "/reuse_identical.bdn.xml";
    cfg.output_path = out.string();
    cfg.overwrite = true;
    cfg.ignore_resolution = true;
    bdn_render_c renderer(cfg);
    ASSERT_TRUE(renderer.execute().success);

    // Collect palette ids actually emitted (PDS) and referenced (PCS).
    std::set<uint8_t> emitted_palettes;
    std::set<uint8_t> referenced_palettes;
    for (auto& s : renderer.segments()) {
        if (auto pds = std::dynamic_pointer_cast<pds_c>(s))
            emitted_palettes.insert(pds->p_id());
        else if (auto pcs = std::dynamic_pointer_cast<pcs_c>(s)) {
            if (!pcs->cobjects.empty())  // skip clear DS
                referenced_palettes.insert(pcs->pal_id());
        }
    }
    // Exactly one PDS emitted (only the first event); all 3 PCS reference it.
    ASSERT_EQ(emitted_palettes.size(), 1u);
    for (auto id : referenced_palettes)
        EXPECT_TRUE(emitted_palettes.count(id) > 0)
            << "PCS references palette " << static_cast<int>(id)
            << " which was never emitted";
    EXPECT_EQ(referenced_palettes.size(), 1u);
}

// Mixed real-world-like stream — 21 events alternating runs of the same
// image. 9 image changes → exactly 9 ODS expected (one per run, reused within
// each run). Must also pass all stream validations.
TEST(ReuseFinal, MixedStreamEmitsOneOdsPerRun) {
    auto out = std::filesystem::temp_directory_path() / "opensup_reuse_test.sup";
    encode_config_t cfg;
    cfg.input_path = fixture_dir() + "/reuse_mixed.bdn.xml";
    cfg.output_path = out.string();
    cfg.overwrite = true;
    cfg.ignore_resolution = true;
    bdn_render_c renderer(cfg);
    ASSERT_TRUE(renderer.execute().success);

    int ods = 0, reuse = 0;
    for (auto& s : renderer.segments())
        if (s->type() == segment_type_e::ods) ods++;
    reuse = renderer.reuse_candidates();

    // 21 events, 9 image-change boundaries → 9 ODS, 12 reused.
    EXPECT_EQ(ods, 9) << "expected 1 ODS per image run";
    EXPECT_EQ(reuse, 12) << "expected 12 reused events (21 - 9)";

    // Re-read and validate the stream end to end.
    sup_file_c sup(out.string());
    auto dss = sup.read_displaysets();
    std::vector<std::shared_ptr<display_set_t>> epochs;
    for (auto& ds : dss)
        epochs.push_back(std::make_shared<display_set_t>(std::move(ds)));
    int warnings = 0;
    EXPECT_TRUE(media::is_compliant(epochs, 25.0, warnings));
    EXPECT_TRUE(media::check_pts_dts_sanity(epochs, 25.0));
    EXPECT_TRUE(media::test_rx_bitrate(epochs, media::pg_decoder_t::RX, 25.0));
}

// Identical fixture — 3 events, 1 ODS (reuse invariant).
TEST(ReuseFinal, IdenticalFixtureKeepsOneOds) {
    auto s = encode_fixture("reuse_identical.bdn.xml");
    EXPECT_EQ(s.ods, 1);
    EXPECT_EQ(s.reuse, 2);
}

}  // namespace
}  // namespace core
}  // namespace opensup
