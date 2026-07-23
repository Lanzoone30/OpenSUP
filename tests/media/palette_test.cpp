#include <gtest/gtest.h>
#include "opensup/media/palette.h"

namespace opensup {
namespace media {
namespace {

TEST(PaletteEntry, Defaults) {
    palette_entry_t e;
    EXPECT_EQ(e.y, 16);
    EXPECT_EQ(e.cr, 128);
    EXPECT_EQ(e.cb, 128);
    EXPECT_EQ(e.alpha, 0);
}

TEST(PaletteEntry, ToBytes) {
    palette_entry_t e(100, 200, 50, 255);  // y=Y, cr=Cb, cb=Cr
    auto b = e.to_bytes();
    // PGS spec: [Y, Cr, Cb, Alpha] = {y, cb, cr, alpha} = {100, 50, 200, 255}
    EXPECT_EQ(b[0], 100);  // Y
    EXPECT_EQ(b[1], 50);   // Cr (was cb, now at [1])
    EXPECT_EQ(b[2], 200);  // Cb (was cr, now at [2])
    EXPECT_EQ(b[3], 255);  // A
}

TEST(PaletteEntry, FromBytes) {
    // Input in PGS format: [Y=50, Cr=100, Cb=150, A=200]
    uint8_t data[] = {50, 100, 150, 200};
    auto e = palette_entry_t::from_bytes(data);
    // Internal: y=Y, cr=Cb, cb=Cr
    EXPECT_EQ(e.y, 50);      // Y
    EXPECT_EQ(e.cr, 150);    // Cb (from PGS byte 2)
    EXPECT_EQ(e.cb, 100);    // Cr (from PGS byte 1)
    EXPECT_EQ(e.alpha, 200); // A
}

TEST(PaletteEntry, Equality) {
    palette_entry_t a(10, 20, 30, 40);
    palette_entry_t b(10, 20, 30, 40);
    palette_entry_t c(10, 20, 30, 41);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(PaletteEntry, ToRGBA) {
    palette_entry_t e(16, 128, 128, 255);
    auto rgba = e.to_rgba();
    // Y=16, Cb=128, Cr=128 → should be black
    EXPECT_EQ(rgba.alpha, 255);
}

TEST(PaletteEntry, FromRGBA) {
    auto e = palette_entry_t::from_rgba(0, 0, 0, 255);
    EXPECT_EQ(e.alpha, 255);
}

TEST(Palette, AddAndGet) {
    palette_t pal;
    pal.set(0, palette_entry_t(16, 128, 128, 255));
    pal.set(1, palette_entry_t(100, 200, 50, 128));

    auto e0 = pal.get(0);
    EXPECT_TRUE(e0.has_value());
    EXPECT_EQ(e0->y, 16);

    auto e2 = pal.get(2);
    EXPECT_FALSE(e2.has_value());
}

TEST(Palette, Delete) {
    palette_t pal;
    pal.set(0, palette_entry_t(16, 128, 128, 255));
    EXPECT_EQ(pal.size(), 1);
    pal.remove(0);
    EXPECT_TRUE(pal.empty());
}

TEST(Palette, ToBytes) {
    palette_t pal;
    pal.set(0, palette_entry_t(16, 128, 128, 255));
    pal.set(5, palette_entry_t(100, 200, 50, 128));

    auto bytes = pal.to_bytes();
    EXPECT_EQ(bytes.size(), 2 * 5); // 2 entries × (1 index + 4 bytes)

    auto pal2 = palette_t::from_bytes(bytes.data(), bytes.size());
    EXPECT_EQ(pal2.size(), 2);
    EXPECT_TRUE(pal2.get(0).has_value());
    EXPECT_TRUE(pal2.get(5).has_value());
}

TEST(Palette, Diff) {
    palette_t base;
    base.set(0, palette_entry_t(16, 128, 128, 255));
    base.set(1, palette_entry_t(50, 100, 150, 200));

    palette_t modified;
    modified.set(0, palette_entry_t(16, 128, 128, 255)); // same
    modified.set(1, palette_entry_t(99, 100, 150, 200)); // different
    modified.set(2, palette_entry_t(0, 128, 128, 255));  // new

    auto d = base.diff(modified);
    EXPECT_EQ(d.size(), 2);
    EXPECT_FALSE(d.has(0)); // not in diff (same as base)
    EXPECT_TRUE(d.has(1));  // changed
    EXPECT_TRUE(d.has(2));  // new
}

TEST(Palette, Offset) {
    palette_t pal;
    pal.set(10, palette_entry_t(16, 128, 128, 255));
    pal.set(20, palette_entry_t(50, 100, 150, 200));

    pal.offset(5);
    EXPECT_TRUE(pal.has(15));
    EXPECT_TRUE(pal.has(25));
    EXPECT_FALSE(pal.has(10));
    EXPECT_FALSE(pal.has(20));
}

} // namespace
} // namespace media
} // namespace opensup
