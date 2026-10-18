// D7: composition objects (cobjs) per display set must match the oracle.
// Port re-acquires graphics reusing fewer objects than the oracle retains.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

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

}  // namespace core
}  // namespace opensup
