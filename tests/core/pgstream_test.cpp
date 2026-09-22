#include <gtest/gtest.h>
#include "opensup/core/pgstream.h"
#include "opensup/core/segments.h"

namespace opensup {
namespace core {
namespace {

/// Build a raw segment with the given type/timestamps and payload length.
std::shared_ptr<pg_segment_c> make_seg(segment_type_e type, uint32_t pts, uint32_t dts,
                                       size_t payload = 0) {
    std::vector<uint8_t> data(PGS_HEADER_LEN + payload, 0);
    data[0] = 'P';
    data[1] = 'G';
    data[10] = static_cast<uint8_t>(type);
    write_u16_be(&data[11], static_cast<uint16_t>(payload));
    auto seg = std::make_shared<pg_segment_c>(data);
    seg->set_tpts(pts);
    seg->set_tdts(dts);
    return seg;
}

TEST(PgStream, NoUnderflow) {
    // Small segments, one second apart, generous bitrate: buffer never drains.
    std::vector<std::shared_ptr<pg_segment_c>> segs;
    uint32_t t = 90000;
    segs.push_back(make_seg(segment_type_e::pcs, t, t, 100));
    segs.push_back(make_seg(segment_type_e::wds, t, t, 50));
    for (int i = 0; i < 10; i++) {
        t += 90000;
        segs.push_back(make_seg(segment_type_e::pcs, t, t, 100));
        segs.push_back(make_seg(segment_type_e::wds, t, t, 50));
        segs.push_back(make_seg(segment_type_e::end, t, t, 0));
    }

    EXPECT_TRUE(test_rx_bitrate(segs, 16'000'000)); // 16 MB/s refill
}

TEST(PgStream, Underflow) {
    // 20 ODS of ~60 KB back-to-back at t=1s with a tiny refill rate:
    // the 1 MiB buffer cannot hold the burst.
    std::vector<std::shared_ptr<pg_segment_c>> segs;
    const uint32_t t = 90000;
    segs.push_back(make_seg(segment_type_e::pcs, t, t, 0));
    for (int i = 0; i < 20; i++)
        segs.push_back(make_seg(segment_type_e::ods, t, t, 60000));
    segs.push_back(make_seg(segment_type_e::end, t, t, 0));

    EXPECT_FALSE(test_rx_bitrate(segs, 1000)); // 1 KB/s: burst >> refill
}

TEST(PgStream, Wrap32) {
    // Timestamps crossing the 32-bit boundary must wrap, not go negative.
    constexpr int64_t bitrate = 90000; // 1 byte per tick
    const uint32_t t1 = 0xFFFFFF00u;
    const uint32_t t2 = 0x00000010u; // wraps: delta = 0x110 = 272 ticks
    constexpr int64_t payload = 60000;

    // Buffer starts full, 1s before t1. First step drains below full so the
    // second step's refill (272 ticks only) is not hidden by the SIZE cap.
    leaky_buffer_c leaky(static_cast<uint32_t>(t1 - 90000), bitrate);
    EXPECT_TRUE(leaky.step(*make_seg(segment_type_e::pcs, t1, t1, payload)));
    const int64_t used_after_first = leaky_buffer_c::SIZE - (payload + PGS_HEADER_LEN - 2);

    EXPECT_TRUE(leaky.step(*make_seg(segment_type_e::end, t2, t2, 0)));
    // Refill only 272 ticks worth; a negative (unwrapped) delta would drain the
    // buffer straight through zero.
    const int64_t expected = used_after_first + 272 - (PGS_HEADER_LEN - 2);
    EXPECT_NEAR(leaky.usage() * leaky_buffer_c::SIZE, expected, 1e-6);
    EXPECT_GT(expected, 0);
}

TEST(PgStream, StatsReported) {
    std::vector<std::shared_ptr<pg_segment_c>> segs;
    uint32_t t = 90000;
    segs.push_back(make_seg(segment_type_e::pcs, t, t, 100));
    segs.push_back(make_seg(segment_type_e::wds, t, t, 50));
    segs.push_back(make_seg(segment_type_e::end, t, t, 0));
    t += 90000;
    segs.push_back(make_seg(segment_type_e::pcs, t, t, 100));
    segs.push_back(make_seg(segment_type_e::wds, t, t, 50));
    segs.push_back(make_seg(segment_type_e::end, t, t, 0));

    EXPECT_TRUE(test_rx_bitrate(segs, 16'000'000));
}

TEST(PgStream, EmptySegments) {
    EXPECT_TRUE(test_rx_bitrate({}, 1000));
    EXPECT_TRUE(test_rx_bitrate({}, 0));
}

TEST(PgStream, LeakyBufferUsage) {
    // Buffer starts full (1.0); refill is capped at SIZE while a burst of
    // back-to-back ODS drains it below full so the next refill is observable.
    leaky_buffer_c leaky(0, 90000); // 1 byte/tick, first tick at 0
    EXPECT_NEAR(leaky.usage(), 1.0, 1e-9);

    EXPECT_TRUE(leaky.step(*make_seg(segment_type_e::pcs, 90000, 90000, 0)));
    const int64_t after_pcs = leaky_buffer_c::SIZE - (PGS_HEADER_LEN - 2); // refill capped
    EXPECT_NEAR(leaky.usage() * leaky_buffer_c::SIZE, after_pcs, 1e-6);

    for (int i = 0; i < 3; i++) // same tick: no refill between, drains 180033 bytes
        EXPECT_TRUE(leaky.step(*make_seg(segment_type_e::ods, 90000, 90000, 60000)));
    const int64_t drained = after_pcs - 3 * (60000 + PGS_HEADER_LEN - 2);

    EXPECT_TRUE(leaky.step(*make_seg(segment_type_e::end, 180000, 180000, 0)));
    // 1s refill (90000 bytes) then consume the 0-payload ENDS.
    const int64_t expected = drained + 90000 - (PGS_HEADER_LEN - 2);
    EXPECT_NEAR(leaky.usage() * leaky_buffer_c::SIZE, expected, 1e-6);
    EXPECT_GT(expected, 0);

    const auto stats = leaky.stats();
    EXPECT_EQ(stats.count, 5);
    EXPECT_NEAR(stats.min_usage, static_cast<double>(drained) / leaky_buffer_c::SIZE, 1e-9);
    // The running average sits between the deepest drain and full.
    EXPECT_GT(stats.avg_usage, stats.min_usage);
    EXPECT_LT(stats.avg_usage, 1.0);
}

} // namespace
} // namespace core
} // namespace opensup
