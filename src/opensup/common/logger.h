// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.

#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <fstream>
#include <mutex>

namespace opensup {
namespace common {

enum class log_level_e : uint8_t {
    ldebug = 0,
    hdebug = 1,
    info   = 2,
    iinfo  = 3,
    einfo  = 4,
    warn   = 5,
    pass   = 6,
    error  = 7,
    fail   = 8,
    fatal  = 9,
};

class logger_c {
public:
    static logger_c& instance();

    void set_level(log_level_e level) noexcept;
    [[nodiscard]] log_level_e level() const noexcept { return m_level; }

    void log(log_level_e level, const std::string& msg);
    void info(const std::string& msg)    { log(log_level_e::info, msg); }
    void warn(const std::string& msg)    { log(log_level_e::warn, msg); }
    void error(const std::string& msg)   { log(log_level_e::error, msg); }
    void pass(const std::string& msg)    { log(log_level_e::pass, msg); }
    void fail(const std::string& msg)    { log(log_level_e::fail, msg); }

    void set_file_output(const std::string& path);
    void set_quiet(bool quiet) noexcept { m_quiet = quiet; }

    using log_callback_t = std::function<void(const std::string&, int)>;
    void set_callback(log_callback_t cb) { std::lock_guard<std::mutex> lock(m_mutex); m_callback = std::move(cb); }

    logger_c(const logger_c&) = delete;
    logger_c& operator=(const logger_c&) = delete;

private:
    logger_c() = default;
    ~logger_c() = default;

    static std::string_view level_name(log_level_e level) noexcept;

    log_level_e m_level = log_level_e::info;
    bool m_quiet = false;
    std::mutex m_mutex;
    std::unique_ptr<std::ofstream> m_file;
    log_callback_t m_callback;
};

inline void log_info(const std::string& msg)    { logger_c::instance().info(msg); }
inline void log_warn(const std::string& msg)    { logger_c::instance().warn(msg); }
inline void log_error(const std::string& msg)   { logger_c::instance().error(msg); }
inline void log_fatal(const std::string& msg)   { logger_c::instance().log(log_level_e::fatal, msg); }

} // namespace common
} // namespace opensup
