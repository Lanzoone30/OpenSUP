// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// CLI flag semantics: --allow-normal is the only normal-case flag exposed.
// --prefer-normal was retired (feature 013); the engine wiring stays reserved
// for a future faithful implementation (specs/013-prefer-normal-case).
#include <gtest/gtest.h>

#include "opensup/cli/cli_parser.h"
#include "opensup/common/logger.h"

#include <fstream>
#include <string>
#include <vector>

namespace opensup {
namespace cli {
namespace {

/// Collect warnings emitted while resolving a single -m value.
std::vector<std::string>
warnings_for_max_kbps(int max_kbps)
{
    auto& log = common::logger_c::instance();
    std::vector<std::string> msgs;
    log.set_callback([&](const std::string& msg, int level) {
        if (level == static_cast<int>(common::log_level_e::warn)) msgs.push_back(msg);
    });
    cli_options_t opts;
    opts.max_kbps = max_kbps;
    (void)options_to_config(opts);
    log.set_callback(nullptr);
    return msgs;
}

TEST(CliParser, MaxKbpsZeroDisablesSilently) {
    EXPECT_TRUE(warnings_for_max_kbps(0).empty());
}

TEST(CliParser, MaxKbpsLowWarnsUnderflow) {
    const auto warnings = warnings_for_max_kbps(50);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], "Max bitrate is low. Buffer underflow errors will be spammed.");
}

TEST(CliParser, MaxKbpsBeyondBdavWarns) {
    const auto warnings = warnings_for_max_kbps(48001);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], "Max bitrate is beyond BDAV limit.");
}

TEST(CliParser, MaxKbpsBoundariesStaySilent) {
    EXPECT_TRUE(warnings_for_max_kbps(10).empty());
    EXPECT_TRUE(warnings_for_max_kbps(500).empty());
    EXPECT_TRUE(warnings_for_max_kbps(48000).empty());
    EXPECT_TRUE(warnings_for_max_kbps(16000).empty());
}

TEST(CliParser, MaxKbpsMeaninglessAborts) {
    const std::string input_path = "/tmp/opensup_cli_parser_max_kbps.xml";
    std::ofstream(input_path) << "<BDN/>";

    std::string a0 = "opensup_cli", a1 = "-i", a2 = input_path;
    std::string a3 = "out.sup", a4 = "-m", a5 = "5";
    std::vector<char*> argv{a0.data(), a1.data(), a2.data(),
                            a3.data(), a4.data(), a5.data()};

    EXPECT_EXIT(parse_args(static_cast<int>(argv.size()), argv.data()),
                ::testing::ExitedWithCode(1), "Meaningless max bitrate");
}

TEST(CliParser, AllowNormalPassesThrough) {
    cli_options_t opts;
    opts.allow_normal_case = true;
    const auto cfg = options_to_config(opts);
    EXPECT_TRUE(cfg.allow_normal_case);
}

TEST(CliParser, DefaultFlagsStayOff) {
    cli_options_t opts;  // no flags
    const auto cfg = options_to_config(opts);
    EXPECT_FALSE(cfg.allow_normal_case);
}

TEST(CliParser, ThreadsDefaultAuto) {
    // R5 (feature 010): sin -j el default del CLI es 0 = auto (hardware
    // concurrency), como recomienda la UI (i18n.js:30) — no secuencial.
    cli_options_t opts;  // no -j flag
    const auto cfg = options_to_config(opts);
    EXPECT_EQ(cfg.threads, 0);
}

TEST(CliParser, RedrawPeriodBelowOneSecondDisabled) {
    cli_options_t opts;
    opts.redraw_period = 0.5;
    EXPECT_DOUBLE_EQ(options_to_config(opts).redraw_period, 0.0);
}

TEST(CliParser, RedrawPeriodAtLeastOneSecondPassesThrough) {
    cli_options_t opts;
    opts.redraw_period = 1.5;
    EXPECT_DOUBLE_EQ(options_to_config(opts).redraw_period, 1.5);
}

TEST(CliParser, ConfigOverridesMapToLiqConfig) {
    cli_options_t opts;
    opts.config_path = "/tmp/opensup_cli_parser_liq.ini";
    opts.liq_quality_cfg = 80;
    opts.liq_speed_cfg = 10;
    opts.liq_dither_cfg = 30;
    const auto cfg = options_to_config(opts);
    EXPECT_EQ(cfg.liq_quality, 80);
    EXPECT_EQ(cfg.liq_speed, 10);
    EXPECT_EQ(cfg.liq_dither_pct, 30);
}

TEST(CliParser, ConfigPartialOverrideKeepsLiqDefaults) {
    cli_options_t opts;
    opts.liq_quality_cfg = 80;
    const auto cfg = options_to_config(opts);
    EXPECT_EQ(cfg.liq_quality, 80);
    EXPECT_EQ(cfg.liq_speed, 4);
    EXPECT_EQ(cfg.liq_dither_pct, 95);
}

TEST(CliParser, NoConfigKeepsLiqDefaults) {
    cli_options_t opts;  // no --config
    const auto cfg = options_to_config(opts);
    EXPECT_EQ(cfg.liq_quality, 95);
    EXPECT_EQ(cfg.liq_speed, 4);
    EXPECT_EQ(cfg.liq_dither_pct, 95);
}

TEST(CliParser, ReadLiqConfigParsesPiliqSection) {
    const std::string path = "/tmp/opensup_cli_parser_liq.ini";
    std::ofstream(path) << "; comment\n[OTHER]\nquality=1\n"
                           "[PILIQ]\nquality = 80\ndither=30\n";

    cli_options_t opts;
    ASSERT_TRUE(read_liq_config(path, opts));
    EXPECT_EQ(opts.liq_quality_cfg, 80);
    EXPECT_EQ(opts.liq_speed_cfg, -1);  // absent key stays sentinel
    EXPECT_EQ(opts.liq_dither_cfg, 30);
}

TEST(CliParser, ReadLiqConfigMissingFileFails) {
    cli_options_t opts;
    EXPECT_FALSE(read_liq_config("/tmp/opensup_cli_parser_nonexistent.ini", opts));
    EXPECT_EQ(opts.liq_quality_cfg, -1);
}

TEST(CliParser, ReadLiqConfigRejectsInvalidValues) {
    const std::string path = "/tmp/opensup_cli_parser_liq_bad.ini";
    cli_options_t opts;

    std::ofstream(path) << "[PILIQ]\nquality=101\n";
    EXPECT_FALSE(read_liq_config(path, opts));

    std::ofstream(path, std::ios::trunc) << "[PILIQ]\nspeed=4x\n";
    EXPECT_FALSE(read_liq_config(path, opts));
}

/// Resolve one cli_options_t into its config, collecting warnings.
struct resolved_t {
    core::encode_config_t cfg;
    std::vector<std::string> warnings;
};

resolved_t
resolve_options(const cli_options_t& opts)
{
    auto& log = common::logger_c::instance();
    resolved_t r;
    log.set_callback([&](const std::string& msg, int level) {
        if (level == static_cast<int>(common::log_level_e::warn)) r.warnings.push_back(msg);
    });
    r.cfg = options_to_config(opts);
    log.set_callback(nullptr);
    return r;
}

// Oracle parity (supercli.py:116-119): unknown matrix/quantizer warns and
// falls back instead of aborting.
}  // namespace
}  // namespace cli
}  // namespace opensup
