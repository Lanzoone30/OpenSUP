// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.
#include "opensup/pch.h"
#include "opensup/core/render/epoch_encoder.h"
#include "opensup/media/pgraphics.h"
#include "opensup/common/logger.h"
#include <algorithm>
#include <cmath>

namespace opensup {
namespace core {
using common::logger_c;
using common::fps_e;
using media::pg_decoder_t;
/// Defined in encode_epoch.cpp.
void trim_transparent_padding(const std::vector<uint8_t>& rgba, int width, int height,
                              std::vector<uint8_t>& out, int& out_w, int& out_h,
                              int& offset_x, int& offset_y);
// Overlap pipeline: active only when m_overlap && m_windows.size() == 2.

void epoch_encoder_c::shift_forward_overlay(
    std::vector<epoch_node_t>& nodes,
    const std::vector<bool>& forced_acq) const
{
    (void)forced_acq;
    // shift_forward_overlay: backtracks from an acquisition node with exactly
    // one new object (sum(new_mask)==1) and merges it into the best previous
    // node of the other window, so the object decodes earlier and the
    // intermediate range stays NORMAL.
    for (size_t k = 0; k < nodes.size(); ++k) {
        auto& node = nodes[k];
        if (node.acq || !node.has_object || !node.new_mask || !node.absolute)
            continue;
        const int future_wid = node.window_id;
        size_t drop_pal_ups_def = 0;
        bool drop_abs_acq_def = false;
        int j = static_cast<int>(k);
        while (j > 0) {
            --j;
            if (nodes[static_cast<size_t>(j)].dts_end < node.timings.base_dts &&
                nodes[static_cast<size_t>(j)].timings.base_pts + 1.0 / m_fps < node.timings.base_pts)
                break;
            drop_abs_acq_def |= nodes[static_cast<size_t>(j)].absolute;
            if (!m_overlap && nodes[static_cast<size_t>(j)].nc_refresh)
                ++drop_pal_ups_def;
        }
        size_t other_new_mask = 0;
        struct cand_t { size_t best_pk; size_t jk; size_t drop_pal_ups; };
        std::vector<cand_t> candidates;
        for (size_t pk = 1; pk <= 15 && k >= pk; ++pk) {
            const size_t pj = k - pk;
            auto& pnode = nodes[pj];
            if (!pnode.has_object)
                continue;
            const bool redefine_same_object =
                pnode.new_mask && pnode.window_id == future_wid;
            const bool overlap_in_window =
                pnode.has_object && pnode.window_id == future_wid;
            if (pnode.window_id != future_wid && pnode.new_mask)
                ++other_new_mask;
            if (redefine_same_object || overlap_in_window || other_new_mask > 1)
                break;
            // The candidate is a copy of pnode, so the scoring gap is measured
            // against the PROMOTED node's timings, not the original node's.
            bool drop_abs_acq = false;
            size_t drop_pal_ups = 0;
            size_t jk = 0;
            int jj = static_cast<int>(pj);
            while (jj >= 0) {
                if (jj == static_cast<int>(pj)) { --jj; continue; }
                if (jj < 0) break;
                if (nodes[static_cast<size_t>(jj)].dts_end < pnode.timings.base_dts &&
                    nodes[static_cast<size_t>(jj)].timings.base_pts + 1.0 / m_fps < pnode.timings.base_pts) {
                    jk = static_cast<size_t>(jj) + 1;
                    break;
                }
                drop_abs_acq |= nodes[static_cast<size_t>(jj)].absolute;
                if (!m_overlap && nodes[static_cast<size_t>(jj)].nc_refresh)
                    ++drop_pal_ups;
                if (jj == 0) { jk = 0; break; }
                --jj;
            }
            if (drop_abs_acq)
                continue;
            // The promoted node must fit after the scoring gap: the node right
            // before jk must not collide with the promoted node's decode
            // (nodes[j].dts_end() < new_node.dts() and
            // nodes[j].pts()+pts_delta < new_node.pts()).
            if (jk == 0 || (nodes[jk - 1].dts_end < pnode.timings.base_dts &&
                            nodes[jk - 1].timings.base_pts + 1.0 / m_fps < pnode.timings.base_pts)) {
                candidates.push_back({pk, jk, drop_pal_ups});
                // Quick exit: first candidate in overlap mode.
                if (drop_pal_ups == 0 || m_overlap)
                    break;
            }
        }
        if (candidates.empty())
            continue;
        auto best_it = std::min_element(
            candidates.begin(), candidates.end(),
            [k](const cand_t& a, const cand_t& b) {
                const double sa = static_cast<double>(a.drop_pal_ups) +
                0.1249 * (static_cast<double>(k) - static_cast<double>(a.best_pk));
                const double sb = static_cast<double>(b.drop_pal_ups) +
                0.1249 * (static_cast<double>(k) - static_cast<double>(b.best_pk));
                return sa < sb;
            });
        const size_t best_pk = best_it->best_pk;
        const size_t jk = best_it->jk;
        const size_t drop_palups = best_it->drop_pal_ups;
        if (drop_pal_ups_def <= drop_palups && !drop_abs_acq_def)
            continue;
        auto& target = nodes[k - best_pk];
        target.pad_left = static_cast<int>(best_pk);
        target.shifted = true;
        target.new_mask = true;
        target.absolute = true;
        target.state = pcs_c::composition_state_e::acquisition;
        target.nc_refresh = false;
        target.rgba = node.rgba;
        target.w = node.w;
        target.h = node.h;
        target.crop_x = node.crop_x;
        target.crop_y = node.crop_y;
        target.ev_x = node.ev_x;
        target.ev_y = node.ev_y;
        target.obj_id = node.obj_id;
        target.palette = node.palette;
        target.indexed = node.indexed;
        target.reusable = false;
        target.has_object = true;
        for (size_t z = jk; z < k - best_pk; ++z) {
            nodes[z].state = pcs_c::composition_state_e::normal;
            nodes[z].nc_refresh = true;
        }
        for (size_t z = k - best_pk + 1; z <= k; ++z) {
            nodes[z].state = pcs_c::composition_state_e::normal;
            nodes[z].nc_refresh = true;
            nodes[z].absolute = false;
        }
        node.acq = false;
        node.new_mask = false;
    }
}

void epoch_encoder_c::set_extended_visibilities(
    std::vector<epoch_node_t>& nodes) const
{
    // set_pgobjects_extended_visibilities: an object stays on screen across
    // contiguous events until a wipe/gap. This pass marks nc_refresh for
    // events that keep the same window object.
    const size_t nwin = m_windows.empty() ? 1 : m_windows.size();
    std::vector<size_t> running_objs(nwin, SIZE_MAX);
    for (size_t k = 0; k < nodes.size(); ++k) {
        auto& node = nodes[k];
        if (!node.has_object || static_cast<size_t>(node.window_id) >= nwin)
            continue;
        const size_t wid = static_cast<size_t>(node.window_id);
        if (running_objs[wid] != SIZE_MAX && !node.absolute && !node.acq)
            node.nc_refresh = true;
        running_objs[wid] = k;
    }
}

/// Two-phase encoding for overlap + 2 windows: build the node plan, run the
/// overlap pre-passes, then emit. The default (non-overlap) path is untouched.
std::vector<std::shared_ptr<pg_segment_c>>
epoch_encoder_c::encode_epoch_overlap(
    const std::vector<bdn_xml_event_c>& events,
    const std::vector<bool>& redraw_flags,
    common::fps_e fps_enum,
    int& palette_id_counter)
{
    std::vector<std::shared_ptr<pg_segment_c>> result;
    if (events.empty()) return result;
    const size_t nwin = m_windows.empty() ? 1 : m_windows.size();
    (void)nwin;
    // Per-window oid alternation (double buffering).
    std::vector<int> overlap_db(nwin, static_cast<int>(nwin));
    std::vector<epoch_node_t> nodes;
    nodes.reserve(events.size());
    double prev_pts = 0.0;
    double prev_dts_end = 0.0;
    int prev_window_id = 0;
    double prev_base_pts = 0.0;
    double prev_wipe_dur = 0.0;
    double prev_tc_out = 0.0;
    uint16_t prev_obj_id = 0;
    int prev_pos_x = 0, prev_pos_y = 0;
    int double_buffer = 1;  // Alternates 0/1 for single-window double buffering
    for (size_t k = 0; k < events.size(); ++k) {
        const auto& ev = events[k];
        const bool forced = ev.forced();
        epoch_node_t node;
        node.window_id = window_for(ev.x(), ev.y(), 0, 0);
        node.forced = forced;
        node.timings = compute_timings(ev.tc_in(),
            static_cast<uint64_t>(ev.width()) * static_cast<uint64_t>(ev.height()));
        node.dts_end = node.timings.base_dts + node.timings.wipe_dur;
        node.has_object = true;
        node.ev_x = ev.x();
        node.ev_y = ev.y();
        // NOTE: `reusable` (byte-identical bitmap in the same window) is
        // resolved below, after the frame is loaded and trimmed — matching
        // the single-pass path. Marking a node reusable on window_id alone
        // suppressed ODS/PDS for genuinely new images (truncated subtitles).
        node.reusable = false;
        node.new_mask = true;
        const double write_dur = node.timings.decode_duration;
        const double margin = (k == 0) ? 0.0 : (node.timings.base_pts - prev_base_pts);
        const auto sig = find_acqs_signals(node.timings.base_dts, prev_dts_end,
                                           node.timings.base_pts, prev_pts,
                                           write_dur, margin);
        node.acq = sig.valid;
        // acq and absolute stay independent: absolute means a new object
        // appeared (forced/redraw/first), acq means the decode fits with
        // margin. A node with absolute && !acq is the shift_forward candidate —
        // its acquisition did not fit and gets moved backward.
        // NOTE: node.absolute and node.state are resolved after the frame is
        // loaded below (they depend on the real reusable decision).

        auto rgba = ev.load_image();
        if (rgba.empty()) {
            logger_c::instance().warn("Empty image at event " + std::to_string(k));
            node.has_object = false;
            nodes.push_back(std::move(node));
            prev_pts = node.timings.base_pts;
            prev_dts_end = node.dts_end;
            prev_base_pts = node.timings.base_pts;
            continue;
        }
        std::vector<uint8_t> trimmed_rgba;
        int trim_w = 0, trim_h = 0, trim_x = 0, trim_y = 0;
        trim_transparent_padding(rgba, ev.width(), ev.height(),
                                 trimmed_rgba, trim_w, trim_h, trim_x, trim_y);
        node.w = (trim_w > 0) ? trim_w : ev.width();
        node.h = (trim_h > 0) ? trim_h : ev.height();
        node.crop_x = (trim_w > 0) ? trim_x : 0;
        node.crop_y = (trim_h > 0) ? trim_y : 0;
        node.rgba = trimmed_rgba;
        media::palette_t pal;
        std::vector<uint8_t> idx;
        if (quantize_image(trimmed_rgba, node.w, node.h, pal, idx)) {
            node.palette = std::move(pal);
            node.indexed = std::move(idx);
        }
        // Resolve reusable now that the frame is known: byte-identical trimmed
        // bitmap in the same window reuses the previously decoded object.
        // (single-pass parity, encode_epoch.cpp:592-593)
        if (k > 0 && node.window_id == prev_window_id) {
            auto& prev = nodes.back();
            node.reusable = (prev.has_object &&
                             prev.w == node.w && prev.h == node.h &&
                             prev.rgba == node.rgba);
        } else {
            node.reusable = false;
        }
        node.new_mask = !node.reusable;
        // Per-window oid alternation (double buffering), only with
        // m_alternate_oids && 2+ windows. Reused events keep their window's
        // previous oid (already decoded). Without the flag all non-reusable
        // objects alternate a global 0/1 buffer, matching the single-pass
        // numbering so a co-displayed normal case keeps distinct oids.
        if (m_alternate_oids && m_windows.size() >= 2) {
            int& dbw = overlap_db[static_cast<size_t>(node.window_id)];
            if (!node.reusable) {
                dbw = static_cast<int>(std::abs(static_cast<long long>(m_windows.size()) - dbw));
                node.obj_id = static_cast<uint16_t>(node.window_id + dbw);
            } else {
                node.obj_id = static_cast<uint16_t>(node.window_id + dbw);
            }
        } else if (!node.reusable) {
            double_buffer = 1 - double_buffer;
            node.obj_id = static_cast<uint16_t>(double_buffer);
        } else {
            node.obj_id = prev_obj_id;   // Reused object keeps its decoded oid
        }
        // Normal-case takeover: with two windows, the event that takes over the
        // other window while the previous object is still displayed becomes a
        // NORMAL composition that also conserves that object as a reference
        // CObject.
        const double obj_decode_dur =
            node.timings.obj_decode_time + node.timings.wipe_dur;
        const double dts_eff = node.timings.base_pts - obj_decode_dur;
        const bool nc_margin_ok =
            (dts_eff > (prev_base_pts - prev_wipe_dur));
        const bool normal_case =
            m_allow_normal_case && m_windows.size() == 2 && k > 0 &&
            node.timings.base_pts < prev_tc_out &&
            node.window_id != prev_window_id && nc_margin_ok &&
            !node.reusable;
        node.absolute = (k == 0) || forced ||
            (k < redraw_flags.size() && redraw_flags[k]) ||
            (node.new_mask && node.window_id != prev_window_id);
        if (normal_case) {
            node.state = pcs_c::composition_state_e::normal;
            node.absolute = false;   // never a shift_forward / acquisition node
            node.normal_case_ref = true;
            node.ref_window_id = static_cast<uint8_t>(prev_window_id);
            node.ref_obj_id = prev_obj_id;
            node.ref_x = prev_pos_x;
            node.ref_y = prev_pos_y;
        } else {
            node.state = node.absolute
                ? pcs_c::composition_state_e::acquisition
                : (node.reusable ? pcs_c::composition_state_e::normal
                                 : pcs_c::composition_state_e::epoch_start);
        }
        prev_pts = node.timings.base_pts;
        prev_dts_end = node.dts_end;
        prev_base_pts = node.timings.base_pts;
        prev_wipe_dur = node.timings.wipe_dur;
        prev_tc_out = ev.tc_out();
        prev_obj_id = node.obj_id;
        prev_pos_x = node.ev_x + node.crop_x;
        prev_pos_y = node.ev_y + node.crop_y;
        prev_window_id = node.window_id;
        nodes.push_back(std::move(node));
    }
    shift_forward_overlay(nodes, redraw_flags);
    set_extended_visibilities(nodes);
    return emit_epoch_from_nodes(nodes, fps_enum, palette_id_counter);
}

/// Emit the full epoch from the prepared nodes (post overlap pre-passes).
std::vector<std::shared_ptr<pg_segment_c>>
epoch_encoder_c::emit_epoch_from_nodes(
    const std::vector<epoch_node_t>& nodes,
    common::fps_e fps_enum,
    int& palette_id_counter)
{
    std::vector<std::shared_ptr<pg_segment_c>> result;
    if (nodes.empty()) return result;
    auto palette_id = static_cast<uint8_t>(palette_id_counter % 8);
    palette_id_counter++;
    for (size_t k = 0; k < nodes.size(); ++k) {
        const auto& node = nodes[k];
        if (!node.has_object)
            continue;
        if (node.has_clear && (!m_overlap || node.clear_pts < node.timings.base_pts)) {
            // Clear display set — inline analog of the emit_clear_ds lambda
            // in encode_epoch (US1 gap rule).
            const double wipe_dur = std::ceil(static_cast<double>(node.clear_w) *
                static_cast<double>(node.clear_h) * pg_decoder_t::FREQ / pg_decoder_t::RC) /
                pg_decoder_t::FREQ;
            double dts = node.clear_pts - wipe_dur;
            if (dts < 0) dts = 0;
            auto clear_pcs = pcs_c::from_scratch(
                static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height),
                static_cast<uint8_t>(fps_enum.to_pcsfps()),
                static_cast<uint16_t>(m_composition_n++),
                pcs_c::composition_state_e::normal, false, palette_id,
                {}, node.clear_pts, dts);
            result.push_back(std::make_shared<pcs_c>(std::move(clear_pcs)));
            window_definition_t clear_wd;
            clear_wd.window_id = 0;
            clear_wd.h_pos = static_cast<uint16_t>(node.clear_x);
            clear_wd.v_pos = static_cast<uint16_t>(node.clear_y);
            clear_wd.width = static_cast<uint16_t>(node.clear_w);
            clear_wd.height = static_cast<uint16_t>(node.clear_h);
            auto clear_wds = wds_c::from_scratch({clear_wd}, node.clear_pts, dts);
            result.push_back(std::make_shared<wds_c>(std::move(clear_wds)));
            auto clear_end = ends_c::from_scratch(node.clear_pts, dts);
            result.push_back(std::make_shared<ends_c>(std::move(clear_end)));
        }
        event_emit_input_t in{
            node.w, node.h, node.crop_x, node.crop_y,
            node.ev_x, node.ev_y, node.forced,
            node.state, node.obj_id,
            static_cast<uint8_t>(node.window_id), palette_id,
            node.reusable, fps_enum, node.palette, node.indexed,
            node.timings};
        in.normal_case_ref = node.normal_case_ref;
        in.ref_window_id = node.ref_window_id;
        in.ref_obj_id = node.ref_obj_id;
        in.ref_x = node.ref_x;
        in.ref_y = node.ref_y;
        auto segs = emit_event_segments(in, result);
        for (auto& seg : segs)
            result.push_back(std::move(seg));
    }
    // End-of-epoch clear (final wipe) — parity with the single-pass path
    // (encode_epoch: emit_clear_ds after the last event).
    if (!nodes.empty()) {
        double last_pts = 0.0;
        int last_w = 0, last_h = 0, last_x = 0, last_y = 0;
        for (const auto& node : nodes) {
            if (!node.has_object) continue;
            last_pts = node.timings.base_pts;
            last_w = node.w;
            last_h = node.h;
            last_x = node.ev_x;
            last_y = node.ev_y;
        }
        if (last_w > 0 && last_h > 0) {
            const double wipe_dur = std::ceil(static_cast<double>(last_w) *
                static_cast<double>(last_h) * pg_decoder_t::FREQ / pg_decoder_t::RC) /
                pg_decoder_t::FREQ;
            double dts = last_pts - wipe_dur;
            if (dts < 0) dts = 0;
            auto clear_pcs = pcs_c::from_scratch(
                static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height),
                static_cast<uint8_t>(fps_enum.to_pcsfps()),
                static_cast<uint16_t>(m_composition_n++),
                pcs_c::composition_state_e::normal, false, palette_id,
                {}, last_pts, dts);
            result.push_back(std::make_shared<pcs_c>(std::move(clear_pcs)));
            window_definition_t clear_wd;
            clear_wd.window_id = 0;
            clear_wd.h_pos = static_cast<uint16_t>(last_x);
            clear_wd.v_pos = static_cast<uint16_t>(last_y);
            clear_wd.width = static_cast<uint16_t>(last_w);
            clear_wd.height = static_cast<uint16_t>(last_h);
            auto clear_wds = wds_c::from_scratch({clear_wd}, last_pts, dts);
            result.push_back(std::make_shared<wds_c>(std::move(clear_wds)));
            auto clear_end = ends_c::from_scratch(last_pts, dts);
            result.push_back(std::make_shared<ends_c>(std::move(clear_end)));
        }
    }
    // align_palette_updates — parity with the single-pass path (US3):
    // strictly monotonic DTS over the emitted stream.
    if (m_overlap) {
        double last_dts = -1.0;
        for (auto& seg : result) {
            if (seg->dts() <= last_dts) {
                const double nd = last_dts + 1.0 / pg_decoder_t::FREQ;
                seg->set_dts(nd);
                if (seg->pts() < nd)
                    seg->set_pts(nd);
                last_dts = nd;
            } else {
                last_dts = seg->dts();
            }
        }
    }
    // Parity with encode_epoch (single-pass): report the epoch's node count
    // and emitted segment count so the overlap path shows the same progress
    // log lines the default path does.
    {
        size_t with_object = 0;
        for (const auto& node : nodes)
            if (node.has_object) with_object++;
        logger_c::instance().info("Encoded epoch with " +
            std::to_string(with_object) + " events, " +
            std::to_string(result.size()) + " segments.");
    }
    return result;
}

} // namespace core
} // namespace opensup
