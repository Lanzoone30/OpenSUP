// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include "opensup/pch.h"
#include "opensup/common/logger.h"

#include <iostream>
#include <ctime>
#include <iomanip>

namespace opensup {
namespace common {

logger_c&
logger_c::instance()
{
    static logger_c inst;
    return inst;
}

void
logger_c::set_level(log_level_e level) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level = level;
}

std::string_view
logger_c::level_name(log_level_e level) noexcept
{
    switch (level) {
        case log_level_e::ldebug: return "LDEBUG";
        case log_level_e::hdebug: return "HDEBUG";
        case log_level_e::info:   return "INFO";
        case log_level_e::iinfo:  return "INFO";
        case log_level_e::einfo:  return "INFO";
        case log_level_e::warn:   return "WARN";
        case log_level_e::pass:   return "PASS";
        case log_level_e::error:  return "ERROR";
        case log_level_e::fail:   return "FAIL";
        case log_level_e::fatal:  return "FATAL";
    }
    return "UNKN";
}

void
logger_c::log(log_level_e level, const std::string& msg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (level < m_level) return;

    auto now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S") << " "
        << level_name(level) << ": " << msg;

    auto line = oss.str();
    if (!m_quiet) {
        std::cout << line << std::endl;
    }
    if (m_file && m_file->is_open()) {
        (*m_file) << line << std::endl;
    }
    if (m_callback) {
        m_callback(msg, static_cast<int>(level));
    }
}

void
logger_c::set_file_output(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_file = std::make_unique<std::ofstream>(path, std::ios::out);
}

} // namespace common
} // namespace opensup
