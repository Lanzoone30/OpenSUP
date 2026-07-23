#include <gtest/gtest.h>
#include "opensup/common/bdvideo.h"

namespace opensup {
namespace common {
namespace {

TEST(FPS, FromDouble) {
    auto fps = fps_e::from_double(23.976);
    EXPECT_EQ(fps.value(), fps_e::film_ntsc);
    EXPECT_EQ(fps.numerator(), 24000);
    EXPECT_EQ(fps.denominator(), 1001);
}

TEST(FPS, FromDoubleFilm) {
    auto fps = fps_e::from_double(24.0);
    EXPECT_EQ(fps.value(), fps_e::film);
}

TEST(FPS, ToPcsfps) {
    auto fps = fps_e(fps_e::film_ntsc);
    EXPECT_EQ(fps.to_pcsfps(), 0x10);
}

TEST(FPS, FromPcsfps) {
    auto fps = fps_e::from_pcsfps(0x20);
    EXPECT_EQ(fps.value(), fps_e::film);
}

TEST(FPS, Comparison) {
    fps_e a(fps_e::film);
    fps_e b(fps_e::pal_p);
    EXPECT_LT(a, b);
    EXPECT_GT(b, a);
}

TEST(VideoFormat, GetInfo) {
    auto info = get_format_info(video_format_e::hd1080);
    EXPECT_EQ(info.width, 1920);
    EXPECT_EQ(info.height, 1080);

    info = get_format_info(video_format_e::sd576_43);
    EXPECT_EQ(info.width, 720);
    EXPECT_EQ(info.height, 576);
}

TEST(BDVideo, ConstructFromHeight) {
    bdvideo_c bd(fps_e(fps_e::film), 1080);
    EXPECT_TRUE(bd.format().has_value());
    EXPECT_EQ(bd.format().value(), video_format_e::hd1080);
}

TEST(BDVideo, CheckFormatFps) {
    auto [valid, expected] = bdvideo_c::check_format_fps(
        video_format_e::sd576_43, fps_e(fps_e::pal_p));
    EXPECT_TRUE(valid);

    auto [valid2, expected2] = bdvideo_c::check_format_fps(
        video_format_e::sd576_43, fps_e(fps_e::film));
    EXPECT_FALSE(valid2);
}

} // namespace
} // namespace common
} // namespace opensup
