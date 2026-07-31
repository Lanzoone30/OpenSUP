// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.

#include "opensup/pch.h"
#include "opensup/common/color_matrix.h"

namespace opensup {
namespace common {

static constexpr color_matrices_t BT601 = {{
    {{1.164,      0,  1.596, 0},
     {1.164, -0.392, -0.813, 0},
     {1.164,  2.017,      0, 0},
     {    0,      0,      0, 1}}
},{
    {{ 0.257,  0.504,  0.098, 0},
     {-0.148, -0.291,  0.439, 0},
     { 0.439, -0.368, -0.071, 0},
     {     0,      0,      0, 1}}
}};

static constexpr color_matrices_t BT709 = {{
    {{1.164,      0,   1.793, 0},
     {1.164, -0.213,  -0.533, 0},
     {1.164,  2.112,       0, 0},
     {    0,      0,       0, 1}}
},{
    {{ 0.183,  0.614,  0.062, 0},
     {-0.101, -0.339,  0.439, 0},
     { 0.439, -0.399, -0.040, 0},
     {     0,      0,      0, 1}}
}};

static constexpr color_matrices_t BT2020 = {{
    {{1.16439,      0, 1.67867, 0},
     {1.16439, -0.18734,-0.65042,0},
     {1.16439,  2.14175,      0,0},
     {     0,       0,      0,1}}
},{
    {{0.22561, 0.58228, 0.05093, 0},
     {-0.12266,-0.31656, 0.43922, 0},
     { 0.43922,-0.40389,-0.03533, 0},
     {      0,      0,       0, 1}}
}};

const color_matrices_t&
get_matrix(color_matrix_e mat) noexcept
{
    switch (mat) {
        case color_matrix_e::bt601:  return BT601;
        case color_matrix_e::bt709:  return BT709;
        case color_matrix_e::bt2020: return BT2020;
    }
    return BT709;
}

std::optional<color_matrix_e>
matrix_from_name(std::string_view name) noexcept
{
    if (name == "bt601")  return color_matrix_e::bt601;
    if (name == "bt709")  return color_matrix_e::bt709;
    if (name == "bt2020") return color_matrix_e::bt2020;
    return std::nullopt;
}

std::string_view
matrix_name(color_matrix_e mat) noexcept
{
    switch (mat) {
        case color_matrix_e::bt601:  return "bt601";
        case color_matrix_e::bt709:  return "bt709";
        case color_matrix_e::bt2020: return "bt2020";
    }
    return "bt709";
}

} // namespace common
} // namespace opensup
