// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Tests for boolean-flag parity gates: dual-format overwrite guard and
// post-encode compliance gate for PES+MUI output.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "opensup/core/interface.h"
#include "opensup/common/logger.h"

namespace opensup {
namespace core {
namespace {

constexpr const char* k_input = OPENDSUP_FIXTURES_DIR "/synth_similar.xml";

std::string
output_dir() {
    return OPENDSUP_TEST_OUTPUT_DIR;
}

void
remove_if_exists(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

encode_config_t
base_config(const std::string& output) {
    encode_config_t cfg;
    cfg.input_path = k_input;
    cfg.output_path = output;
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.fps = 25.0;
    cfg.both_formats = true;
    return cfg;
}

size_t
file_size(const std::string& path) {
    std::error_code ec;
    return std::filesystem::file_size(path, ec);
}

TEST(FlagsGate, PesMuiOverwriteGuardBlocks) {
    const std::string sup = output_dir() + "/flags_gate_overwrite.sup";
    const std::string pes = output_dir() + "/flags_gate_overwrite.pes";
    const std::string mui = pes + ".mui";

    remove_if_exists(sup);
    remove_if_exists(pes);
    remove_if_exists(mui);

    // Pre-create PES/MUI as sentinels.
    {
        std::ofstream(pes).put('x');
        std::ofstream(mui).put('x');
    }
    const size_t pes_before = file_size(pes);
    const size_t mui_before = file_size(mui);

    auto cfg = base_config(sup);
    cfg.overwrite = false;

    bdn_render_c render(cfg);
    const auto result = render.execute();

    ASSERT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
    EXPECT_EQ(file_size(pes), pes_before);
    EXPECT_EQ(file_size(mui), mui_before);

    remove_if_exists(sup);
    remove_if_exists(pes);
    remove_if_exists(mui);
}

TEST(FlagsGate, PesMuiOverwriteAllowsWithY) {
    const std::string sup = output_dir() + "/flags_gate_overwrite_y.sup";
    const std::string pes = output_dir() + "/flags_gate_overwrite_y.pes";
    const std::string mui = pes + ".mui";

    remove_if_exists(sup);
    remove_if_exists(pes);
    remove_if_exists(mui);

    {
        std::ofstream(pes).put('x');
        std::ofstream(mui).put('x');
    }

    auto cfg = base_config(sup);
    cfg.overwrite = true;

    bdn_render_c render(cfg);
    const auto result = render.execute();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error.empty());
    EXPECT_GT(file_size(pes), 0u);
    EXPECT_GT(file_size(mui), 0u);

    remove_if_exists(sup);
    remove_if_exists(pes);
    remove_if_exists(mui);
}

TEST(FlagsGate, CompliantWritesPesMui) {
    const std::string sup = output_dir() + "/flags_gate_compliant.sup";
    const std::string pes = output_dir() + "/flags_gate_compliant.pes";
    const std::string mui = pes + ".mui";

    remove_if_exists(sup);
    remove_if_exists(pes);
    remove_if_exists(mui);

    auto cfg = base_config(sup);
    cfg.overwrite = true;
    cfg.max_kbps = 0;  // No bitrate check => compliant.

    bdn_render_c render(cfg);
    const auto result = render.execute();

    ASSERT_TRUE(result.success);
    EXPECT_GT(file_size(pes), 0u);
    EXPECT_GT(file_size(mui), 0u);

    remove_if_exists(sup);
    remove_if_exists(pes);
    remove_if_exists(mui);
}

TEST(FlagsGate, NonCompliantSkipsPesMuiButWritesSup) {
    const std::string sup = output_dir() + "/flags_gate_noncompliant.sup";
    const std::string pes = output_dir() + "/flags_gate_noncompliant.pes";
    const std::string mui = pes + ".mui";

    remove_if_exists(sup);
    remove_if_exists(pes);
    remove_if_exists(mui);

    auto cfg = base_config(sup);
    cfg.overwrite = true;
    // Inject a non-compliant post-encode decision; the SUP must still be written.
    cfg.post_encode_compliance_hook = []() { return false; };

    bdn_render_c render(cfg);
    const auto result = render.execute();

    ASSERT_TRUE(result.success);
    EXPECT_TRUE(std::filesystem::exists(sup));
    EXPECT_FALSE(std::filesystem::exists(pes));
    EXPECT_FALSE(std::filesystem::exists(mui));

    remove_if_exists(sup);
    remove_if_exists(pes);
    remove_if_exists(mui);
}

} // namespace
} // namespace core
} // namespace opensup
