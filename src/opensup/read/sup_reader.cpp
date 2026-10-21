// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include "opensup/pch.h"
#include "opensup/read/sup_reader.h"

#include <fstream>
#include <iterator>

namespace opensup {
namespace read {

namespace {

using core::pg_segment_c;
using core::PGS_HEADER_LEN;
using core::read_u16_be;

std::shared_ptr<pg_segment_c> make_segment(const std::vector<uint8_t>& seg_data, uint8_t type)
{
    using namespace core;
    switch (type) {
        case 0x16: return std::make_shared<pcs_c>(seg_data);
        case 0x17: return std::make_shared<wds_c>(seg_data);
        case 0x14: return std::make_shared<pds_c>(seg_data);
        case 0x15: return std::make_shared<ods_c>(seg_data);
        case 0x80: return std::make_shared<ends_c>(seg_data);
        default:   return std::make_shared<pg_segment_c>(seg_data);
    }
}

bool is_epoch_start(const core::display_set_t& ds)
{
    for (auto& seg : ds.segments) {
        if (seg->type() == core::segment_type_e::pcs) {
            auto pcs = std::dynamic_pointer_cast<core::pcs_c>(seg);
            return pcs && (static_cast<uint8_t>(pcs->composition_state()) &
                           static_cast<uint8_t>(core::pcs_c::composition_state_e::epoch_start));
        }
    }
    return false;
}

}  // namespace

size_t sup_stream_t::epochs() const noexcept
{
    return epoch_starts.empty() ? (display_sets.empty() ? 0 : 1) : epoch_starts.size();
}

std::vector<const core::display_set_t*> sup_stream_t::epoch(size_t index) const
{
    std::vector<const core::display_set_t*> out;
    if (index >= epoch_starts.size()) return out;
    const size_t begin = epoch_starts[index];
    const size_t end = (index + 1 < epoch_starts.size()) ? epoch_starts[index + 1]
                                                         : display_sets.size();
    for (size_t k = begin; k < end; ++k) out.push_back(&display_sets[k]);
    return out;
}

sup_stream_t parse_sup(const std::vector<uint8_t>& data)
{
    sup_stream_t stream;
    std::vector<std::shared_ptr<pg_segment_c>> current;

    size_t i = 0;
    while (i + PGS_HEADER_LEN <= data.size()) {
        if (data[i] != 'P' || data[i + 1] != 'G') {
            stream.errors.push_back("lost sync at byte " + std::to_string(i) +
                                    " (no PG magic)");
            break;
        }
        const uint8_t type = data[i + 10];
        const size_t len = read_u16_be(&data[i + 11]);
        const size_t total = PGS_HEADER_LEN + len;
        if (i + total > data.size()) {
            stream.errors.push_back("truncated segment at byte " + std::to_string(i));
            break;
        }
        if (type == 0x16 && !current.empty()) {  // a PCS opens a display set
            stream.display_sets.emplace_back(current);
            current.clear();
        }
        std::vector<uint8_t> seg_data(data.begin() + static_cast<ptrdiff_t>(i),
                                      data.begin() + static_cast<ptrdiff_t>(i + total));
        current.push_back(make_segment(seg_data, type));
        i += total;
        if (type == 0x80) {  // ENDS closes it
            stream.display_sets.emplace_back(current);
            current.clear();
        }
    }
    if (!current.empty()) {
        stream.errors.push_back("trailing display set without ENDS");
        stream.display_sets.emplace_back(current);
    }
    stream.trailing_bytes = data.size() - i;

    for (size_t k = 0; k < stream.display_sets.size(); ++k)
        if (is_epoch_start(stream.display_sets[k])) stream.epoch_starts.push_back(k);
    return stream;
}

sup_stream_t load_sup(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        sup_stream_t stream;
        stream.errors.push_back("cannot open file: " + path);
        return stream;
    }
    const std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    if (data.empty()) {
        sup_stream_t stream;
        stream.errors.push_back("empty file: " + path);
        return stream;
    }
    return parse_sup(data);
}

}
}
