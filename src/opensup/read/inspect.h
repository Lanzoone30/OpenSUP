// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Read-only inspection of existing .sup streams (spec 034): census counters
// (oracle debug_stats) and structural compliance (oracle is_compliant /
// test_diplayset / check_pts_dts_sanity). Nothing here re-emits a stream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "opensup/read/sup_reader.h"

namespace opensup {
namespace read {

/// Census of a .sup stream: segment counts + the oracle debug_stats counters.
struct stream_stats_t {
    int epochs = 0;
    int display_sets = 0;
    int segments = 0;
    size_t bytes = 0;

    int es = 0;          ///< composition state EPOCH_START
    int acq = 0;         ///< composition state ACQUISITION
    int nc = 0;          ///< composition state NORMAL
    int nc_ods = 0;      ///< NORMAL + windows + ODS, no palette update
    int pu = 0;          ///< palette update (PCS pal_flag)
    int pu_buffered = 0; ///< palette update buffered past the window decode time

    int pcs = 0, wds = 0, pds = 0, ods = 0, ends = 0;
};

/// One compliance finding. Hard findings make the stream non-compliant.
struct finding_t {
    size_t ds = 0;      ///< 0-based display set index
    std::string rule;   ///< short rule id (e.g. "test_diplayset/pal_id")
    std::string detail; ///< human-readable explanation
    bool hard = true;
};

/// Result of a structural validation pass.
struct validate_report_t {
    bool compliant = true;
    int max_kbps = 0; ///< 0 = rx_bitrate check disabled
    std::vector<finding_t> findings;
    stream_stats_t stats;
};

/// Census a parsed stream (never fails; malformed input just counts less).
stream_stats_t stream_stats(const sup_stream_t& stream);

/// Run the ported oracle rules. Hard violations flip `compliant` to false.
/// Takes a mutable stream because the segment accessors are non-const.
validate_report_t validate(sup_stream_t& stream, int max_kbps = 0);

}
}
