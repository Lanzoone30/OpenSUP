#include <gtest/gtest.h>
#include "opensup/common/color_matrix.h"

namespace opensup {
namespace common {
namespace {

TEST(ColorMatrix, GetBT601) {
    const auto& m = get_matrix(color_matrix_e::bt601);
    EXPECT_NEAR(m.y2r[0][0], 1.164, 0.001);
    EXPECT_NEAR(m.y2r[0][2], 1.596, 0.001);
}

TEST(ColorMatrix, GetBT709) {
    const auto& m = get_matrix(color_matrix_e::bt709);
    EXPECT_NEAR(m.y2r[0][0], 1.164, 0.001);
    EXPECT_NEAR(m.y2r[0][2], 1.793, 0.001);
}

TEST(ColorMatrix, GetBT2020) {
    const auto& m = get_matrix(color_matrix_e::bt2020);
    EXPECT_NEAR(m.y2r[0][0], 1.16439, 0.001);
    EXPECT_NEAR(m.y2r[0][2], 1.67867, 0.001);
}

TEST(ColorMatrix, R2Y) {
    const auto& m = get_matrix(color_matrix_e::bt601);
    EXPECT_NEAR(m.r2y[0][0], 0.257, 0.001);
    EXPECT_NEAR(m.r2y[1][1], -0.291, 0.001);
}

TEST(ColorMatrix, FromName) {
    auto m = matrix_from_name("bt709");
    EXPECT_TRUE(m.has_value());
    EXPECT_EQ(m.value(), color_matrix_e::bt709);
}

TEST(ColorMatrix, FromNameInvalid) {
    auto m = matrix_from_name("invalid");
    EXPECT_FALSE(m.has_value());
}

TEST(ColorMatrix, Name) {
    EXPECT_EQ(matrix_name(color_matrix_e::bt601), "bt601");
    EXPECT_EQ(matrix_name(color_matrix_e::bt2020), "bt2020");
}

} // namespace
} // namespace common
} // namespace opensup
