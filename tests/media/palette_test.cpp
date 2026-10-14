#include <gtest/gtest.h>

#include <array>

#include "opensup/media/palette.h"

using opensup::media::palette_entry_t;

namespace {

// struct fields are (y, cr, cb); cr holds Cb and cb holds Cr internally.
std::array<int, 3> ycc(const palette_entry_t& e) {
    return {e.y, e.cr, e.cb};
}

} // namespace

TEST(PaletteColorSpace, DefaultIsBt709) {
    auto a = palette_entry_t::from_rgba(255, 0, 0, 255);
    auto b = palette_entry_t::from_rgba(255, 0, 0, 255, "bt709");
    EXPECT_EQ(ycc(a), ycc(b));
}

TEST(PaletteColorSpace, UnknownMatrixFallsBackToBt709) {
    auto a = palette_entry_t::from_rgba(255, 0, 0, 255, "bogus");
    auto b = palette_entry_t::from_rgba(255, 0, 0, 255, "bt709");
    EXPECT_EQ(ycc(a), ycc(b));
}

TEST(PaletteColorSpace, MatricesDifferOnSaturatedRed) {
    EXPECT_EQ(ycc(palette_entry_t::from_rgba(255, 0, 0, 255, "bt601")),
              (std::array<int, 3>{82, 90, 240}));
    EXPECT_EQ(ycc(palette_entry_t::from_rgba(255, 0, 0, 255, "bt709")),
              (std::array<int, 3>{63, 102, 240}));
    EXPECT_EQ(ycc(palette_entry_t::from_rgba(255, 0, 0, 255, "bt2020")),
              (std::array<int, 3>{74, 97, 240}));
}

TEST(PaletteColorSpace, TransparentBlackIsMatrixInvariant) {
    auto a = palette_entry_t::from_rgba(0, 0, 0, 0, "bt601");
    auto b = palette_entry_t::from_rgba(0, 0, 0, 0, "bt2020");
    EXPECT_EQ(ycc(a), ycc(b));
    EXPECT_EQ(a.y, 16);
    EXPECT_EQ(a.cr, 128);
    EXPECT_EQ(a.cb, 128);
    EXPECT_EQ(a.alpha, 0);
}
