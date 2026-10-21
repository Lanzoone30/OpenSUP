// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// NDJSON event emission for Wails UI integration.
// Each event is a single JSON line, flushed immediately.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "opensup/core/interface.h"

namespace opensup {
namespace cli {

/// Emit a log event: {"type":"log","level":"info","msg":"..."}
void emit_log_event(int level, const std::string& msg);

/// Emit a palette warning: {"type":"warn","kind":"out-of-range","count":2}
void emit_warn(const std::string& kind, size_t count);

/// Emit a progress event: {"type":"progress","percent":42,"epoch":3,"total":12}
void emit_progress_event(int percent, int epoch, int total);

/// Emit a done event with full encode result.
void emit_done_event(const core::encode_result_t& result);

}
}