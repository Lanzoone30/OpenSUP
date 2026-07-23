#pragma once

#include <string>
#include <stdexcept>

namespace opensup {
namespace common {

class opensup_error_x : public std::runtime_error {
public:
    explicit opensup_error_x(const std::string& msg)
        : std::runtime_error(msg) {}
};

class encoding_error_x : public opensup_error_x {
public:
    explicit encoding_error_x(const std::string& msg)
        : opensup_error_x(msg) {}
};

class compliance_error_x : public opensup_error_x {
public:
    explicit compliance_error_x(const std::string& msg)
        : opensup_error_x(msg) {}
};

class parse_error_x : public opensup_error_x {
public:
    explicit parse_error_x(const std::string& msg)
        : opensup_error_x(msg) {}
};

class format_error_x : public opensup_error_x {
public:
    explicit format_error_x(const std::string& msg)
        : opensup_error_x(msg) {}
};

} // namespace common
} // namespace opensup
