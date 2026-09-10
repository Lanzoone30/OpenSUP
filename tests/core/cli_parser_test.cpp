// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// CLI flag semantics (feature 010 F4): --prefer-normal refuerza allow,
// igual que la UI (app.js:661-668) y SUPer (supercli.py:148-150). Sin el
// flag, los defaults siguen off (Constitution II).
#include <gtest/gtest.h>

#include "opensup/cli/cli_parser.h"

namespace opensup {
namespace cli {
namespace {

TEST(CliParser, PreferNormalImpliesAllow) {
    cli_options_t opts;  // --prefer-normal only, no --allow-normal
    opts.prefer_normal_case = true;
    const auto cfg = options_to_config(opts);
    EXPECT_TRUE(cfg.allow_normal_case);
    EXPECT_TRUE(cfg.prefer_normal_case);
}

TEST(CliParser, DefaultFlagsStayOff) {
    cli_options_t opts;  // no flags
    const auto cfg = options_to_config(opts);
    EXPECT_FALSE(cfg.allow_normal_case);
    EXPECT_FALSE(cfg.prefer_normal_case);
}

TEST(CliParser, ThreadsDefaultAuto) {
    // R5 (feature 010): sin -j el default del CLI es 0 = auto (hardware
    // concurrency), como recomienda la UI (i18n.js:30) — no secuencial.
    cli_options_t opts;  // no -j flag
    const auto cfg = options_to_config(opts);
    EXPECT_EQ(cfg.threads, 0);
}

}  // namespace
}  // namespace cli
}  // namespace opensup
