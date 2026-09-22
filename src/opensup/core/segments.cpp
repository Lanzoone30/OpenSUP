// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/core/segments.h"

#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace opensup {
namespace core {

// ── Big-endian helpers ──
uint16_t read_u16_be(const uint8_t* data) noexcept {
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

uint32_t read_u24_be(const uint8_t* data) noexcept {
    return (static_cast<uint32_t>(data[0]) << 16) |
           (static_cast<uint32_t>(data[1]) << 8)  | data[2];
}

uint32_t read_u32_be(const uint8_t* data) noexcept {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8)  | data[3];
}

void write_u16_be(uint8_t* data, uint16_t val) noexcept {
    data[0] = static_cast<uint8_t>((val >> 8) & 0xFF);
    data[1] = static_cast<uint8_t>(val & 0xFF);
}

void write_u24_be(uint8_t* data, uint32_t val) noexcept {
    data[0] = static_cast<uint8_t>((val >> 16) & 0xFF);
    data[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    data[2] = static_cast<uint8_t>(val & 0xFF);
}

void write_u32_be(uint8_t* data, uint32_t val) noexcept {
    data[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
    data[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    data[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(val & 0xFF);
}

segment_type_e segment_type_from_byte(uint8_t b) {
    switch (b) {
        case 0x14: return segment_type_e::pds;
        case 0x15: return segment_type_e::ods;
        case 0x16: return segment_type_e::pcs;
        case 0x17: return segment_type_e::wds;
        case 0x80: return segment_type_e::end;
        default: throw std::invalid_argument("Unknown segment type");
    }
}

// ── Window Definition ──
std::vector<uint8_t> window_definition_t::to_bytes() const {
    std::vector<uint8_t> out(9);
    out[0] = window_id;
    write_u16_be(&out[1], h_pos);
    write_u16_be(&out[3], v_pos);
    write_u16_be(&out[5], width);
    write_u16_be(&out[7], height);
    return out;
}

window_definition_t window_definition_t::from_bytes(const uint8_t* data) {
    window_definition_t wd;
    wd.window_id = data[0];
    wd.h_pos  = read_u16_be(data + 1);
    wd.v_pos  = read_u16_be(data + 3);
    wd.width  = read_u16_be(data + 5);
    wd.height = read_u16_be(data + 7);
    return wd;
}

// ── CObject ──
void c_object_t::set_forced(bool f) noexcept {
    flags = static_cast<uint8_t>((flags & ~forced) | (f ? forced : 0));
}

std::vector<uint8_t> c_object_t::to_bytes() const {
    if (is_cropped()) {
        std::vector<uint8_t> out(16);
        write_u16_be(&out[0], o_id);
        out[2] = window_id;
        out[3] = flags;
        write_u16_be(&out[4], h_pos);
        write_u16_be(&out[6], v_pos);
        write_u16_be(&out[8], hc_pos);
        write_u16_be(&out[10], vc_pos);
        write_u16_be(&out[12], c_w);
        write_u16_be(&out[14], c_h);
        return out;
    } else {
        std::vector<uint8_t> out(8);
        write_u16_be(&out[0], o_id);
        out[2] = window_id;
        out[3] = flags;
        write_u16_be(&out[4], h_pos);
        write_u16_be(&out[6], v_pos);
        return out;
    }
}

c_object_t c_object_t::from_bytes(const uint8_t* data, bool cropped) {
    c_object_t obj;
    obj.o_id      = read_u16_be(data);
    obj.window_id = data[2];
    obj.flags     = data[3];
    obj.h_pos     = read_u16_be(data + 4);
    obj.v_pos     = read_u16_be(data + 6);
    if (obj.is_cropped() || cropped) {
        obj.hc_pos = read_u16_be(data + 8);
        obj.vc_pos = read_u16_be(data + 10);
        obj.c_w    = read_u16_be(data + 12);
        obj.c_h    = read_u16_be(data + 14);
    }
    return obj;
}

// ── PGSegment ──
pg_segment_c::pg_segment_c(const std::vector<uint8_t>& data)
    : m_data(data)
{
    if (m_data.size() < PGS_HEADER_LEN)
        throw std::invalid_argument("Data too short for PGS header");
    if (m_data[0] != 'P' || m_data[1] != 'G')
        throw std::invalid_argument("Not a PGS segment (no PG magic)");
}

double pg_segment_c::pts() const noexcept {
    return static_cast<double>(tpts()) / PGS_FREQ;
}

void pg_segment_c::set_pts(double pts) noexcept {
    set_tpts(static_cast<uint32_t>(std::round(pts * PGS_FREQ)));
}

double pg_segment_c::dts() const noexcept {
    return static_cast<double>(tdts()) / PGS_FREQ;
}

void pg_segment_c::set_dts(double dts) noexcept {
    set_tdts(static_cast<uint32_t>(std::round(dts * PGS_FREQ)));
}

uint32_t pg_segment_c::tpts() const noexcept {
    return read_u32_be(&m_data[2]);
}

void pg_segment_c::set_tpts(uint32_t pts) noexcept {
    write_u32_be(&m_data[2], pts);
}

uint32_t pg_segment_c::tdts() const noexcept {
    return read_u32_be(&m_data[6]);
}

void pg_segment_c::set_tdts(uint32_t dts) noexcept {
    write_u32_be(&m_data[6], dts);
}

segment_type_e pg_segment_c::type() const noexcept {
    return segment_type_from_byte(m_data[10]);
}

uint16_t pg_segment_c::size() const noexcept {
    return read_u16_be(&m_data[11]);
}

std::vector<uint8_t> pg_segment_c::payload() const {
    if (m_data.size() <= PGS_HEADER_LEN) return {};
    return std::vector<uint8_t>(m_data.begin() + PGS_HEADER_LEN, m_data.end());
}

std::vector<uint8_t> pg_segment_c::to_bytes() const {
    return m_data;
}

void pg_segment_c::update() {
    update_length();
}

void pg_segment_c::update_length() {
    uint16_t len = static_cast<uint16_t>(m_data.size() - PGS_HEADER_LEN);
    write_u16_be(&m_data[11], len);
}

std::vector<uint8_t> pg_segment_c::make_header(segment_type_e type) {
    std::vector<uint8_t> hdr(PGS_HEADER_LEN, 0);
    hdr[0] = 'P'; hdr[1] = 'G';
    hdr[10] = static_cast<uint8_t>(type);
    return hdr;
}

// ── PCS ──
pcs_c::pcs_c(const std::vector<uint8_t>& data)
    : pg_segment_c(data)
{
    auto pl = payload();
    if (pl.size() < 11) return;
    uint8_t n = pl[10];
    size_t offset = 11;
    for (uint8_t i = 0; i < n; i++) {
        if (offset >= pl.size()) break;
        bool cropped = (pl[offset + 3] & c_object_t::cropped);
        size_t obj_size = cropped ? 16 : 8;
        if (offset + obj_size > pl.size()) break;
        cobjects.push_back(c_object_t::from_bytes(&pl[offset], cropped));
        offset += obj_size;
    }
}

uint16_t pcs_c::video_width() const noexcept {
    auto pl = payload();
    return read_u16_be(&pl[0]);
}
void pcs_c::set_video_width(uint16_t w) noexcept {
    auto pl = payload();
    write_u16_be(&m_data[PGS_HEADER_LEN], w);
}
uint16_t pcs_c::video_height() const noexcept {
    auto pl = payload();
    return read_u16_be(&pl[2]);
}
void pcs_c::set_video_height(uint16_t h) noexcept {
    write_u16_be(&m_data[PGS_HEADER_LEN + 2], h);
}
uint8_t pcs_c::fps() const noexcept { return m_data[PGS_HEADER_LEN + 4]; }
void pcs_c::set_fps(uint8_t f) noexcept { m_data[PGS_HEADER_LEN + 4] = f; }
uint16_t pcs_c::composition_n() const noexcept {
    return read_u16_be(&m_data[PGS_HEADER_LEN + 5]);
}
void pcs_c::set_composition_n(uint16_t n) noexcept {
    write_u16_be(&m_data[PGS_HEADER_LEN + 5], n);
}
pcs_c::composition_state_e pcs_c::composition_state() const noexcept {
    return static_cast<composition_state_e>(m_data[PGS_HEADER_LEN + 7]);
}
void pcs_c::set_composition_state(composition_state_e s) noexcept {
    m_data[PGS_HEADER_LEN + 7] = static_cast<uint8_t>(s);
}
bool pcs_c::pal_flag() const noexcept {
    return (m_data[PGS_HEADER_LEN + 8] & 0x80) != 0;
}
void pcs_c::set_pal_flag(bool f) noexcept {
    if (f) m_data[PGS_HEADER_LEN + 8] |= 0x80;
    else   m_data[PGS_HEADER_LEN + 8] &= static_cast<uint8_t>(0x7F);
}
uint8_t pcs_c::pal_id() const noexcept { return m_data[PGS_HEADER_LEN + 9]; }
void pcs_c::set_pal_id(uint8_t id) noexcept { m_data[PGS_HEADER_LEN + 9] = id; }
uint8_t pcs_c::n_objects() const noexcept { return m_data[PGS_HEADER_LEN + 10]; }

void pcs_c::update() {
    m_data[PGS_HEADER_LEN + 10] = static_cast<uint8_t>(cobjects.size());
    std::vector<uint8_t> new_pl = {m_data.begin() + PGS_HEADER_LEN,
                                    m_data.begin() + PGS_HEADER_LEN + 11};
    for (auto& obj : cobjects) {
        auto obj_bytes = obj.to_bytes();
        new_pl.insert(new_pl.end(), obj_bytes.begin(), obj_bytes.end());
    }
    m_data.resize(PGS_HEADER_LEN + new_pl.size());
    std::copy(new_pl.begin(), new_pl.end(), m_data.begin() + PGS_HEADER_LEN);
    update_length();
}

std::vector<uint8_t> pcs_c::to_bytes() const {
    // Force rebuild before returning
    const_cast<pcs_c*>(this)->update();
    return m_data;
}

pcs_c pcs_c::from_scratch(uint16_t width, uint16_t height, uint8_t fps,
                           uint16_t comp_n, composition_state_e state,
                           bool pal_flag, uint8_t pal_id,
                           const std::vector<c_object_t>& cobjects_,
                           double pts, double dts)
{
    std::vector<uint8_t> pl(11, 0);
    write_u16_be(&pl[0], width);
    write_u16_be(&pl[2], height);
    pl[4] = fps;
    write_u16_be(&pl[5], comp_n);
    pl[7] = static_cast<uint8_t>(state);
    pl[8] = pal_flag ? 0x80 : 0x00;
    pl[9] = pal_id;
    pl[10] = static_cast<uint8_t>(cobjects_.size());

    size_t off = 11;
    for (auto& obj : cobjects_) {
        auto obj_b = obj.to_bytes();
        pl.insert(pl.end(), obj_b.begin(), obj_b.end());
        off += obj_b.size();
    }

    auto hdr = make_header(segment_type_e::pcs);
    pcs_c seg;
    seg.m_data = hdr;
    seg.m_data.insert(seg.m_data.end(), pl.begin(), pl.end());
    seg.set_pts(pts);
    seg.set_dts(dts);
    seg.update_length();
    seg.cobjects = cobjects_;
    return seg;
}

// ── WDS ──
wds_c::wds_c(const std::vector<uint8_t>& data)
    : pg_segment_c(data)
{
    auto pl = payload();
    if (pl.empty()) return;
    uint8_t n = pl[0];
    for (uint8_t i = 0; i < n; i++) {
        auto off = static_cast<size_t>(1) + static_cast<size_t>(i) * 9;
        if (off + 9 > pl.size()) break;
        windows.push_back(window_definition_t::from_bytes(&pl[off]));
    }
}

uint8_t wds_c::n_windows() const noexcept {
    auto pl = payload();
    return pl.empty() ? 0 : pl[0];
}

void wds_c::update() {
    std::vector<uint8_t> new_pl;
    new_pl.push_back(static_cast<uint8_t>(windows.size()));
    for (auto& w : windows) {
        auto wb = w.to_bytes();
        new_pl.insert(new_pl.end(), wb.begin(), wb.end());
    }
    m_data.resize(PGS_HEADER_LEN + new_pl.size());
    std::copy(new_pl.begin(), new_pl.end(), m_data.begin() + PGS_HEADER_LEN);
    update_length();
}

std::vector<uint8_t> wds_c::to_bytes() const {
    const_cast<wds_c*>(this)->update();
    return m_data;
}

wds_c wds_c::from_scratch(const std::vector<window_definition_t>& windows_,
                           double pts, double dts)
{
    auto hdr = make_header(segment_type_e::wds);
    wds_c seg;
    seg.m_data = hdr;
    seg.m_data.push_back(static_cast<uint8_t>(windows_.size()));
    for (auto& w : windows_) {
        auto wb = w.to_bytes();
        seg.m_data.insert(seg.m_data.end(), wb.begin(), wb.end());
    }
    seg.set_pts(pts);
    seg.set_dts(dts);
    seg.windows = windows_;
    seg.update_length();
    return seg;
}

// ── PDS ──
pds_c::pds_c(const std::vector<uint8_t>& data)
    : pg_segment_c(data) {}

uint8_t pds_c::p_id() const noexcept { return m_data[PGS_HEADER_LEN]; }
void pds_c::set_p_id(uint8_t id) noexcept { m_data[PGS_HEADER_LEN] = id; }
uint8_t pds_c::p_vn() const noexcept { return m_data[PGS_HEADER_LEN + 1]; }
void pds_c::set_p_vn(uint8_t vn) noexcept { m_data[PGS_HEADER_LEN + 1] = vn; }

media::palette_t pds_c::to_palette() const {
    auto pl = payload();
    if (pl.size() < 3) return {};
    return media::palette_t::from_bytes(&pl[2], pl.size() - 2);
}

void pds_c::set_palette(const media::palette_t& pal) {
    auto pal_bytes = pal.to_bytes();
    m_data.resize(PGS_HEADER_LEN + 2 + pal_bytes.size());
    m_data[PGS_HEADER_LEN]     = p_id();
    m_data[PGS_HEADER_LEN + 1] = p_vn();
    std::copy(pal_bytes.begin(), pal_bytes.end(),
              m_data.begin() + PGS_HEADER_LEN + 2);
    update_length();
}

pds_c pds_c::from_scratch(const media::palette_t& palette,
                           uint8_t p_vn, uint8_t p_id,
                           double pts, double dts, int offset)
{
    auto hdr = make_header(segment_type_e::pds);
    pds_c seg;
    seg.m_data = hdr;
    seg.m_data.push_back(p_id);
    seg.m_data.push_back(p_vn);
    auto pal = palette;
    if (offset > 0) pal.offset(offset);
    auto pal_bytes = pal.to_bytes();
    seg.m_data.insert(seg.m_data.end(), pal_bytes.begin(), pal_bytes.end());
    seg.set_pts(pts);
    seg.set_dts(dts);
    seg.update_length();
    return seg;
}

// ── ODS ──
ods_c::ods_c(const std::vector<uint8_t>& data)
    : pg_segment_c(data) {}

uint16_t ods_c::o_id() const noexcept { return read_u16_be(&m_data[PGS_HEADER_LEN]); }
void ods_c::set_o_id(uint16_t id) noexcept { write_u16_be(&m_data[PGS_HEADER_LEN], id); }
uint8_t ods_c::o_vn() const noexcept { return m_data[PGS_HEADER_LEN + 2]; }
void ods_c::set_o_vn(uint8_t vn) noexcept { m_data[PGS_HEADER_LEN + 2] = vn; }
ods_c::sequence_flags_e ods_c::seq_flags() const noexcept {
    return static_cast<sequence_flags_e>(m_data[PGS_HEADER_LEN + 3]);
}
void ods_c::set_seq_flags(sequence_flags_e f) noexcept {
    m_data[PGS_HEADER_LEN + 3] = static_cast<uint8_t>(f);
}

uint32_t ods_c::rle_len() const {
    return read_u24_be(&m_data[PGS_HEADER_LEN + 4]);
}

void ods_c::set_rle_len(uint32_t len) {
    write_u24_be(&m_data[PGS_HEADER_LEN + 4], len);
}

uint16_t ods_c::width() const {
    return read_u16_be(&m_data[PGS_HEADER_LEN + 7]);
}

void ods_c::set_width(uint16_t w) {
    write_u16_be(&m_data[PGS_HEADER_LEN + 7], w);
}

uint16_t ods_c::height() const {
    return read_u16_be(&m_data[PGS_HEADER_LEN + 9]);
}

void ods_c::set_height(uint16_t h) {
    write_u16_be(&m_data[PGS_HEADER_LEN + 9], h);
}

std::vector<uint8_t> ods_c::data() const {
    auto pl = payload();
    size_t offset = (seq_flags() == sequence_flags_e::first ||
                     seq_flags() == sequence_flags_e::single) ? 11 : 4;
    if (offset >= pl.size()) return {};
    return std::vector<uint8_t>(pl.begin() + static_cast<ptrdiff_t>(offset), pl.end());
}

void ods_c::set_data(const std::vector<uint8_t>& d) {
    auto pl = payload();
    bool is_first = (seq_flags() == sequence_flags_e::first ||
                     seq_flags() == sequence_flags_e::single);
    size_t hdr_sz = is_first ? 11 : 4;
    m_data.resize(PGS_HEADER_LEN + hdr_sz + d.size());
    std::copy(d.begin(), d.end(), m_data.begin() + static_cast<ptrdiff_t>(PGS_HEADER_LEN + hdr_sz));
    update_length();
}

std::vector<ods_c> ods_c::from_scratch(uint16_t o_id, uint8_t o_vn,
                                         uint16_t width, uint16_t height,
                                         const std::vector<uint8_t>& rle_data,
                                         double pts, double dts)
{
    constexpr uint32_t MAX_FIRST  = 0xFFE4;
    constexpr uint32_t MAX_OTHERS = 0xFFEB;

    auto make_first = [&](bool is_last) -> ods_c {
        auto hdr = make_header(segment_type_e::ods);
        ods_c seg;
        seg.m_data = hdr;
        seg.m_data.resize(PGS_HEADER_LEN + 11, 0);
        seg.set_o_id(o_id);
        seg.set_o_vn(o_vn);
        seg.set_seq_flags(is_last ? sequence_flags_e::single
                                   : sequence_flags_e::first);
        seg.set_width(width);
        seg.set_height(height);
        seg.set_pts(pts);
        seg.set_dts(dts);
        return seg;
    };

    auto make_next = [&](bool is_last) -> ods_c {
        auto hdr = make_header(segment_type_e::ods);
        ods_c seg;
        seg.m_data = hdr;
        seg.m_data.resize(PGS_HEADER_LEN + 4, 0);
        seg.set_o_id(o_id);
        seg.set_o_vn(o_vn);
        seg.set_seq_flags(is_last ? sequence_flags_e::last
                                   : sequence_flags_e::middle);
        seg.set_pts(pts);
        seg.set_dts(dts);
        return seg;
    };

    std::vector<ods_c> result;

    if (rle_data.size() <= MAX_FIRST) {
        auto seg = make_first(true);
        seg.set_rle_len(static_cast<uint32_t>(rle_data.size() + 4));
        auto pl = seg.payload();
        auto d_pl = seg.data();
        seg.set_data(rle_data);
        result.push_back(std::move(seg));
    } else {
        auto first = make_first(false);
        first.set_rle_len(static_cast<uint32_t>(rle_data.size() + 4));
        first.set_data(std::vector<uint8_t>(rle_data.begin(),
                                             rle_data.begin() + MAX_FIRST));
        result.push_back(std::move(first));

        size_t remaining = rle_data.size() - MAX_FIRST;
        size_t offset = MAX_FIRST;
        while (remaining > 0) {
            size_t chunk = (remaining <= MAX_OTHERS) ? remaining : MAX_OTHERS;
            bool is_last = (remaining <= MAX_OTHERS);
            auto seg = make_next(is_last);
            seg.set_data(std::vector<uint8_t>(rle_data.begin() + static_cast<ptrdiff_t>(offset),
                                                   rle_data.begin() + static_cast<ptrdiff_t>(offset + chunk)));
            result.push_back(std::move(seg));
            offset += chunk;
            remaining -= chunk;
        }
    }
    return result;
}

// ── ENDS ──
ends_c::ends_c(const std::vector<uint8_t>& data)
    : pg_segment_c(data) {}

ends_c ends_c::from_scratch(double pts, double dts) {
    auto hdr = make_header(segment_type_e::end);
    ends_c seg;
    seg.m_data = std::move(hdr);
    seg.set_pts(pts);
    seg.set_dts(dts);
    seg.update_length();
    return seg;
}

// ── Display Set ──
display_set_t::display_set_t(std::vector<std::shared_ptr<pg_segment_c>> segs)
    : segments(std::move(segs)) {}

void display_set_t::update() {
    // Auto-set END pts to match last real segment
    for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
        auto& seg = *it;
        if (std::dynamic_pointer_cast<ends_c>(seg)) continue;
        if (segments.back()->type() != segment_type_e::end) break;
        segments.back()->set_pts(seg->pts());
        segments.back()->set_dts(seg->dts());
        break;
    }
}

double display_set_t::t_in() const noexcept {
    if (segments.empty()) return 0.0;
    return segments[0]->pts();
}

double display_set_t::t_out() const noexcept {
    if (segments.empty()) return 0.0;
    return segments.back()->pts();
}

double display_set_t::pts() const noexcept {
    return t_in();
}

void display_set_t::set_pts(double new_pts) noexcept {
    for (auto& seg : segments)
        seg->set_pts(new_pts);
}

pcs_c& display_set_t::pcs() {
    for (auto& seg : segments) {
        if (auto p = std::dynamic_pointer_cast<pcs_c>(seg))
            return *p;
    }
    throw std::runtime_error("No PCS in display set");
}

std::optional<std::reference_wrapper<wds_c>> display_set_t::wds() {
    for (auto& seg : segments) {
        if (auto p = std::dynamic_pointer_cast<wds_c>(seg))
            return *p;
    }
    return std::nullopt;
}

std::vector<std::shared_ptr<pds_c>> display_set_t::pds() const {
    std::vector<std::shared_ptr<pds_c>> result;
    for (auto& seg : segments) {
        if (auto p = std::dynamic_pointer_cast<pds_c>(seg))
            result.push_back(p);
    }
    return result;
}

std::vector<std::shared_ptr<ods_c>> display_set_t::ods() const {
    std::vector<std::shared_ptr<ods_c>> result;
    for (auto& seg : segments) {
        if (auto p = std::dynamic_pointer_cast<ods_c>(seg))
            result.push_back(p);
    }
    return result;
}

ends_c& display_set_t::end() {
    for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
        if (auto p = std::dynamic_pointer_cast<ends_c>(*it))
            return *p;
    }
    throw std::runtime_error("No ENDS in display set");
}

display_set_t display_set_t::from_bytes(const std::vector<uint8_t>& data) {
    // naive single-pass splitter, works for valid streams
    std::vector<uint8_t> remaining = data;
    std::vector<std::shared_ptr<pg_segment_c>> segs;

    while (!remaining.empty()) {
        if (remaining.size() < PGS_HEADER_LEN) break;
        uint16_t seg_len = read_u16_be(&remaining[11]);
        size_t total = PGS_HEADER_LEN + seg_len;
        if (total > remaining.size()) break;

        std::vector<uint8_t> seg_data(remaining.begin(), remaining.begin() + static_cast<ptrdiff_t>(total));
        uint8_t type_byte = remaining[10];

        std::shared_ptr<pg_segment_c> seg;
        switch (type_byte) {
            case 0x16: seg = std::make_shared<pcs_c>(seg_data); break;
            case 0x17: seg = std::make_shared<wds_c>(seg_data); break;
            case 0x14: seg = std::make_shared<pds_c>(seg_data); break;
            case 0x15: seg = std::make_shared<ods_c>(seg_data); break;
            case 0x80: seg = std::make_shared<ends_c>(seg_data); break;
            default:   seg = std::make_shared<pg_segment_c>(seg_data); break;
        }
        segs.push_back(seg);
        remaining.erase(remaining.begin(), remaining.begin() + static_cast<ptrdiff_t>(total));
    }
    return display_set_t(segs);
}

} // namespace core
} // namespace opensup
