#include <gtest/gtest.h>
#include "opensup/common/timecode.h"
#include "opensup/common/bdvideo.h"

namespace opensup {
namespace common {
namespace {

TEST(Timecode, FromFrames) {
    fps_e fps(fps_e::film);
    auto tc = tc_t::from_frames(24, fps);
    EXPECT_EQ(tc.frames(), 24);
}

TEST(Timecode, FromSeconds) {
    fps_e fps(fps_e::film);
    auto tc = tc_t::from_seconds(1.0, fps);
    EXPECT_EQ(tc.frames(), 25); // offset +1
}

TEST(Timecode, ToPts) {
    fps_e fps(fps_e::film);
    auto tc = tc_t::from_frames(24, fps);
    double pts = tc.to_pts();
    // frame 24 → (23/24) * 90000 = 86250 / 90000
    EXPECT_NEAR(pts, 0.95833, 0.001);
}

TEST(Timecode, Addition) {
    fps_e fps(fps_e::film);
    auto a = tc_t::from_frames(10, fps);
    auto b = tc_t::from_frames(20, fps);
    auto c = a + b;
    EXPECT_EQ(c.frames(), 30);
}

TEST(Timecode, AdditionInt) {
    fps_e fps(fps_e::film);
    auto a = tc_t::from_frames(10, fps);
    auto b = a + 5;
    EXPECT_EQ(b.frames(), 15);
}

TEST(Timecode, Equality) {
    fps_e fps(fps_e::film);
    auto a = tc_t::from_frames(24, fps);
    auto b = tc_t::from_frames(24, fps);
    EXPECT_EQ(a, b);
}

} // namespace
} // namespace common
} // namespace opensup
