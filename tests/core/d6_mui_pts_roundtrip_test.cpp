// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Feature 029 / D6: the MUI writer must round-trip timestamps. A --withsup
// encode is decoded with the oracle reader (scenaristream.TSPair.from_mui);
// every MUI record's PTS/DTS must match the twin .sup segment. Today the port
// writes the PTS unshifted, so the oracle decodes garbage: 0 legible.
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "opensup/core/interface.h"

namespace opensup {
namespace core {
namespace {

namespace fs = std::filesystem;

struct ts_record {
    uint8_t type;
    uint32_t pts;
    uint32_t dts;
};

// SUP segment reader: 13-byte PG header (magic, pts, dts, type, length).
// Same framing as the oracle reader already used in tests/core.
std::vector<ts_record> read_sup(const std::string& path) {
    std::vector<ts_record> out;
    std::ifstream f(path, std::ios::binary);
    if (!f) return out;
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    size_t i = 0;
    while (i + 13 <= d.size() && d[i] == 'P' && d[i + 1] == 'G') {
        ts_record r{};
        r.type = d[i + 10];
        r.pts = (uint32_t(d[i + 2]) << 24) | (uint32_t(d[i + 3]) << 16) |
                (uint32_t(d[i + 4]) << 8) | uint32_t(d[i + 5]);
        r.dts = (uint32_t(d[i + 6]) << 24) | (uint32_t(d[i + 7]) << 16) |
                (uint32_t(d[i + 8]) << 8) | uint32_t(d[i + 9]);
        const uint16_t len = uint16_t((d[i + 11] << 8) | d[i + 12]);
        out.push_back(r);
        i += 13u + len;
    }
    return out;
}

std::string shell_quote(const std::string& s) { return "'" + s + "'"; }

// Run the oracle-backed script and parse its per-record TSV (type, pts, dts).
std::vector<ts_record> run_mui_script(const std::string& script,
                                      const std::string& sup,
                                      const std::string& mui,
                                      std::string* stderr_out) {
    const std::string cmd = "python3 " + shell_quote(script) + " " +
                            shell_quote(sup) + " " + shell_quote(mui) + " 2>&1";
    std::vector<ts_record> out;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return out;
    std::string all;
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe)) all += buf;
    const int rc = pclose(pipe);
    if (stderr_out) *stderr_out = all;
    if (rc != 0) return out;  // traceback: caller reports stderr

    std::istringstream ss(all);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        const auto t1 = line.find('\t');
        const auto t2 = line.find('\t', t1 + 1);
        if (t1 == std::string::npos || t2 == std::string::npos) continue;
        ts_record r{};
        r.type = uint8_t(std::stoul(line.substr(0, t1), nullptr, 0));
        r.pts = uint32_t(std::stoul(line.substr(t1 + 1, t2 - t1 - 1)));
        r.dts = uint32_t(std::stoul(line.substr(t2 + 1)));
        out.push_back(r);
    }
    return out;
}

TEST(D6MuiPtsRoundtrip, MuiPesTimestampsMatchSupTwin) {
    const fs::path temp_dir = fs::path(OPENDSUP_TEST_OUTPUT_DIR).parent_path();
    const fs::path repo = temp_dir.parent_path();
    const fs::path fixture = temp_dir / "params-audit" / "ncbdn";
    const fs::path script = repo / "tests" / "scripts" / "mui_pts_tsv.py";

    if (!fs::exists(fixture / "BDN_Index.xml") ||
        !fs::exists(fixture / "0001.png"))
        GTEST_SKIP() << "ncbdn fixture no local: " << fixture;
    if (!fs::exists(script)) GTEST_SKIP() << "script no local: " << script;

    const fs::path out_dir = fs::path(OPENDSUP_TEST_OUTPUT_DIR) / "d6_mui";
    fs::create_directories(out_dir);
    const fs::path sup_path = out_dir / "roundtrip.sup";
    const fs::path mui_path = out_dir / "roundtrip.pes.mui";

    encode_config_t cfg;
    cfg.input_path = (fixture / "BDN_Index.xml").string();
    cfg.output_path = sup_path.string();
    cfg.overwrite = true;
    cfg.both_formats = true;  // --withsup
    cfg.threads = 1;
    bdn_render_c render(cfg);
    const auto res = render.execute();
    ASSERT_TRUE(res.success) << res.error;
    ASSERT_TRUE(fs::exists(mui_path)) << "MUI no generado: " << mui_path;

    const auto sup = read_sup(sup_path.string());
    std::string err;
    const auto mui = run_mui_script(script.string(), sup_path.string(),
                                    mui_path.string(), &err);
    ASSERT_FALSE(mui.empty()) << "script sin records (rc!=0?): " << err;
    ASSERT_EQ(mui.size(), sup.size())
        << "records MUI " << mui.size() << " vs SUP " << sup.size() << "\n"
        << err;

    const size_t n = sup.size();
    size_t legible = 0, exact = 0;
    size_t first_bad = n;
    for (size_t i = 0; i < n; ++i) {
        const bool pts_ok = mui[i].pts == sup[i].pts;
        const bool dts_ok = mui[i].dts == sup[i].dts;
        if (pts_ok) ++legible;
        if (pts_ok && dts_ok) ++exact;
        if (!pts_ok && first_bad == n) first_bad = i;
    }

    std::string detail;
    if (first_bad != n) {
        std::ostringstream os;
        os << "\n  first bad rec " << first_bad << ": mui("
           << mui[first_bad].pts << "," << mui[first_bad].dts << ") sup("
           << sup[first_bad].pts << "," << sup[first_bad].dts << ")";
        detail = os.str();
    }
    EXPECT_EQ(legible, n) << "D6 MUI PTS: legibles " << legible << "/" << n
                          << ", exactos " << exact << "/" << n << detail;
    EXPECT_EQ(exact, n) << "D6 MUI PTS+DTS: exactos " << exact << "/" << n
                        << " (legibles " << legible << "/" << n << ")"
                        << detail;
}

}  // namespace
}  // namespace core
}  // namespace opensup
