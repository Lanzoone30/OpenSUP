#include "opensup/pch.h"
#include "opensup/common/ssim.h"

namespace opensup {
namespace common {

double
ssim_c::compare(const uint8_t* /*img1*/, const uint8_t* /*img2*/,
                int /*width*/, int /*height*/, int /*channels*/)
{
    // ponytail: stub — returns 1.0 (identical) until OpenCV is integrated.
    return 1.0;
}

} // namespace common
} // namespace opensup
