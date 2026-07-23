#include <gtest/gtest.h>
#include "opensup/common/geometry.h"

namespace opensup {
namespace common {
namespace {

TEST(GeometryPos, Equality) {
    pos_t a{10, 20};
    pos_t b{10, 20};
    pos_t c{10, 30};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(GeometryShape, Area) {
    shape_t s{100, 200};
    EXPECT_EQ(s.area(), 20000);
    EXPECT_EQ(s.width(), 100);
    EXPECT_EQ(s.height(), 200);
}

TEST(GeometryShape, FromBox) {
    box_t b{0, 50, 0, 100};
    auto s = shape_t::from_box(b);
    EXPECT_EQ(s.w, 100);
    EXPECT_EQ(s.h, 50);
}

TEST(GeometryShape, Union) {
    shape_t a{100, 100};
    shape_t b{200, 50};
    auto u = shape_t::union_shape({a, b});
    EXPECT_EQ(u.w, 200);
    EXPECT_EQ(u.h, 100);
}

TEST(GeometryBox, Area) {
    box_t b{0, 100, 0, 200};
    EXPECT_EQ(b.area(), 20000);
}

TEST(GeometryBox, Coordinates) {
    box_t b{10, 100, 20, 200};
    EXPECT_EQ(b.x, 20);
    EXPECT_EQ(b.y, 10);
    EXPECT_EQ(b.dx, 200);
    EXPECT_EQ(b.dy, 100);
    EXPECT_EQ(b.x2(), 220);
    EXPECT_EQ(b.y2(), 110);
}

TEST(GeometryBox, Intersection) {
    box_t a{0, 100, 0, 100};
    box_t b{50, 100, 50, 100};
    auto r = box_t::intersect(a, b);
    EXPECT_EQ(r.x, 50);
    EXPECT_EQ(r.y, 50);
    EXPECT_EQ(r.dx, 50);
    EXPECT_EQ(r.dy, 50);
    EXPECT_EQ(r.area(), 2500);
}

TEST(GeometryBox, NoIntersection) {
    box_t a{0, 100, 0, 100};
    box_t b{200, 100, 200, 100};
    auto r = box_t::intersect(a, b);
    EXPECT_EQ(r.area(), 0);
}

TEST(GeometryBox, Union) {
    box_t a{0, 100, 0, 100};
    box_t b{50, 100, 50, 100};
    auto r = box_t::union_box(a, b);
    EXPECT_EQ(r.x, 0);
    EXPECT_EQ(r.y, 0);
    EXPECT_EQ(r.dx, 150);
    EXPECT_EQ(r.dy, 150);
    EXPECT_EQ(r.area(), 22500);
}

TEST(GeometryBox, FromCoords) {
    auto r = box_t::from_coords(10, 20, 110, 120);
    EXPECT_EQ(r.x, 10);
    EXPECT_EQ(r.y, 20);
    EXPECT_EQ(r.dx, 100);
    EXPECT_EQ(r.dy, 100);
}

TEST(GeometryBox, OverlapWith) {
    box_t a{0, 100, 0, 100};
    box_t b{50, 100, 50, 100};
    double overlap = a.overlap_with(b);
    EXPECT_NEAR(overlap, 0.25, 0.01);
}

} // namespace
} // namespace common
} // namespace opensup
