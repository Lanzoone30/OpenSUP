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
/// Defined in encode_epoch.cpp.
std::vector<uint8_t> crop_indexed(const std::vector<uint8_t>& indexed, int ax, int ay,
                                  int obj_w, int obj_h, int x0, int y0, int x1, int y1);
// Overlap pipeline: active only when m_overlap && m_windows.size() == 2.

void epoch_encoder_c::shift_forward_overlay(
    std::vector<epoch_node_t>& nodes,
    const std::vector<bool>& forced_acq) const
{
    // Backtracks from an acquisition node with exactly
    // one new object (sum(new_mask)==1) and merges it into the best previous
    // node of the other window, so the object decodes earlier and the
    // intermediate range stays NORMAL.
    for (size_t k = 0; k < nodes.size(); ++k) {
        auto& node = nodes[k];
        if (!forced_acq.empty() && forced_acq[k])
            continue;   // anchor: keep full acquisition with its object
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
            if (!forced_acq.empty() && forced_acq[pj])
                continue;   // anchor must keep its own object
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
    // An object stays on screen across
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
    int double_buffer = 1;  // Legacy global 0/1 toggle (--alternate-oids), any window count
    for (size_t k = 0; k < events.size(); ++k) {
        const auto& ev = events[k];
        const bool forced = ev.forced();
        epoch_node_t node;
        node.window_id = window_for(ev.x(), ev.y(), 0, 0);
        node.forced = forced;
        node.timings = compute_timings(ev.tc_in(),
            static_cast<uint64_t>(ev.width()) * static_cast<uint64_t>(ev.height()));
        node.dts_end = node.timings.base_dts + node.timings.wipe_dur;
        node.tc_out = ev.tc_out();
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
        // Anchor events re-send the object even when the bitmap is unchanged.
        if (k < redraw_flags.size() && redraw_flags[k]) {
            node.reusable = false;
            node.new_mask = true;
        }
        // Object id assignment: per-window double buffering by default;
        // --alternate-oids requests the legacy global 0/1 scheme.
        if (m_alternate_oids) {
            if (!node.reusable) {
                double_buffer = 1 - double_buffer;
                node.obj_id = static_cast<uint16_t>(double_buffer);
            } else {
                node.obj_id = prev_obj_id;   // Reused object keeps its decoded oid
            }
        } else {
            int& dbw = overlap_db[static_cast<size_t>(node.window_id)];
            if (!node.reusable) {
                dbw = static_cast<int>(std::abs(static_cast<long long>(m_windows.size()) - dbw));
                node.obj_id = static_cast<uint16_t>(node.window_id + dbw);
            } else {
                node.obj_id = prev_obj_id;
            }
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
    // PGS allows exactly one EpochStart per epoch: it is
    // the first displayed object; any later start candidate becomes acquisition.
    bool epoch_start_seen = false;
    for (auto& node : nodes) {
        if (!node.has_object) continue;
        if (!epoch_start_seen) {
            node.state = pcs_c::composition_state_e::epoch_start;
            epoch_start_seen = true;
        } else if (node.state == pcs_c::composition_state_e::epoch_start) {
            node.state = pcs_c::composition_state_e::acquisition;
        }
    }
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
    // Emission-time per-window oid slots and last-object cache, matching the
    // single-pass multi-window path: every acquisition re-lists the coupled
    // windows, so an untouched window keeps its object with a fresh oid.
    const size_t wen = m_windows.empty() ? 1 : m_windows.size();
    std::vector<int> emit_db(wen, static_cast<int>(wen));
    struct em_win_obj_t {
        std::vector<uint8_t> indexed;
        int w = 0, h = 0, x = 0, y = 0;
        bool active = false;
    };
    std::vector<em_win_obj_t> em_win_objects(wen);
    bool em_prev_all = false;
    for (size_t k = 0; k < nodes.size(); ++k) {
        const auto& node = nodes[k];
        if (!node.has_object)
            continue;
        if (node.has_clear && (!m_overlap || node.clear_pts < node.timings.base_pts)) {
            // Clear display set — inline analog of the emit_clear_ds lambda
            // in encode_epoch.
            const double wipe_dur = std::ceil(static_cast<double>(node.clear_w) *
                static_cast<double>(node.clear_h) * pg_decoder_t::FREQ / pg_decoder_t::RC) /
                pg_decoder_t::FREQ;
            double dts = node.clear_pts - wipe_dur;
            if (dts < 0) dts = 0;
            if ((wipe_dur + 3.0 / pg_decoder_t::FREQ) >= 1.0 / m_fps) {
                emit_palette_wipe(result, static_cast<uint8_t>(fps_enum.to_pcsfps()),
                                  node.clear_pts, dts, palette_id);
            } else {
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
        }
        std::vector<std::shared_ptr<pg_segment_c>> ev_segs;
        if (wen < 2) {
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
                ev_segs.push_back(std::move(seg));
        } else {
            // Split the node's bitmap per layout window; the touched windows go
            // first, then the coupled untouched ones (kept alive on acquisition).
            const int ax = node.ev_x + node.crop_x, ay = node.ev_y + node.crop_y;
            std::vector<size_t> emit_order;
            for (size_t wi = 0; wi < wen; ++wi) {
                const auto& wd = m_windows[wi];
                const int ix0 = std::max(ax, static_cast<int>(wd.h_pos));
                const int iy0 = std::max(ay, static_cast<int>(wd.v_pos));
                const int ix1 = std::min(ax + node.w,
                    static_cast<int>(wd.h_pos) + static_cast<int>(wd.width));
                const int iy1 = std::min(ay + node.h,
                    static_cast<int>(wd.v_pos) + static_cast<int>(wd.height));
                if (ix1 > ix0 && iy1 > iy0)
                    emit_order.push_back(wi);
            }
            const bool reemit_retained = !node.reusable && !node.normal_case_ref &&
                !emit_order.empty() && em_prev_all;
            if (reemit_retained) {
                for (size_t wi = 0; wi < wen; ++wi)
                    if (std::find(emit_order.begin(), emit_order.end(), wi) ==
                            emit_order.end() && em_win_objects[wi].active)
                        emit_order.push_back(wi);
            }
            for (size_t wi : emit_order) {
                const auto& wd = m_windows[wi];
                const int wx = wd.h_pos, wy = wd.v_pos;
                const int ix0 = std::max(ax, wx), iy0 = std::max(ay, wy);
                const int ix1 = std::min(ax + node.w, wx + static_cast<int>(wd.width));
                const int iy1 = std::min(ay + node.h, wy + static_cast<int>(wd.height));
                const bool covered = (ix1 > ix0 && iy1 > iy0);
                std::vector<uint8_t> sub;
                int ow, oh, ox, oy;
                if (covered) {
                    sub = crop_indexed(node.indexed, ax, ay, node.w, node.h,
                                       ix0, iy0, ix1, iy1);
                    ow = ix1 - ix0; oh = iy1 - iy0; ox = ix0 - wx; oy = iy0 - wy;
                    em_win_objects[wi] = em_win_obj_t{sub, ow, oh, ox, oy, true};
                } else {
                    sub = em_win_objects[wi].indexed;
                    ow = em_win_objects[wi].w; oh = em_win_objects[wi].h;
                    ox = em_win_objects[wi].x; oy = em_win_objects[wi].y;
                }
                uint16_t win_oid;
                if (wd.window_id == static_cast<uint8_t>(node.window_id)) {
                    if (!node.reusable) {
                        int& dbw = emit_db[wi];
                        dbw = static_cast<int>(std::abs(
                            static_cast<long long>(wen) - dbw));
                    }
                    win_oid = node.obj_id;
                } else {
                    int& dbw = emit_db[wi];
                    dbw = static_cast<int>(std::abs(
                        static_cast<long long>(wen) - dbw));
                    win_oid = static_cast<uint16_t>(static_cast<int>(wi) + dbw);
                }
                event_emit_input_t win_in{
                    ow, oh, ox, oy, wx, wy, node.forced, node.state,
                    win_oid, wd.window_id, palette_id, node.reusable, fps_enum,
                    node.palette, sub, node.timings};
                win_in.normal_case_ref =
                    node.normal_case_ref && wd.window_id != node.ref_window_id;
                win_in.ref_window_id = node.ref_window_id;
                win_in.ref_obj_id = node.ref_obj_id;
                win_in.ref_x = node.ref_x;
                win_in.ref_y = node.ref_y;
                auto wsegs = emit_event_segments(win_in, result);
                for (auto& seg : wsegs)
                    ev_segs.push_back(std::move(seg));
            }
            em_prev_all = (emit_order.size() == wen);
            // Merge into one display set: single PCS carrying every window's
            // CObject, single WDS, one of each equal PDS, single ENDS.
            std::shared_ptr<pcs_c> merged_pcs;
            std::vector<uint8_t> first_pds;
            bool have_wds = false, have_pds = false;
            for (size_t si = 0; si < ev_segs.size(); ++si) {
                auto& seg = ev_segs[si];
                if (auto pcs = std::dynamic_pointer_cast<pcs_c>(seg)) {
                    if (merged_pcs) {
                        merged_pcs->cobjects.insert(merged_pcs->cobjects.end(),
                                                    pcs->cobjects.begin(),
                                                    pcs->cobjects.end());
                        merged_pcs->update();
                        continue;
                    }
                    merged_pcs = pcs;
                } else if (std::dynamic_pointer_cast<wds_c>(seg)) {
                    if (have_wds)
                        continue;
                    have_wds = true;
                } else if (std::dynamic_pointer_cast<pds_c>(seg)) {
                    auto bytes = seg->to_bytes();
                    if (have_pds && bytes == first_pds)
                        continue;
                    have_pds = true;
                    first_pds = std::move(bytes);
                } else if (dynamic_cast<ends_c*>(seg.get()) != nullptr &&
                           si + 1 < ev_segs.size()) {
                    continue;
                }
                result.push_back(std::move(seg));
            }
            if (merged_pcs)
                m_last_cobjs = merged_pcs->cobjects;
            continue;
        }
        for (auto& seg : ev_segs)
            result.push_back(std::move(seg));
    }
    // End-of-epoch clear (final wipe) — parity with the single-pass path
    // (encode_epoch: emit_clear_ds after the last event).
    if (!nodes.empty()) {
        double last_pts = 0.0, last_base_pts = 0.0;
        int last_w = 0, last_h = 0, last_x = 0, last_y = 0;
        for (const auto& node : nodes) {
            if (!node.has_object) continue;
            last_pts = node.tc_out;
            last_base_pts = node.timings.base_pts;
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
            const double write_ticks = std::ceil(static_cast<double>(last_w) *
                static_cast<double>(last_h) * pg_decoder_t::FREQ / pg_decoder_t::RC);
            const double last_frames = (last_pts - last_base_pts) * m_fps;
            const bool perform_wds_end = last_frames >=
                std::ceil((write_ticks + 10.0) / pg_decoder_t::FREQ * m_fps);
            auto emit_wds_clear = [&](double pts) {
                double ddts = pts - wipe_dur;
                if (ddts < 0) ddts = 0;
                auto clear_pcs = pcs_c::from_scratch(
                    static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height),
                    static_cast<uint8_t>(fps_enum.to_pcsfps()),
                    static_cast<uint16_t>(m_composition_n++),
                    pcs_c::composition_state_e::normal, false, palette_id,
                    {}, pts, ddts);
                result.push_back(std::make_shared<pcs_c>(std::move(clear_pcs)));
                window_definition_t clear_wd;
                clear_wd.window_id = 0;
                clear_wd.h_pos = static_cast<uint16_t>(last_x);
                clear_wd.v_pos = static_cast<uint16_t>(last_y);
                clear_wd.width = static_cast<uint16_t>(last_w);
                clear_wd.height = static_cast<uint16_t>(last_h);
                auto clear_wds = wds_c::from_scratch({clear_wd}, pts, ddts);
                result.push_back(std::make_shared<wds_c>(std::move(clear_wds)));
                auto clear_end = ends_c::from_scratch(pts, ddts);
                result.push_back(std::make_shared<ends_c>(std::move(clear_end)));
            };
            if (!perform_wds_end) {
                emit_palette_wipe(result, static_cast<uint8_t>(fps_enum.to_pcsfps()),
                                  last_pts, dts, palette_id);
                // Last epoch only: no successor bounds the trailing wipe.
                if (m_is_last_epoch) {
                    const double nf_shift = std::max(1.0, std::ceil(
                        (write_ticks + 10.0) * m_fps / pg_decoder_t::FREQ));
                    emit_wds_clear(last_pts + nf_shift / m_fps);
                }
            } else {
                emit_wds_clear(last_pts);
            }
        }
    }
    // align_palette_updates — parity with the single-pass path:
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

}
}
