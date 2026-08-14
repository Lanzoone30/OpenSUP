// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// NDJSON event emission for Wails UI integration.

#include "opensup/pch.h"
#include "json_emitter.h"

#include <array>
#include <iostream>
#include <string_view>

namespace opensup {
namespace cli {

namespace {

constexpr std::array<std::string_view, 10> kLogLevelNames = {
    "ldebug", "hdebug", "info", "iinfo", "einfo",
    "warn",   "pass",   "error", "fail", "fatal"
};

std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

void emit_line(const std::string& json) {
    std::cout << json << '\n';
    std::cout.flush();
}

} // namespace

void emit_log_event(int level, const std::string& msg) {
    const char* level_name = "info";
    if (level >= 0 && static_cast<std::size_t>(level) < kLogLevelNames.size()) {
        level_name = kLogLevelNames[static_cast<std::size_t>(level)].data();
    }
    emit_line("{\"type\":\"log\",\"level\":\"" + std::string(level_name)
              + "\",\"msg\":\"" + escape_json(msg) + "\"}");
}

void emit_progress_event(int percent, int epoch, int total) {
    emit_line("{\"type\":\"progress\",\"percent\":" + std::to_string(percent)
              + ",\"epoch\":" + std::to_string(epoch)
              + ",\"total\":" + std::to_string(total) + "}");
}

void emit_done_event(const core::encode_result_t& result) {
    if (result.success) {
        emit_line("{\"type\":\"done\",\"success\":true,\"events\":"
                  + std::to_string(result.events)
                  + ",\"epochs\":" + std::to_string(result.epochs)
                  + ",\"segments\":" + std::to_string(result.segments)
                  + ",\"duration_ms\":" + std::to_string(result.duration_ms) + "}");
    } else {
        emit_line("{\"type\":\"done\",\"success\":false,\"error\":\""
                  + escape_json(result.error) + "\"}");
    }
}

} // namespace cli
} // namespace opensup