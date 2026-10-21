// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include "opensup/pch.h"
#include "opensup/cli/inspector.h"

#include "opensup/media/pgraphics.h"
#include "opensup/read/inspect.h"
#include "opensup/read/sup_reader.h"

#include <CLI/CLI.hpp>

#include <iostream>
#include <sstream>
#include <vector>

namespace opensup {
namespace cli {

namespace {

using core::display_set_t;
using core::ods_c;
using core::pcs_c;
using core::pds_c;
using core::pg_segment_c;
using core::segment_type_e;
using core::wds_c;

const char* type_name(segment_type_e t)
{
    switch (t) {
        case segment_type_e::pcs: return "PCS";
        case segment_type_e::wds: return "WDS";
        case segment_type_e::pds: return "PDS";
        case segment_type_e::ods: return "ODS";
        case segment_type_e::end: return "ENDS";
    }
    return "???";
}

const char* state_name(uint8_t s)
{
    switch (s) {
        case 0x00: return "NORMAL";
        case 0x40: return "ACQUISITION";
        case 0x80: return "EPOCH_START";
        case 0xC0: return "EPOCH_CONTINUE";
    }
    return "UNKNOWN";
}

std::string json_escape(const std::string& in)
{
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

void print_stats(const read::stream_stats_t& st, const std::string& path, bool json)
{
    if (json) {
        std::cout << "{\"type\":\"stats\",\"file\":\"" << json_escape(path) << "\""
                  << ",\"epochs\":" << st.epochs
                  << ",\"display_sets\":" << st.display_sets
                  << ",\"segments\":" << st.segments
                  << ",\"bytes\":" << st.bytes
                  << ",\"es\":" << st.es << ",\"acq\":" << st.acq
                  << ",\"nc\":" << st.nc << ",\"nc_ods\":" << st.nc_ods
                  << ",\"pu\":" << st.pu << ",\"pu_buffered\":" << st.pu_buffered
                  << ",\"pcs\":" << st.pcs << ",\"wds\":" << st.wds
                  << ",\"pds\":" << st.pds << ",\"ods\":" << st.ods
                  << ",\"ends\":" << st.ends << "}\n";
        return;
    }
    std::cout << "file: " << path << "\n"
              << "epochs: " << st.epochs << ", display sets: " << st.display_sets
              << ", segments: " << st.segments << ", bytes: " << st.bytes << "\n"
              << "n(ES)=" << st.es << ", n(ACQ)=" << st.acq << ", n(NC)=" << st.nc
              << " (n(ODS)=" << st.nc_ods << ", n(PU)=" << st.pu << ")."
              << " Buffered: n(PU)=" << st.pu_buffered << ".\n"
              << "PCS=" << st.pcs << " WDS=" << st.wds << " PDS=" << st.pds
              << " ODS=" << st.ods << " ENDS=" << st.ends << "\n";
}

void dump_display_set(size_t index, const display_set_t& ds)
{
    const auto* pcs = [&]() -> const pcs_c* {
        for (auto& seg : ds.segments)
            if (seg->type() == segment_type_e::pcs) return static_cast<const pcs_c*>(seg.get());
        return nullptr;
    }();
    std::cout << "DS #" << index << "  PTS=" << ds.pts() << "s";
    if (pcs) std::cout << "  state=" << state_name(static_cast<uint8_t>(pcs->composition_state()))
                       << "  comp_n=" << pcs->composition_n()
                       << "  objects=" << static_cast<int>(pcs->n_objects())
                       << "  pal_flag=" << (pcs->pal_flag() ? 1 : 0)
                       << "  pal_id=" << static_cast<int>(pcs->pal_id())
                       << "  video=" << pcs->video_width() << "x" << pcs->video_height();
    std::cout << "\n";
    for (auto& seg : ds.segments) {
        std::cout << "  " << type_name(seg->type()) << " size=" << seg->size()
                  << " pts=" << seg->tpts() << " dts=" << seg->tdts();
        if (seg->type() == segment_type_e::wds) {
            auto* wds = static_cast<const wds_c*>(seg.get());
            for (auto& w : wds->windows)
                std::cout << "\n    window " << static_cast<int>(w.window_id) << " @("
                          << w.h_pos << "," << w.v_pos << ") " << w.width << "x" << w.height;
        } else if (seg->type() == segment_type_e::pds) {
            auto* pds = static_cast<const pds_c*>(seg.get());
            std::cout << " p_id=" << static_cast<int>(pds->p_id())
                      << " p_vn=" << static_cast<int>(pds->p_vn());
        } else if (seg->type() == segment_type_e::ods) {
            auto* ods = static_cast<const ods_c*>(seg.get());
            std::cout << " o_id=" << ods->o_id() << " o_vn=" << static_cast<int>(ods->o_vn())
                      << " " << ods->width() << "x" << ods->height()
                      << " rle=" << ods->rle_len()
                      << " seq=0x" << std::hex << static_cast<int>(ods->seq_flags()) << std::dec;
        }
        std::cout << "\n";
    }
}

int run_validate(const std::string& path, bool json, int max_kbps)
{
    auto stream = read::load_sup(path);
    if (!stream.errors.empty() && stream.display_sets.empty()) {
        std::cerr << "validate: cannot read " << path << ": " << stream.errors.front() << "\n";
        return 2;
    }
    auto rep = read::validate(stream, max_kbps);

    if (json) {
        for (auto& f : rep.findings)
            std::cout << "{\"type\":\"" << (f.hard ? "error" : "warn") << "\",\"ds\":" << f.ds
                      << ",\"rule\":\"" << json_escape(f.rule) << "\",\"detail\":\""
                      << json_escape(f.detail) << "\"}\n";
        std::cout << "{\"type\":\"validate\",\"compliant\":" << (rep.compliant ? "true" : "false")
                  << ",\"findings\":" << rep.findings.size() << "}\n";
    } else {
        for (auto& f : rep.findings)
            std::cout << (f.hard ? "ERROR" : "WARN") << " ds=" << f.ds << " " << f.rule << ": "
                      << f.detail << "\n";
        std::cout << (rep.compliant ? "conformant" : "non-conformant") << ": " << path
                  << " (" << rep.stats.display_sets << " display sets, "
                  << rep.stats.epochs << " epochs)\n";
    }
    return rep.compliant ? 0 : 1;
}

int run_inspect(const std::string& path, int only_ds)
{
    auto stream = read::load_sup(path);
    if (!stream.errors.empty() && stream.display_sets.empty()) {
        std::cerr << "inspect: cannot read " << path << ": " << stream.errors.front() << "\n";
        return 2;
    }
    std::cout << "file: " << path << "\nepochs: " << stream.epochs()
              << ", display sets: " << stream.display_sets.size() << "\n";
    for (size_t k = 0; k < stream.display_sets.size(); ++k) {
        if (only_ds >= 0 && static_cast<int>(k) != only_ds) continue;
        dump_display_set(k, stream.display_sets[k]);
    }
    if (only_ds >= 0 && static_cast<size_t>(only_ds) >= stream.display_sets.size()) {
        std::cerr << "inspect: display set " << only_ds << " out of range\n";
        return 2;
    }
    for (auto& e : stream.errors) std::cerr << "inspect: " << e << "\n";
    return 0;
}

}  // namespace

bool is_inspect_subcommand(int argc, char** argv)
{
    if (argc < 2) return false;
    const std::string first = argv[1];
    return first == "validate" || first == "inspect" || first == "stats";
}

int run_inspector(int argc, char** argv)
{
    CLI::App app{"OpenSUP - PGS encoder"};
    app.require_subcommand(1);

    std::string path;
    bool json = false;
    int max_kbps = 0;
    int ds = -1;

    auto* validate_cmd = app.add_subcommand("validate", "Check an existing .sup for spec compliance");
    validate_cmd->add_option("file", path, "Input .sup file")->required();
    validate_cmd->add_flag("--json", json, "Emit NDJSON findings");
    validate_cmd->add_option("--max-kbps", max_kbps, "Enable decoder buffer check");

    auto* inspect_cmd = app.add_subcommand("inspect", "Dump the structure of an existing .sup");
    inspect_cmd->add_option("file", path, "Input .sup file")->required();
    inspect_cmd->add_option("--ds", ds, "Only this display set index (default: all)");

    auto* stats_cmd = app.add_subcommand("stats", "Census counters for an existing .sup");
    stats_cmd->add_option("file", path, "Input .sup file")->required();
    stats_cmd->add_flag("--json", json, "Emit one NDJSON object");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    if (validate_cmd->count() > 0) return run_validate(path, json, max_kbps);
    if (inspect_cmd->count() > 0) return run_inspect(path, ds);
    if (stats_cmd->count() > 0) {
        auto stream = read::load_sup(path);
        if (!stream.errors.empty() && stream.display_sets.empty()) {
            std::cerr << "stats: cannot read " << path << ": " << stream.errors.front() << "\n";
            return 2;
        }
        print_stats(read::stream_stats(stream), path, json);
        return 0;
    }
    return 2;
}

}
}
