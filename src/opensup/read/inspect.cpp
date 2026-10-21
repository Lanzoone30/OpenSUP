// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/read/inspect.h"

#include "opensup/common/bdvideo.h"
#include "opensup/core/pgstream.h"
#include "opensup/core/segments.h"
#include "opensup/media/pgraphics.h"
#include "opensup/media/rle_decode.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>

namespace opensup {
namespace read {

using core::display_set_t;
using core::ends_c;
using core::ods_c;
using core::pcs_c;
using core::pds_c;
using core::pg_segment_c;
using core::segment_type_e;
using core::wds_c;

namespace {

constexpr uint32_t PTS_MASK = 0xFFFFFFFFu;
constexpr uint32_t PTS_DIFF_BOUND = PTS_MASK >> 1;

/// Modular difference on 32-bit timestamps (mirrors the oracle's `& PTS_MASK`).
uint32_t md(int64_t a, int64_t b) noexcept
{
    return static_cast<uint32_t>((a - b) & 0xFFFFFFFFll);
}

/// FNV-1a over an ODS payload; only equality with a previous hash matters.
size_t fnv1a(const std::vector<uint8_t>& data) noexcept
{
    size_t h = 1469598103934665603ull;
    for (uint8_t b : data) h = (h ^ b) * 1099511628211ull;
    return h;
}

pcs_c* pcs_of(display_set_t& ds) noexcept
{
    for (auto& seg : ds.segments)
        if (seg->type() == segment_type_e::pcs)
            return static_cast<pcs_c*>(seg.get());
    return nullptr;
}

const pcs_c* pcs_of(const display_set_t& ds) noexcept
{
    for (auto& seg : ds.segments)
        if (seg->type() == segment_type_e::pcs)
            return static_cast<const pcs_c*>(seg.get());
    return nullptr;
}

uint8_t state_of(const display_set_t& ds) noexcept
{
    const auto* pcs = pcs_of(ds);
    return pcs ? static_cast<uint8_t>(pcs->composition_state()) : 0;
}

bool has_type(const display_set_t& ds, segment_type_e t) noexcept
{
    for (auto& seg : ds.segments)
        if (seg->type() == t) return true;
    return false;
}

int index_of(const display_set_t& ds, segment_type_e t) noexcept
{
    for (size_t k = 0; k < ds.segments.size(); ++k)
        if (ds.segments[k]->type() == t) return static_cast<int>(k);
    return -1;
}

wds_c* wds_of(display_set_t& ds) noexcept
{
    for (auto& seg : ds.segments)
        if (seg->type() == segment_type_e::wds)
            return static_cast<wds_c*>(seg.get());
    return nullptr;
}

const wds_c* wds_of(const display_set_t& ds) noexcept
{
    for (auto& seg : ds.segments)
        if (seg->type() == segment_type_e::wds)
            return static_cast<const wds_c*>(seg.get());
    return nullptr;
}

/// Window decode time of a WDS, in 90 kHz ticks (oracle: sum ceil(w*h*FREQ/RC)).
uint32_t window_ticks(const wds_c* wds) noexcept
{
    if (!wds) return 0;
    uint32_t sum = 0;
    for (auto& w : wds->windows)
        sum += static_cast<uint32_t>(std::ceil(static_cast<double>(w.width) * w.height *
                                                media::pg_decoder_t::FREQ /
                                                media::pg_decoder_t::RC));
    return sum;
}

/// Split a stream into epochs by display-set index.
std::vector<std::vector<size_t>> split_epochs(const sup_stream_t& stream)
{
    std::vector<std::vector<size_t>> epochs;
    const size_t n = stream.display_sets.size();
    if (n == 0) return epochs;

    std::vector<size_t> starts = stream.epoch_starts;
    if (starts.empty() || starts.front() != 0) starts.insert(starts.begin(), 0);

    for (size_t e = 0; e < starts.size(); ++e) {
        const size_t begin = starts[e];
        const size_t end = (e + 1 < starts.size()) ? starts[e + 1] : n;
        std::vector<size_t> idx;
        for (size_t k = begin; k < end; ++k) idx.push_back(k);
        epochs.push_back(std::move(idx));
    }
    return epochs;
}

}  // namespace

stream_stats_t stream_stats(const sup_stream_t& stream)
{
    stream_stats_t st;
    st.epochs = static_cast<int>(stream.epochs());
    st.display_sets = static_cast<int>(stream.display_sets.size());

    auto epochs = split_epochs(stream);
    std::map<size_t, uint32_t> epoch_delta;  // ds index -> epoch window decode time
    for (auto& ep : epochs) {
        if (ep.empty()) continue;
        const uint32_t ticks = window_ticks(wds_of(stream.display_sets[ep[0]]));
        for (size_t i : ep) epoch_delta[i] = ticks;
    }

    for (size_t k = 0; k < stream.display_sets.size(); ++k) {
        const auto& ds = stream.display_sets[k];
        const uint8_t state = state_of(ds);
        if (state == 0) ++st.nc;
        else if (state & 0x40) ++st.acq;
        else if (state & 0x80) ++st.es;

        const auto* pcs = pcs_of(ds);
        const bool pal_flag = pcs && pcs->pal_flag();
        if (pal_flag) {
            ++st.pu;
            const uint32_t delta = epoch_delta.count(k) ? epoch_delta[k] : 0;
            if (pcs && md(pcs->tpts(), pcs->tdts()) > delta + 1) ++st.pu_buffered;
        } else if (has_type(ds, segment_type_e::wds) && state == 0 && has_type(ds, segment_type_e::ods)) {
            ++st.nc_ods;
        }

        for (auto& seg : ds.segments) {
            ++st.segments;
            st.bytes += core::PGS_HEADER_LEN + seg->size();
            switch (seg->type()) {
                case segment_type_e::pcs: ++st.pcs; break;
                case segment_type_e::wds: ++st.wds; break;
                case segment_type_e::pds: ++st.pds; break;
                case segment_type_e::ods: ++st.ods; break;
                case segment_type_e::end: ++st.ends; break;
            }
        }
    }
    return st;
}

validate_report_t validate(sup_stream_t& stream, int max_kbps)
{
    validate_report_t rep;
    rep.max_kbps = max_kbps;
    rep.stats = stream_stats(stream);

    auto add = [&rep](size_t ds, bool hard, const char* rule, std::string detail) {
        if (hard) rep.compliant = false;
        rep.findings.push_back({ds, rule, std::move(detail), hard});
    };

    for (auto& e : stream.errors) add(0, true, "stream", e);
    if (stream.trailing_bytes)
        add(0, true, "stream",
            "trailing bytes after the last segment: " + std::to_string(stream.trailing_bytes));

    auto epochs = split_epochs(stream);
    int64_t prev_pts = -99999999, prev_dts = -99999999;
    uint16_t prev_pcs_id = 0xFFFF; ///< composition number of the previous display set

    for (auto& ep : epochs) {
        if (ep.empty()) continue;
        const size_t first = ep[0];
        auto& first_ds = stream.display_sets[first];
        auto* fpcs = pcs_of(first_ds);
        if (!fpcs) continue;

        // The PCS stores an fps *code* (0x10 = 23.976 ...); frame_duration needs
        // the real rate, exactly as the oracle receives the BDN fps.
        const double fps = common::fps_e::from_pcsfps(static_cast<int32_t>(fpcs->fps())).to_double();
        const uint32_t frame_duration = static_cast<uint32_t>(std::floor(media::pg_decoder_t::FREQ / fps));

        // --- epoch level (oracle is_compliant / check_pts_dts_sanity) --------
        if ((static_cast<uint8_t>(fpcs->composition_state()) & 0x80) == 0)
            add(first, true, "epoch/start", "first display set of the epoch is not an Epoch Start");

        auto* fwds = wds_of(first_ds);
        if (fwds) {
            for (auto& w : fwds->windows) {
                if (static_cast<uint32_t>(w.h_pos) + w.width > fpcs->video_width() ||
                    static_cast<uint32_t>(w.v_pos) + w.height > fpcs->video_height())
                    add(first, true, "epoch/window",
                        "window out of screen: id=" + std::to_string(w.window_id));
            }
            if (fwds->windows.size() == 2) {
                const auto& a = fwds->windows[0];
                const auto& b = fwds->windows[1];
                const int x0 = std::max<int>(a.h_pos, b.h_pos);
                const int x1 = std::min<int>(a.h_pos + a.width, b.h_pos + b.width);
                const int y0 = std::max<int>(a.v_pos, b.v_pos);
                const int y1 = std::min<int>(a.v_pos + a.height, b.v_pos + b.height);
                if (x1 > x0 && y1 > y0) add(first, true, "epoch/window", "overlapping windows in epoch");
            }
        }

        const uint32_t pts_delta = window_ticks(fwds);
        const uint32_t wipe_duration = static_cast<uint32_t>(
            std::ceil(static_cast<double>(fpcs->video_width()) * fpcs->video_height() *
                      media::pg_decoder_t::FREQ / media::pg_decoder_t::RC));

        const uint32_t es_diff = md(fpcs->tdts(), prev_pts);
        if (!(es_diff > 0 && es_diff < PTS_DIFF_BOUND))
            add(first, true, "pts_dts/epoch_start",
                "epoch start DTS does not follow the previous epoch PTS");
        if (!(md(fpcs->tpts(), fpcs->tdts()) > wipe_duration))
            add(first, true, "pts_dts/epoch_start",
                "epoch start PTS-DTS delta does not exceed the wipe duration");

        std::map<uint8_t, uint8_t> pds_vn;
        std::map<uint8_t, media::palette_t> pals;
        std::map<uint16_t, std::pair<uint16_t, uint16_t>> slots; ///< o_id -> (w, h)
        size_t slot_bytes = 0;
        std::set<uint16_t> ods_filled;
        std::map<uint16_t, uint8_t> ods_vn;
        std::map<uint16_t, size_t> ods_hash;
        std::vector<uint8_t> chain; ///< concatenated ODS payloads of one object
        uint32_t cumulated = 0;     ///< coded bytes in the current ODS chain
        uint16_t chain_w = 0, chain_h = 0, chain_id = 0;
        uint8_t chain_vn = 0;
        bool have_window = false;
        std::map<uint8_t, std::array<uint16_t, 4>> window_shape;

        for (size_t kd = 0; kd < ep.size(); ++kd) {
            auto& ds = stream.display_sets[ep[kd]];
            auto* pcs = pcs_of(ds);
            if (!pcs) {
                add(ep[kd], true, "test_diplayset/pcs", "display set has no PCS");
                continue;
            }
            const uint8_t state = static_cast<uint8_t>(pcs->composition_state());

            // --- test_diplayset -------------------------------------------------
            if (ds.segments.empty() || ds.segments[0]->type() != segment_type_e::pcs)
                add(ep[kd], true, "test_diplayset/order", "PCS is not the first segment");
            if (pcs->n_objects() > 2 || pcs->n_objects() != pcs->cobjects.size())
                add(ep[kd], true, "test_diplayset/objects",
                    "PCS object count " + std::to_string(pcs->n_objects()) +
                        " does not match the object list (" + std::to_string(pcs->cobjects.size()) + ")");

            const int wds_idx = index_of(ds, segment_type_e::wds);
            if (state != 0) {
                if (pcs->pal_flag())
                    add(ep[kd], true, "test_diplayset/pal_flag", "palette update on a non-normal display set");
                if (wds_idx < 0)
                    add(ep[kd], true, "test_diplayset/wds", "non-normal display set without a WDS");
            }
            if (pcs->pal_id() >= 8) add(ep[kd], true, "test_diplayset/pal_id", "PCS palette id >= 8");
            if (wds_idx >= 0) {
                if (wds_idx != 1)
                    add(ep[kd], true, "test_diplayset/order", "WDS is not the second segment");
                if (pcs->pal_flag())
                    add(ep[kd], true, "test_diplayset/pal_flag", "windows present on a palette update");
                auto* wds = static_cast<wds_c*>(ds.segments[static_cast<size_t>(wds_idx)].get());
                if (wds->windows.empty() || wds->windows.size() > 2)
                    add(ep[kd], true, "test_diplayset/wds",
                        "unsupported window count " + std::to_string(wds->windows.size()));
            }
            if (ds.segments.empty() || ds.segments.back()->type() != segment_type_e::end)
                add(ep[kd], true, "test_diplayset/end", "ENDS is missing or not the last segment");

            // --- segments in stream order (oracle is_compliant) -------------------
            if (state != 0) { // acquisition: past objects/palettes must not be accessed
                ods_filled.clear();
                pals.clear();
            }
            std::set<uint8_t> seen_pds;

            for (auto& seg : ds.segments) {
                switch (seg->type()) {
                    case segment_type_e::pcs: {
                        if (pcs->composition_n() != ((prev_pcs_id + 1) & 0xFFFF) &&
                            pcs->composition_n() != prev_pcs_id && state != 0x80)
                            add(ep[kd], false, "test_diplayset/comp_n",
                                "composition number does not increment by one");
                        prev_pcs_id = pcs->composition_n();
                        break;
                    }
                    case segment_type_e::wds: {
                        auto* wds = static_cast<wds_c*>(seg.get());
                        if (!have_window) {
                            have_window = true;
                            for (auto& w : wds->windows)
                                window_shape[w.window_id] = {w.h_pos, w.v_pos, w.width, w.height};
                        } else {
                            for (auto& w : wds->windows) {
                                auto it = window_shape.find(w.window_id);
                                if (it != window_shape.end() &&
                                    it->second != std::array<uint16_t, 4>{w.h_pos, w.v_pos,
                                                                          w.width, w.height})
                                    add(ep[kd], true, "epoch/window",
                                        "window " + std::to_string(w.window_id) +
                                            " changed mid-epoch");
                            }
                        }
                        break;
                    }
                    case segment_type_e::pds: {
                        auto* pds = static_cast<pds_c*>(seg.get());
                        if (!seen_pds.insert(pds->p_id()).second)
                            add(ep[kd], true, "test_diplayset/pds",
                                "duplicate PDS palette id " + std::to_string(pds->p_id()));
                        if (pds->to_palette().size() > 256)
                            add(ep[kd], true, "test_diplayset/pds",
                                "palette defines more than 256 entries");
                        const media::palette_t delta = pds->to_palette();
                        media::palette_t& acc = pals[pds->p_id()];
                        for (auto& e : delta.entries()) acc.set(e.first, e.second);
                        const auto warn = pds_c::validate_palette(acc);
                        if (warn.out_of_range)
                            add(ep[kd], true, "palette/range",
                                "palette " + std::to_string(pds->p_id()) + " is not limited range");
                        if (warn.reserved_alpha)
                            add(ep[kd], true, "palette/reserved",
                                "palette entry 0xFF is set and not transparent");
                        if (pds_vn.count(pds->p_id()) &&
                            ((pds_vn[pds->p_id()] + 1) & 0xFF) != pds->p_vn())
                            add(ep[kd], false, "palette/version",
                                "palette version not incremented by one");
                        pds_vn[pds->p_id()] = pds->p_vn();
                        break;
                    }
                    case segment_type_e::ods: {
                        auto* ods = static_cast<ods_c*>(seg.get());
                        // Flags are bit fields: a single ODS is FIRST|LAST (0xC0).
                        const uint8_t sflags = static_cast<uint8_t>(ods->seq_flags());
                        const bool is_first = (sflags & 0x80) != 0;
                        const bool is_last = (sflags & 0x40) != 0;
                        const uint16_t w = ods->width(), h = ods->height();
                        if (is_first) {
                            chain.clear();
                            chain_w = w;
                            chain_h = h;
                            chain_id = ods->o_id();
                            chain_vn = ods->o_vn();
                            if (w < 8 || h < 8 || w > 4096 || h > 4096) {
                                add(ep[kd], true, "object/dimensions",
                                    "illegal object dimensions " + std::to_string(w) + "x" +
                                        std::to_string(h));
                                break;
                            }
                            auto it = slots.find(ods->o_id());
                            if (it == slots.end()) {
                                if (slots.size() >= 64 ||
                                    slot_bytes + static_cast<size_t>(w) * h >
                                        media::pg_decoder_t::DECODED_BUF_SIZE)
                                    add(ep[kd], true, "object/buffer",
                                        "object buffer overflow: no memory for slot " +
                                            std::to_string(ods->o_id()));
                                else {
                                    slots[ods->o_id()] = {w, h};
                                    slot_bytes += static_cast<size_t>(w) * h;
                                }
                            } else if (it->second != std::make_pair(w, h)) {
                                add(ep[kd], true, "object/slot",
                                    "object-slot " + std::to_string(ods->o_id()) +
                                        " dimensions mismatch");
                            }
                            if (cumulated > 0)
                                add(ep[kd], true, "object/chain",
                                    "a past ODS was not properly terminated");
                        } else if (chain_id != ods->o_id() || chain_vn != ods->o_vn()) {
                            add(ep[kd], true, "object/chain",
                                "object definition header mismatch in a chain");
                        }

                        cumulated += static_cast<uint32_t>(core::PGS_HEADER_LEN + seg->size());
                        const auto data = ods->data();
                        chain.insert(chain.end(), data.begin(), data.end());

                        if (is_last) {
                            if (cumulated >= media::pg_decoder_t::CODED_BUF_SIZE - 16 * 9)
                                add(ep[kd], true, "object/coded_size",
                                    "coded object size exceeds the 1 MiB buffer");
                            cumulated = 0;

                            const size_t hsh = fnv1a(chain);
                            auto prev = ods_hash.find(ods->o_id());
                            if (ods_filled.count(ods->o_id()) && prev != ods_hash.end() &&
                                prev->second != hsh && ods_vn.count(ods->o_id()) &&
                                ods_vn[ods->o_id()] == ods->o_vn())
                                add(ep[kd], false, "object/version",
                                    "object " + std::to_string(ods->o_id()) +
                                        " changed without incrementing its version");
                            ods_filled.insert(ods->o_id());
                            ods_vn[ods->o_id()] = ods->o_vn();
                            ods_hash[ods->o_id()] = hsh;

                            bool rle_ok = true;
                            const auto bitmap = media::decode_rle_checked(chain, chain_w, chain_h, rle_ok);
                            if (!rle_ok)
                                add(ep[kd], false, "object/rle",
                                    "object " + std::to_string(ods->o_id()) +
                                        " has too long RLE line(s)");
                            else {
                                const media::palette_t& pal = pals[pcs->pal_id()];
                                for (uint8_t px : bitmap) {
                                    if (px != 0xFF && !pal.has(px)) {
                                        add(ep[kd], false, "object/palette",
                                            "object " + std::to_string(ods->o_id()) +
                                                " uses undefined palette entry");
                                        break;
                                    }
                                }
                            }
                        }
                        break;
                    }
                    case segment_type_e::end: break;
                }
            }

            // --- composition objects (oracle placement/expiry rules) --------------
            if (auto* wds = wds_of(ds)) {
                for (auto& cobj : pcs->cobjects) {
                    auto sit = slots.find(cobj.o_id);
                    if (sit == slots.end()) {
                        add(ep[kd], true, "object/slot",
                            "composition object " + std::to_string(cobj.o_id) +
                                " uses an unknown buffer slot");
                        continue;
                    }
                    if (!ods_filled.count(cobj.o_id)) {
                        add(ep[kd], true, "object/expired",
                            "object " + std::to_string(cobj.o_id) + " uses expired memory");
                        continue;
                    }
                    uint16_t w = sit->second.first, h = sit->second.second;
                    if (cobj.is_cropped()) {
                        if (h < cobj.c_h || w < cobj.c_w ||
                            h < static_cast<uint32_t>(cobj.c_h) + cobj.vc_pos ||
                            w < static_cast<uint32_t>(cobj.c_w) + cobj.hc_pos) {
                            add(ep[kd], true, "object/crop",
                                "cropped dimension exceeds object " + std::to_string(cobj.o_id));
                            continue;
                        }
                        w = cobj.c_w;
                        h = cobj.c_h;
                        if (w == 0 || h == 0)
                            add(ep[kd], false, "object/crop", "zero cropping width or height");
                    }
                    const core::window_definition_t* wd = nullptr;
                    for (auto& w2 : wds->windows)
                        if (w2.window_id == cobj.window_id) wd = &w2;
                    if (wd &&
                        (cobj.h_pos < wd->h_pos ||
                         static_cast<uint32_t>(cobj.h_pos) + w >
                             static_cast<uint32_t>(wd->h_pos) + wd->width ||
                         cobj.v_pos < wd->v_pos ||
                         static_cast<uint32_t>(cobj.v_pos) + h >
                             static_cast<uint32_t>(wd->v_pos) + wd->height))
                        add(ep[kd], true, "object/window",
                            "composition object " + std::to_string(cobj.o_id) +
                                " misplaced outside window " + std::to_string(cobj.window_id));
                }
            }

            // --- composition number / duplicate PTS (soft) -----------------------
            if (kd > 0) {
                if ((state & 0x80) != 0)
                    add(ep[kd], true, "epoch/start", "epoch start in the middle of an epoch");
                auto* prev_pcs = pcs_of(stream.display_sets[ep[kd - 1]]);
                if (prev_pcs && prev_pcs->tpts() == pcs->tpts())
                    add(ep[kd], false, "test_diplayset/pts", "two display sets share the same PTS");
            }
            prev_pcs_id = pcs->composition_n();

            // --- check_pts_dts_sanity --------------------------------------------
            const int64_t ds_pts = pcs->tpts();
            const int64_t ds_dts = pcs->tdts();
            bool ds_comply = md(ds_dts, prev_dts) <= PTS_DIFF_BOUND &&
                             md(ds_pts, prev_pts) <= PTS_DIFF_BOUND;
            if (auto* wds = wds_of(ds)) {
                const uint32_t diff = md(ds_pts, prev_pts);
                ds_comply &= diff > pts_delta && diff < PTS_DIFF_BOUND;
                ds_comply &= md(ds_pts, wds->tpts()) <= pts_delta;
                ds_comply &= md(wds->tpts(), wds->tdts()) <= wipe_duration * 2;
                ds_comply &= wds->tpts() != wds->tdts();
            } else {
                ds_comply &= md(ds_pts, prev_pts) >= frame_duration;
            }
            for (auto& pds : ds.pds()) ds_comply &= pds->tpts() == pds->tdts();
            for (auto& ods : ds.ods()) ds_comply &= ods->tpts() != ods->tdts();
            for (auto& seg : ds.segments) {
                ds_comply &= md(ds_pts, seg->tpts()) <= PTS_DIFF_BOUND;
                ds_comply &= md(seg->tdts(), ds_dts) <= PTS_DIFF_BOUND;
                ds_comply &= md(seg->tpts(), ds_dts) <= PTS_DIFF_BOUND;
                ds_comply &= md(seg->tpts(), seg->tdts()) <= PTS_DIFF_BOUND;
                const uint32_t diff = md(seg->tdts(), prev_dts);
                ds_comply &= diff < PTS_DIFF_BOUND;
                ds_comply &= md(seg->tpts(), seg->tdts()) < static_cast<uint32_t>(media::pg_decoder_t::FREQ);
                prev_dts = seg->tdts();
            }
            prev_pts = ds_pts;
            if (!ds_comply)
                add(ep[kd], true, "pts_dts/sanity",
                    "display set timestamps are out of spec (state=" +
                        std::to_string(state) + ")");
        }
    }

    if (max_kbps > 0) {
        std::vector<std::shared_ptr<pg_segment_c>> segments;
        for (auto& ds : stream.display_sets)
            for (auto& seg : ds.segments) segments.push_back(seg);
        if (!test_rx_bitrate(segments, static_cast<int64_t>(max_kbps) * 1000 / 8))
            add(0, true, "rx_bitrate",
                "decoder buffer underflow at " + std::to_string(max_kbps) + " Kbps");
    }

    return rep;
}

}
}
