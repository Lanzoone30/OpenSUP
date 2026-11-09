// Copyright (C) 2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// The engine phase lines must reach the same logger the GUI and CLI read, in
// order, and the first progress event must be (0, 0, total) so the UI can keep
// its pending state until the first epoch finishes.
#include <gtest/gtest.h>

#include <algorithm>
#include <tuple>
#include <string>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/common/logger.h"

namespace opensup {
namespace core {
namespace {

struct capture_t {
    std::vector<std::string> logs;
    std::vector<std::tuple<int, int, int>> progress;
};

capture_t
run_and_capture(const encode_config_t& base) {
    capture_t out;
    auto cfg = base;
    auto& log = common::logger_c::instance();
    log.set_callback([&](const std::string& msg, int) { out.logs.push_back(msg); });
    cfg.progress_cb = [&](int percent, int epoch, int total) {
        out.progress.emplace_back(percent, epoch, total);
    };
    bdn_render_c render(cfg);
    (void)render.execute();
    log.set_callback(nullptr);
    return out;
}

encode_config_t
base_config() {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/synth_similar.xml";
    cfg.output_path = OPENDSUP_TEST_OUTPUT_DIR "/interface_log.sup";
    cfg.overwrite = true;
    return cfg;
}

bool
has_line(const std::vector<std::string>& logs, const std::string& needle) {
    return std::any_of(logs.begin(), logs.end(), [&](const std::string& line) {
        return line.find(needle) != std::string::npos;
    });
}

}  // namespace

TEST(InterfaceLog, PhasesInOrder) {
    const auto cap = run_and_capture(base_config());
    const std::vector<std::string> phases = {
        "Reading BDN XML...",
        "Input: 3 events, 1920x1080 @ 25.000 fps NDF",
        "Grouping 3 events into epochs and calculating windows...",
        "Split into ",
        "Using ",
        "Encoding epoch 1/",
    };
    size_t from = 0;
    for (const auto& phase : phases) {
        const auto it = std::find_if(cap.logs.begin() + static_cast<std::ptrdiff_t>(from),
                                     cap.logs.end(),
                                     [&](const std::string& line) {
                                         return line.find(phase) != std::string::npos;
                                     });
        ASSERT_NE(it, cap.logs.end()) << "missing phase: " << phase;
        from = static_cast<size_t>(it - cap.logs.begin()) + 1;
    }
}

TEST(InterfaceLog, FirstProgressEventIsPending) {
    const auto cap = run_and_capture(base_config());
    ASSERT_FALSE(cap.progress.empty());
    EXPECT_EQ(std::get<0>(cap.progress.front()), 0);
    EXPECT_EQ(std::get<1>(cap.progress.front()), 0);
    const int total = std::get<2>(cap.progress.front());
    EXPECT_GT(total, 0);
    for (size_t i = 1; i < cap.progress.size(); ++i) {
        EXPECT_GT(std::get<1>(cap.progress[i]), 0);
        EXPECT_EQ(std::get<2>(cap.progress[i]), total);
    }
}

TEST(InterfaceLog, SmallFileSkipsGroupingPercentages) {
    const auto cap = run_and_capture(base_config());
    EXPECT_FALSE(has_line(cap.logs, "Grouping events: "));
}

}  // namespace core
}  // namespace opensup
