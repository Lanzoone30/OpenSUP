// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

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
