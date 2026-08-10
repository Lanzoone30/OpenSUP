// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.

#pragma once

#include <string>
#include <stdexcept>

namespace opensup {
namespace common {

/// Base exception for all OpenSUP errors.
class opensup_error_x : public std::runtime_error {
public:
    explicit opensup_error_x(const std::string& msg)
        : std::runtime_error(msg) {}
};

/// Raised when the encoder pipeline cannot complete a job.
class encoding_error_x : public opensup_error_x {
public:
    explicit encoding_error_x(const std::string& msg)
        : opensup_error_x(msg) {}
};

/// Raised when a stream fails BD spec compliance checks.
class compliance_error_x : public opensup_error_x {
public:
    explicit compliance_error_x(const std::string& msg)
        : opensup_error_x(msg) {}
};

/// Raised on malformed input files (BDN/XML, SUP).
class parse_error_x : public opensup_error_x {
public:
    explicit parse_error_x(const std::string& msg)
        : opensup_error_x(msg) {}
};

/// Raised when a file does not match the expected format.
class format_error_x : public opensup_error_x {
public:
    explicit format_error_x(const std::string& msg)
        : opensup_error_x(msg) {}
};

} // namespace common
} // namespace opensup
