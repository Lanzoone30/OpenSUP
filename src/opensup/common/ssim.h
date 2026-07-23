#pragma once

// ponytail: SSIM wrapper stub — full implementation requires OpenCV/SSIM-PIL dependency.
// Add when SSIM-based scene detection is needed in the encoder pipeline.
namespace opensup {
namespace common {

class ssim_c {
public:
    static double compare(const uint8_t* img1, const uint8_t* img2,
                          int width, int height, int channels = 4);
};

} // namespace common
} // namespace opensup
