// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Max-bitrate validation is advisory: a decoder-buffer underflow only warns
// and never changes the written .sup.
#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "opensup/core/interface.h"
#include "opensup/common/logger.h"

namespace opensup {
namespace core {
namespace {

constexpr const char* k_underflow_msg =
    "Detected buffer underflow(s) given the provided test bitrate.";

std::vector<std::string>
run_and_capture_warnings(const encode_config_t& cfg) {
    auto& log = common::logger_c::instance();
    std::vector<std::string> msgs;
    log.set_callback([&](const std::string& msg, int level) {
        if (level == static_cast<int>(common::log_level_e::warn)) msgs.push_back(msg);
    });
    bdn_render_c render(cfg);
    (void)render.execute();
    log.set_callback(nullptr);
    return msgs;
}

bool has_msg(const std::vector<std::string>& msgs, const std::string& needle) {
    for (const auto& m : msgs)
        if (m.find(needle) != std::string::npos) return true;
    return false;
}

encode_config_t
base_config(const std::string& output, int max_kbps) {
    encode_config_t cfg;
    cfg.input_path = OPENDSUP_FIXTURES_DIR "/synth_similar.xml";
    cfg.output_path = output;
    cfg.overwrite = true;
    cfg.max_kbps = max_kbps;
    return cfg;
}

std::string
read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

// The underflow branch (test_rx_bitrate == false -> warning) is unreachable
// through a full encode: the decoder buffer is 1 MiB and every fixture yields
// far less. test_rx_bitrate's false path is covered in pgstream_test.cpp; here
// we cover the interface wiring that must stay silent without underflow.

TEST(InterfaceBitrate, NoUnderflowStaysSilent) {
    const auto cfg = base_config(OPENDSUP_TEST_OUTPUT_DIR "/bitrate_ok.sup", 500000);
    EXPECT_FALSE(has_msg(run_and_capture_warnings(cfg), k_underflow_msg));
}

TEST(InterfaceBitrate, OutputUnaffectedByUnderflow) {
    const std::string lo = OPENDSUP_TEST_OUTPUT_DIR "/bitrate_lo.sup";
    const std::string hi = OPENDSUP_TEST_OUTPUT_DIR "/bitrate_hi.sup";
    (void)bdn_render_c(base_config(lo, 1)).execute();
    (void)bdn_render_c(base_config(hi, 500000)).execute();
    EXPECT_EQ(read_file(lo), read_file(hi));
}

}  // namespace
}  // namespace core
}  // namespace opensup
