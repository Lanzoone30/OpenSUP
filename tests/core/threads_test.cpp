#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "opensup/core/interface.h"

namespace opensup {
namespace core {
namespace {

/// Segment payload hashes (FNV-1a over raw bytes) — byte-exact equality
/// check that does not depend on serialization details.
struct SegmentHasher {
    static size_t hash(const std::shared_ptr<pg_segment_c>& seg) {
        const auto raw = seg->to_bytes();
        size_t h = 14695981039346656037ull;
        for (uint8_t b : raw) {
            h ^= b;
            h *= 1099511628211ull;
        }
        return h;
    }
};

/// Concatenated hash of every segment's bytes (order matters).
static size_t stream_hash(const std::vector<std::shared_ptr<pg_segment_c>>& segs) {
    size_t h = 14695981039346656037ull;
    for (const auto& s : segs) {
        h ^= SegmentHasher::hash(s);
        h *= 1099511628211ull;
    }
    return h;
}

TEST(Threads, DeterministicAcrossWorkerCounts) {
    // Multi-epoch fixture: 6 events spaced >1s apart → 6 epochs.
    const std::string input = OPENDSUP_FIXTURES_DIR "/synth_bdn_multi.xml";
    const std::string out1 = "threads_det_j1.sup";
    const std::string out4 = "threads_det_j4.sup";

    encode_config_t cfg1;
    cfg1.input_path = input;
    cfg1.output_path = out1;
    cfg1.overwrite = true;
    cfg1.threads = 1;

    encode_config_t cfg4 = cfg1;
    cfg4.output_path = out4;
    cfg4.threads = 4;

    bdn_render_c r1(cfg1);
    bdn_render_c r4(cfg4);
    const auto res1 = r1.execute();
    const auto res4 = r4.execute();

    ASSERT_TRUE(res1.success) << res1.error;
    ASSERT_TRUE(res4.success) << res4.error;
    ASSERT_EQ(res4.segments, res1.segments);
    EXPECT_EQ(stream_hash(r1.segments()), stream_hash(r4.segments()))
        << "parallel (-j4) output differs from sequential (-j1)";
}

TEST(Threads, AutoThreadsSameOutput) {
    // 0 = auto: same deterministic result as explicit worker counts.
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/synth_bdn.xml";
    cfg.output_path = "threads_auto.sup";
    cfg.overwrite = true;
    cfg.threads = 0;

    bdn_render_c r(cfg);
    const auto res = r.execute();
    ASSERT_TRUE(res.success) << res.error;
    EXPECT_GT(res.epochs, 0);

    // Sequential reference on the same input.
    encode_config_t ref_cfg = cfg;
    ref_cfg.threads = 1;
    bdn_render_c ref(ref_cfg);
    ASSERT_TRUE(ref.execute().success);
    EXPECT_EQ(stream_hash(r.segments()), stream_hash(ref.segments()));
}

}  // namespace
}  // namespace core
}  // namespace opensup