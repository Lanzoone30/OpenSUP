// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Census counters for a flat PGS segment list (spec 034, US3). Mirrors the
// oracle debug_stats; the encode side counts its own output here.

#pragma once

#include <memory>
#include <vector>

#include "opensup/core/segments.h"

namespace opensup {
namespace core {

/// Segment census of one PGS stream.
struct pg_counters_t {
    int epochs = 0;
    int display_sets = 0;
    int segments = 0;
    int es = 0;          ///< composition state EPOCH_START
    int acq = 0;         ///< composition state ACQUISITION
    int nc = 0;          ///< composition state NORMAL
    int nc_ods = 0;      ///< NORMAL + windows + ODS, no palette update
    int pu = 0;          ///< palette update (PCS pal_flag)
    int pu_buffered = 0; ///< palette update buffered past the window decode time
};

/// Count a stream given in stream order. Display sets end at ENDS; an epoch
/// starts at a PCS carrying the EPOCH_START state.
pg_counters_t count_pg_segments(const std::vector<std::shared_ptr<pg_segment_c>>& segments);

}
}
