#pragma once

#include <cstdint>
#include <array>
#include <string_view>
#include <optional>

namespace opensup {
namespace common {

using matrix4x4_t = std::array<std::array<double, 4>, 4>;

enum class color_matrix_e : uint8_t {
    bt601  = 0,
    bt709  = 1,
    bt2020 = 2,
};

struct color_matrices_t {
    matrix4x4_t y2r;  // YCbCr → RGBA
    matrix4x4_t r2y;  // RGBA → YCbCr
};

[[nodiscard]] const color_matrices_t& get_matrix(color_matrix_e mat) noexcept;
[[nodiscard]] std::optional<color_matrix_e> matrix_from_name(std::string_view name) noexcept;
[[nodiscard]] std::string_view matrix_name(color_matrix_e mat) noexcept;

} // namespace common
} // namespace opensup
