// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Read-only CLI: `validate`, `inspect` and `stats` over an existing .sup
// (spec 034). Dispatched from main() before the encode parser.

#pragma once

namespace opensup {
namespace cli {

/// True when argv[1] requests an inspection mode (literal subcommand).
bool is_inspect_subcommand(int argc, char** argv);

/// Run the inspection CLI. @return process exit code.
int run_inspector(int argc, char** argv);

}
}
