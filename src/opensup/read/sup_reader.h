// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Reader for existing .sup streams (spec 034). Read-only: it never re-emits.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "opensup/core/segments.h"

namespace opensup {
namespace read {

/// A parsed .sup stream: display sets in stream order plus epoch boundaries.
struct sup_stream_t {
    std::vector<core::display_set_t> display_sets;
    /// Indices into `display_sets` whose PCS starts a new epoch.
    std::vector<size_t> epoch_starts;
    /// Bytes after the last parsable segment (0 for a well-formed stream).
    size_t trailing_bytes = 0;
    /// Structural parse problems (empty for a well-formed stream).
    std::vector<std::string> errors;

    [[nodiscard]] bool well_formed() const noexcept {
        return errors.empty() && trailing_bytes == 0;
    }
    /// Number of epochs (a stream always has at least one if non-empty).
    [[nodiscard]] size_t epochs() const noexcept;

    /// Display sets of one epoch (bounded by the next epoch start).
    [[nodiscard]] std::vector<const core::display_set_t*> epoch(size_t index) const;
};

/// Parse a raw .sup buffer. Never throws; problems land in `errors`.
sup_stream_t parse_sup(const std::vector<uint8_t>& data);

/// Read a file and parse it. An unreadable file yields one error entry.
sup_stream_t load_sup(const std::string& path);

}
}
