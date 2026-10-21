// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "opensup/core/segments.h"

namespace opensup {
namespace core {

/// Inputs the BD decode-rate model needs to time one display set.
struct ds_timing_input_t {
    pcs_c::composition_state_e state = pcs_c::composition_state_e::normal;
    bool pal_flag = false;
    uint64_t plane_area = 0;                          // PCS video plane area
    double pts = 0.0;                                 // PCS presentation time, seconds
    std::vector<window_definition_t> windows;         // WDS windows
    std::vector<c_object_t> cobjects;                 // PCS composition objects
    std::vector<std::pair<uint16_t, uint64_t>> ods;   // (o_id, area), sequence-first
};

/// Per-object decode duration carried by an ODS.
struct ods_timing_t {
    uint16_t o_id = 0;
    double obj_decode = 0.0;
};

/// Timestamps for one display set, in seconds.
struct ds_timing_t {
    double wipe_duration = 0.0;
    double decode_duration = 0.0;
    double dts = 0.0;      // pts - decode_duration
    std::vector<ods_timing_t> ods;
};

/// Compute the display-set timestamps from the composition state, window layout
/// and object transfer times. Deterministic and stateless.
[[nodiscard]] ds_timing_t compute_ds_timing(const ds_timing_input_t& in) noexcept;

/// Base decode duration (seconds) of an epoch's window set: a full-container copy
/// plus, per window, the max of the accumulated object-decode time and the running
/// duration, followed by the window copy. Sizes the epoch-boundary check.
[[nodiscard]] double decode_base(const std::vector<window_definition_t>& windows,
                                 uint64_t container_area) noexcept;

}  // namespace core
}  // namespace opensup
