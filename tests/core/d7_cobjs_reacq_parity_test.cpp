// D7: composition objects (cobjs) per display set must match the oracle.
// Port re-acquires graphics reusing fewer objects than the oracle retains.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
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

struct cobj_t {
    uint16_t o_id;
    uint8_t window_id;
    bool operator==(const cobj_t& o) const {
        return o_id == o.o_id && window_id == o.window_id;
    }
};

using cobj_set_t = std::vector<cobj_t>;

std::string fmt(const cobj_set_t& s) {
    std::ostringstream os;
    os << "{";
    for (size_t k = 0; k < s.size(); ++k) {
        if (k) os << " ";
        os << "(" << s[k].o_id << "," << static_cast<int>(s[k].window_id) << ")";
    }
    os << "}";
    return os.str();
}

// Cobjs of each display set, in stream order. A PCS (0x16) opens a set and the
// cobjs list is read from its payload; ENDS (0x80) closes it.
std::vector<cobj_set_t> scan_sup_cobjs(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    std::vector<cobj_set_t> sets;
    size_t i = 0;
    cobj_set_t cur;
    bool have_pcs = false;
    while (i + 13 <= d.size() && d[i] == 'P' && d[i + 1] == 'G') {
        const uint8_t type = d[i + 10];
        const size_t len = (static_cast<size_t>(d[i + 11]) << 8) | d[i + 12];
        const uint8_t* p = d.data() + i + 13;
        if (type == 0x16 && len >= 11) {
            cur.clear();
            have_pcs = true;
            const uint8_t n = p[10];
            for (uint8_t k = 0; k < n && 11u + 8u * k + 8u <= len; ++k) {
                const uint8_t* c = p + 11 + 8 * k;
                cur.push_back({static_cast<uint16_t>((c[0] << 8) | c[1]), c[2]});
            }
        } else if (type == 0x80) {
            if (have_pcs) sets.push_back(cur);
            cur.clear();
            have_pcs = false;
        }
        i += 13 + len;
    }
    return sets;
}

void expect_parity(const std::string& port_rel, const std::string& oracle_rel) {
    const std::string port = std::string(OPENDSUP_FIXTURES_DIR) + "/ncbdn/" + port_rel;
    const std::string oracle = std::string(OPENDSUP_FIXTURES_DIR) + "/ncbdn/" + oracle_rel;
    std::ifstream po(port), oo(oracle);
    if (!po || !oo) {
        GTEST_SKIP() << "fixture ausente: " << port_rel << " / " << oracle_rel;
    }
    const auto a = scan_sup_cobjs(port);
    const auto b = scan_sup_cobjs(oracle);
    ASSERT_EQ(a.size(), b.size()) << "cantidad de display sets difiere";
    for (size_t i = 0; i < b.size(); ++i) {
        EXPECT_EQ(a[i].size(), b[i].size())
            << "DS" << i + 1 << " cobjs: port=" << a[i].size()
            << " oraculo=" << b[i].size() << "\n  port   " << fmt(a[i])
            << "\n  oraculo" << fmt(b[i]);
        cobj_set_t missing;
        for (const auto& c : b[i]) {
            if (std::find(a[i].begin(), a[i].end(), c) == a[i].end()) {
                missing.push_back(c);
            }
        }
        EXPECT_TRUE(missing.empty())
            << "DS" << i + 1 << " faltan cobjs (o_id,window_id) del oraculo: "
            << fmt(missing) << "\n  port   " << fmt(a[i]) << "\n  oraculo"
            << fmt(b[i]);
    }
}

// Per-display-set geometry: window bboxes carried by the WDS plus the declared
// size of every ODS, keyed by window_id and o_id respectively.
struct geom_ds_t {
    struct win_t { uint8_t id; uint16_t x, y, w, h; };
    struct obj_t { uint16_t o_id, w, h; };
    std::vector<win_t> wins;
    std::vector<obj_t> objs;
};

std::string fmt_geom(const geom_ds_t& g) {
    std::ostringstream os;
    os << "W{";
    for (size_t k = 0; k < g.wins.size(); ++k) {
        if (k) os << " ";
        os << "(" << static_cast<int>(g.wins[k].id) << "," << g.wins[k].x << ","
           << g.wins[k].y << "," << g.wins[k].w << "," << g.wins[k].h << ")";
    }
    os << "} O{";
    for (size_t k = 0; k < g.objs.size(); ++k) {
        if (k) os << " ";
        os << "(" << g.objs[k].o_id << "," << g.objs[k].w << ","
           << g.objs[k].h << ")";
    }
    os << "}";
    return os.str();
}

// Geometry of each display set, in stream order. Same framing as
// scan_sup_cobjs: PCS (0x16) opens a set, ENDS (0x80) closes it; WDS (0x17) and
// ODS (0x15) payloads contribute windows and object sizes.
std::vector<geom_ds_t> scan_sup_geometry(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    std::vector<geom_ds_t> sets;
    size_t i = 0;
    geom_ds_t cur;
    bool have_pcs = false;
    while (i + 13 <= d.size() && d[i] == 'P' && d[i + 1] == 'G') {
        const uint8_t type = d[i + 10];
        const size_t len = (static_cast<size_t>(d[i + 11]) << 8) | d[i + 12];
        const uint8_t* p = d.data() + i + 13;
        if (type == 0x16) {
            cur = geom_ds_t{};
            have_pcs = true;
        } else if (type == 0x17 && len >= 1) {
            const uint8_t n = p[0];
            for (uint8_t k = 0; k < n && 1u + 9u * k + 9u <= len; ++k) {
                const uint8_t* w = p + 1 + 9 * k;
                cur.wins.push_back(
                    {w[0], static_cast<uint16_t>((w[1] << 8) | w[2]),
                     static_cast<uint16_t>((w[3] << 8) | w[4]),
                     static_cast<uint16_t>((w[5] << 8) | w[6]),
                     static_cast<uint16_t>((w[7] << 8) | w[8])});
            }
        } else if (type == 0x15 && len >= 11) {
            cur.objs.push_back({static_cast<uint16_t>((p[0] << 8) | p[1]),
                                static_cast<uint16_t>((p[7] << 8) | p[8]),
                                static_cast<uint16_t>((p[9] << 8) | p[10])});
        } else if (type == 0x80) {
            if (have_pcs) sets.push_back(cur);
            cur = geom_ds_t{};
            have_pcs = false;
        }
        i += 13 + len;
    }
    return sets;
}

}  // namespace

TEST(D7CobjsReacqParity, Base) {
    expect_parity("nc_cpp_base.sup", "nc_py_base.sup");
}

TEST(D7CobjsReacqParity, AnOverlap) {
    expect_parity("nc_cpp_an_ov.sup", "nc_py_an_ov.sup");
}

// Third D7 case: the --allow-normal variant must re-list the same cobjs as the
// oracle (same re-adquisition reuse rule, no overlap merging).
TEST(D7CobjsReacqParity, AnNormal) {
    expect_parity("nc_cpp_an.sup", "nc_py_an.sup");
}

// T005/T010: bateria 1 base mode. Encodes the local ENIGMA-1080p fixture and
// compares cobjs per display set against the oracle; the acquisition PCS must
// list only the windows the event composites.
TEST(Bateria1CobjsParity, Base) {
    const std::string repo = std::string(OPENDSUP_FIXTURES_DIR) + "/../..";
    const std::string bdn = repo + "/temp/ENIGMA-1080p/BDN_Index_1920.xml";
    const std::string oracle = repo + "/temp/params-audit/py_base.sup";
    if (!std::filesystem::exists(bdn)) GTEST_SKIP() << "bateria 1 no local";
    if (!std::filesystem::exists(oracle))
        GTEST_SKIP() << "oraculo py_base.sup no local";

    const std::string out = repo + "/temp/c_out/cpp_base030.sup";
    std::filesystem::create_directories(
        std::filesystem::path(out).parent_path());
    encode_config_t cfg;
    cfg.input_path = bdn;
    cfg.output_path = out;
    cfg.overwrite = true;
    cfg.threads = 1;
    bdn_render_c render(cfg);
    const auto res = render.execute();
    ASSERT_TRUE(res.success) << res.error;

    const auto port = scan_sup_cobjs(out);
    const auto orac = scan_sup_cobjs(oracle);
    ASSERT_EQ(port.size(), orac.size()) << "cantidad de display sets difiere";

    int divergent = 0;
    std::ostringstream rep;
    for (size_t k = 0; k < orac.size(); ++k) {
        if (port[k].size() == orac[k].size()) continue;
        ++divergent;
        rep << "port=" << port[k].size() << " DS=" << k + 1 << " port "
            << fmt(port[k]) << " oraculo " << fmt(orac[k]) << "\n";
    }
    EXPECT_EQ(divergent, 0) << "DS divergentes=" << divergent << "\n" << rep.str();
}

// T006: display-set geometry (WDS bboxes + ODS sizes). The ncbd fixture has
// two <Graphic> per event; the parser composes their union canvas, so the port
// geometry now matches the oracle byte-for-byte (no skip).
TEST(DFGsReacqParity, Geometry) {
    const std::string nc = std::string(OPENDSUP_FIXTURES_DIR) + "/ncbdn";
    const std::string port = nc + "/nc_cpp_base.sup";
    const std::string oracle = nc + "/nc_py_base.sup";
    std::ifstream po(port), oo(oracle);
    if (!po || !oo) GTEST_SKIP() << "ncbdn no local";

    const auto a = scan_sup_geometry(port);
    const auto b = scan_sup_geometry(oracle);
    ASSERT_EQ(a.size(), b.size()) << "cantidad de display sets difiere";

    int divergent = 0;
    std::ostringstream rep;
    for (size_t k = 0; k < b.size(); ++k) {
        if (fmt_geom(a[k]) == fmt_geom(b[k])) continue;
        ++divergent;
        rep << "DS=" << k + 1 << " port " << fmt_geom(a[k]) << " oraculo "
            << fmt_geom(b[k]) << "\n";
    }
    EXPECT_EQ(divergent, 0) << "DS divergentes=" << divergent << "\n" << rep.str();
}

}  // namespace core
}  // namespace opensup
