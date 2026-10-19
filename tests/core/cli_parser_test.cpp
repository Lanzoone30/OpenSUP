// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// CLI flag semantics. 032 aligns the surface: quantizer ids
// (2 = HexTree, 3 = LIQ), -n/--allow-normal, -k/--prefer-normal, -d/--ahead,
// -t/--threads, --ssim-tol long-only.
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
TEST(CliParser, UnknownBtMatrixWarnsAndFallsBackTo709) {
    cli_options_t opts;
    opts.bt_matrix = "bt999";
    const auto r = resolve_options(opts);
    EXPECT_EQ(r.cfg.bt_matrix, "bt709");
    ASSERT_EQ(r.warnings.size(), 1u);
    EXPECT_EQ(r.warnings[0], "Unknown color space 'bt999', using BT.709.");
}

TEST(CliParser, QuantizerOutOfRangeWarnsAndFallsBackToPiliq) {
    for (const int q : {5, -1}) {
        cli_options_t opts;
        opts.quantizer = q;
        const auto r = resolve_options(opts);
        EXPECT_EQ(r.cfg.quantizer_id, 3) << "q=" << q;
        ASSERT_EQ(r.warnings.size(), 1u);
        EXPECT_EQ(r.warnings[0],
                  "Invalid quantizer '" + std::to_string(q) + "', using PNGQ/LIQ (3).");
    }
}

TEST(CliParser, QuantizerAvailableIdsStaySilent) {
    for (const int q : {2, 3}) {
        cli_options_t opts;
        opts.quantizer = q;
        const auto r = resolve_options(opts);
        EXPECT_EQ(r.cfg.quantizer_id, q) << "q=" << q;
        EXPECT_TRUE(r.warnings.empty()) << "q=" << q;
    }
}

TEST(CliParser, QuantizerUnavailableBackendAborts) {
    for (const int q : {0, 1, 4}) {
        cli_options_t opts;
        opts.quantizer = q;
        EXPECT_EXIT(options_to_config(opts), ::testing::ExitedWithCode(1),
                    "backend not available in this build");
    }
}

// --- 032: flag surface (shorts, aliases, prefer-normal) ---

/// Run parse_args over argv, collecting warnings emitted while parsing.
cli_options_t
run_cli(std::vector<std::string>& args, std::vector<std::string>& warnings)
{
    auto& log = common::logger_c::instance();
    log.set_callback([&](const std::string& msg, int level) {
        if (level == static_cast<int>(common::log_level_e::warn)) warnings.push_back(msg);
    });
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (auto& s : args) argv.push_back(s.data());
    const auto opts = parse_args(static_cast<int>(argv.size()), argv.data());
    log.set_callback(nullptr);
    return opts;
}

TEST(CliParser, NewShortFlagsMapToSuPerSurface) {
    const std::string path = "/tmp/opensup_cli_parser_shorts.xml";
    std::ofstream(path) << "<BDN/>";
    std::vector<std::string> args{"opensup_cli", "-i", path, "out.sup",
                                  "-d", "-t", "4", "-n", "-k"};
    std::vector<std::string> warnings;
    const auto opts = run_cli(args, warnings);
    EXPECT_TRUE(opts.overlap);
    EXPECT_EQ(opts.threads, 4);
    EXPECT_TRUE(opts.allow_normal_case);
    EXPECT_TRUE(opts.prefer_normal_case);
    EXPECT_TRUE(warnings.empty());
}

TEST(CliParser, OverlapAliasIsDeprecatedButHonored) {
    const std::string path = "/tmp/opensup_cli_parser_alias.xml";
    std::ofstream(path) << "<BDN/>";
    std::vector<std::string> args{"opensup_cli", "-i", path, "out.sup", "--overlap"};
    std::vector<std::string> warnings;
    const auto opts = run_cli(args, warnings);
    EXPECT_TRUE(opts.overlap);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], "--overlap is deprecated, use -d/--ahead.");
}

TEST(CliParser, ThreadsShortAliasIsDeprecatedButHonored) {
    const std::string path = "/tmp/opensup_cli_parser_alias_j.xml";
    std::ofstream(path) << "<BDN/>";
    std::vector<std::string> args{"opensup_cli", "-i", path, "out.sup", "-j", "3"};
    std::vector<std::string> warnings;
    const auto opts = run_cli(args, warnings);
    EXPECT_EQ(opts.threads, 3);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], "-j is deprecated, use -t/--threads.");
}

TEST(CliParser, PreferNormalForcesAllowNormal) {
    cli_options_t opts;
    opts.prefer_normal_case = true;
    const auto r = resolve_options(opts);
    EXPECT_TRUE(r.cfg.allow_normal_case);
    EXPECT_TRUE(r.cfg.prefer_normal_case);
    ASSERT_EQ(r.warnings.size(), 1u);
    EXPECT_EQ(r.warnings[0],
              "--prefer-normal requires --allow-normal, forcefully enabling this flag.");
}

}  // namespace
}  // namespace cli
}  // namespace opensup
