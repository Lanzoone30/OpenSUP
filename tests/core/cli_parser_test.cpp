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

namespace opensup {
namespace cli {
namespace {

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

}  // namespace
}  // namespace cli
}  // namespace opensup
