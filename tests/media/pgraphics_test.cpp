#include <gtest/gtest.h>
#include "opensup/media/pgraphics.h"

namespace opensup {
namespace media {
namespace {

TEST(RLE, EncodeDecodeSmall) {
    std::vector<uint8_t> bitmap = {1, 2, 2, 2, 3, 3, 0, 0, 0, 4};
    auto encoded = encode_rle(bitmap, 5, 2);
    auto decoded = decode_rle(encoded, 5, 2);
    EXPECT_EQ(bitmap, decoded);
}

TEST(RLE, EncodeDecodeZeros) {
    std::vector<uint8_t> bitmap = {0, 0, 0, 0, 0, 1, 0, 0, 2, 2};
    auto encoded = encode_rle(bitmap, 5, 2);
    auto decoded = decode_rle(encoded, 5, 2);
    EXPECT_EQ(bitmap, decoded);
}

TEST(RLE, EncodeDecodeSinglePixel) {
    std::vector<uint8_t> bitmap = {42};
    auto encoded = encode_rle(bitmap, 1, 1);
    auto decoded = decode_rle(encoded, 1, 1);
    EXPECT_EQ(bitmap, decoded);
}

TEST(RLE, EncodeDecodeUniform) {
    std::vector<uint8_t> bitmap(100, 7);
    auto encoded = encode_rle(bitmap, 10, 10);
    auto decoded = decode_rle(encoded, 10, 10);
    EXPECT_EQ(bitmap, decoded);
}

TEST(RLE, EncodeDecodeAllZeros) {
    std::vector<uint8_t> bitmap(256, 0);
    auto encoded = encode_rle(bitmap, 16, 16);
    auto decoded = decode_rle(encoded, 16, 16);
    EXPECT_EQ(bitmap, decoded);
}

TEST(PGDecoder, DecodeObjDuration) {
    double dur = pg_decoder_t::decode_obj_duration(1920 * 1080);
    EXPECT_GT(dur, 0.0);
}

TEST(PGDecoder, CopyGPduration) {
    double dur = pg_decoder_t::copy_gp_duration(1920 * 1080);
    EXPECT_GT(dur, 0.0);
}

TEST(BufferSlot, Create) {
    buffer_slot_t slot(100, 200);
    EXPECT_EQ(slot.width, 100);
    EXPECT_EQ(slot.height, 200);
    EXPECT_EQ(slot.size(), 20000);
    EXPECT_EQ(slot.version, -1);
}

TEST(BufferSlot, LockUntil) {
    buffer_slot_t slot(100, 100);
    EXPECT_TRUE(slot.writable_at(0.0));
    slot.lock_until(10.0);
    EXPECT_FALSE(slot.writable_at(5.0));
    EXPECT_TRUE(slot.writable_at(10.0));
    EXPECT_TRUE(slot.writable_at(15.0));
    EXPECT_EQ(slot.version, 0);
}

TEST(PGObjectBuffer, RequestSlot) {
    pg_object_buffer_t buf;
    auto [id, slot] = buf.request_slot(100, 100, 0.0);
    EXPECT_TRUE(id.has_value());
    EXPECT_NE(slot, nullptr);
    EXPECT_EQ(slot->width, 100);
}

TEST(PGObjectBuffer, ReuseSlot) {
    pg_object_buffer_t buf;
    auto [id1, slot1] = buf.request_slot(100, 100, 0.0);
    slot1->lock_until(10.0);
    // Same dims, dts=15.0 > pts=10.0 → should reuse
    auto [id2, slot2] = buf.request_slot(100, 100, 15.0);
    EXPECT_TRUE(id2.has_value());
}

TEST(PGObjectBuffer, Allocate) {
    pg_object_buffer_t buf;
    auto id = buf.allocate(100, 100);
    EXPECT_TRUE(id.has_value());
    auto slot = buf.get(*id);
    EXPECT_NE(slot, nullptr);
}

TEST(PGObjectBuffer, GetSlotVersion) {
    pg_object_buffer_t buf;
    auto [id, slot] = buf.request_slot(100, 100, 0.0);
    slot->lock_until(10.0);
    auto ver = buf.get_slot_version(*id);
    EXPECT_TRUE(ver.has_value());
}

} // namespace
} // namespace media
} // namespace opensup
