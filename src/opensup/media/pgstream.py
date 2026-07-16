#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Copyright (C) 2024-2026 cibo
This file is part of OpenSUP, based on SUPer <https://github.com/cubicibo/SUPer>.

OpenSUP is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OpenSUP is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenSUP.  If not, see <http://www.gnu.org/licenses/>.
"""

from typing import Any, Type, Optional
from dataclasses import dataclass, field
import numpy as np

from opensup.core.segments import PGSegment, PCS, WDS, PDS, ODS, ENDS, Epoch, DisplaySet
from opensup.media.pgraphics import PGDecoder, PGraphics, PGObjectBuffer
from opensup.media.palette import Palette
from opensup.utils.logging import LogFacility
from opensup.utils.timecode import TC
from opensup.utils.geometry import Box

logger = LogFacility.get_logger('OpenSUP')


@dataclass
class EpochContext:
    box: Box
    windows: list[Box]
    events: list[Any]
    min_dts: float = -np.inf
    max_pts: float = np.inf
    redraw_flags: list[bool] = field(default_factory=list)


@dataclass
class BufferStats:
    min: float = np.inf
    avg: float = 0
    maxrate: float = 0
    count: int = 0
    tsmaxrate: int = 0
    tsmin: int = 0
    tsavg: int = 0
    max1s: float = 0


class LeakyBuffer:
    SIZE = (1 << 20)
    TS_MASK = (1 << 32) - 1

    def __init__(self, first_ts: int, bitrate: Optional[int] = None) -> None:
        self.used_bytes = self.__class__.SIZE
        self.bitrate = (PGDecoder.RX) if bitrate is None else int(bitrate)
        self.last_ts = first_ts
        self.stats = BufferStats()
        self.good_ds = True
        self.rate_past = []

    def set_tc_func(self, tc_func) -> None:
        self._tc_func = tc_func

    def step(self, segment: Type[PGSegment]) -> bool:
        if isinstance(segment, PCS):
            self.good_ds = True

        new_ts = segment.tdts
        dticks = (new_ts - self.last_ts) & self.__class__.TS_MASK

        self.used_bytes = min(self.used_bytes + round(dticks*self.bitrate/PGDecoder.FREQ), self.__class__.SIZE)
        self.used_bytes -= (len(segment) - 2)

        self.set_stats(segment.tdts)
        self.last_ts = new_ts

        self.good_ds &= self.used_bytes >= 0

        if not self.good_ds and isinstance(segment, ENDS):
            logger.warning(f"PG stream underflow at {self._tc_func(segment.tpts)}: {self.used_bytes} bytes.")
        return self.used_bytes >= 0

    def set_bitrate(self, size_ds: int, curr_ts: int, prev_ts: int) -> None:
        dticks = (curr_ts - prev_ts) & self.__class__.TS_MASK
        if dticks > 0:
            rate = size_ds*PGDecoder.FREQ/dticks
            if rate >= self.stats.maxrate:
                self.stats.maxrate = rate
                self.stats.tsmaxrate = self._tc_func(curr_ts)
        self.rate_past = list(filter(lambda x: (curr_ts - x[1]) & self.__class__.TS_MASK <= PGDecoder.FREQ, self.rate_past))
        self.rate_past.append((size_ds, curr_ts))

        crate = sum(map(lambda x: x[0], self.rate_past))/(128*1024)
        if crate >= self.stats.max1s:
            self.stats.tsavg = self._tc_func(curr_ts)
            self.stats.max1s = crate

    def get_usage(self) -> float:
        return self.used_bytes/self.__class__.SIZE

    def set_stats(self, ts: int) -> None:
        if self.get_usage() <= self.stats.min:
            self.stats.min = self.get_usage()
            self.stats.tsmin = self._tc_func(ts)
        self.stats.avg = ((self.stats.avg*self.stats.count) + self.get_usage())/(self.stats.count + 1)
        self.stats.count += 1

    def get_stats(self) -> BufferStats:
        return (100*self.stats.min, 100*self.stats.avg, round(self.stats.max1s, 3))


def test_rx_bitrate(epochs: list[Epoch], bitrate: int, fps: float) -> bool:
    prev_ts = (epochs[0][0][0].tdts-int(PGDecoder.FREQ)) & ((1<<32)-1)
    is_ok = True
    leaky = LeakyBuffer(prev_ts, bitrate)

    f_print_tc = lambda pts: str(TC.s2tc(pts/PGDecoder.FREQ, fps)) + ('' if (float(fps).is_integer() or fps < 25) else (', DF=' + str(TC.s2tc(pts/PGDecoder.FREQ, fps, True))))

    leaky.set_tc_func(f_print_tc)

    dur_offset = 0
    ts_first = prev_ts
    for epoch in epochs:
        for ds in epoch:
            for seg in ds:
                is_ok &= leaky.step(seg)
            leaky.set_bitrate(len(bytes(ds)), ds.pcs.tpts, prev_ts)
            if ds.pcs.tpts < prev_ts and ts_first < np.inf and ts_first != prev_ts:
                dur_offset += LeakyBuffer.TS_MASK + 1
                ts_first = np.inf
            prev_ts = ds.pcs.tpts
    dur_offset += (epochs[-1][-1].pcs.tpts - epochs[0][0].pcs.tpts)
    stats = leaky.get_stats()

    avg_bitrate = sum(map(lambda x: len(bytes(x)), epochs))/(dur_offset/PGDecoder.FREQ)
    logger.iinfo(f"Bitrate: AVG={avg_bitrate/(128*1024):.04f} Mbps, PEAK(1s)={stats[2]:.03f} Mbps @ {leaky.stats.tsavg}.")

    f_log_fun = logger.iinfo if is_ok else logger.warning
    f_log_fun(f"Target bitrate underflow margin (higher is better): AVG={stats[1]:.02f}%, MIN={stats[0]:.02f}% @ {leaky.stats.tsmin}")
    return is_ok


def test_diplayset(ds: DisplaySet) -> bool:
    comply = ds.pcs is not None and isinstance(ds[0], PCS)
    comply = comply and 0 <= ds.pcs.n_objects <= 2 and ds.pcs.n_objects == len(ds.pcs.cobjects)
    if ds.pcs.composition_state != PCS.CompositionState.NORMAL:
        comply &= ds.pcs.pal_flag is False
        comply &= ds.wds is not None
    comply = comply and ds.pcs.pal_id < 8
    if ds.wds:
        comply &= isinstance(ds[1], WDS)
        comply &= ds.pcs.pal_flag is False
        comply &= 1 <= len(ds.wds.windows) <= 2
    if ds.pds:
        pds_ids = set()
        for pds in ds.pds:
            comply &= pds.p_id not in pds_ids
            pds_ids.add(pds.p_id)
            if ds.pcs.pal_flag:
                comply &= ds.pcs.pal_id == pds.p_id
            comply &= pds.p_id < 8
            comply &= pds.n_entries <= 256
    if ds.ods:
        ctx_cnt = 0
        for ods in ds.ods:
            ctx_cnt += bool(ods.flags & ODS.ODSFlags.SEQUENCE_FIRST)
            ctx_cnt -= bool(ods.flags & ODS.ODSFlags.SEQUENCE_LAST)
        comply &= 0 == ctx_cnt
    return comply & (ds.end is not None) & isinstance(ds[-1], ENDS)


def is_compliant(epochs: list[Epoch], fps: float) -> bool:
    ts_mask = ((1 << 32) - 1)
    prev_pts = -1
    compliant = True
    warnings = 0
    cumulated_ods_size = 0
    prev_pcs_id = 0xFFFF

    to_tc = lambda pts: str(TC.s2tc(pts, fps)) + ('' if (float(fps).is_integer() or fps < 25) else (', DF=' + str(TC.s2tc(pts, fps, True))))

    for ke, epoch in enumerate(epochs):
        windows = {}
        ods_vn = {}
        ods_hash = {}
        ods_filled = set()
        pds_vn = [-1] * 8
        pals = [Palette() for _ in range(8)]
        buffer = PGObjectBuffer()

        if epoch[0].pcs.composition_state & PCS.CompositionState.EPOCH_START == 0:
            logger.warning(f"First display set in epoch is not an Epoch Start at {to_tc(epoch[0].pcs.pts)}.")
            compliant = False

        if epoch[0].wds:
            for wd in epoch[0].wds.windows:
                if wd.h_pos + wd.width > epoch[0].pcs.width or wd.v_pos + wd.height > epoch[0].pcs.height:
                    logger.error(f"Window {wd.window_id} out of screen in epoch starting at {to_tc(epoch[0].pcs.pts)}.")
                    compliant = False

        last_ds = []
        for kd, ds in enumerate(epoch.ds):
            compliant &= test_diplayset(ds)
            current_pts = ds.pcs.pts

            if epoch.ds[kd-1].pcs.pts != prev_pts and current_pts != epoch.ds[kd-1].pcs.pts:
                prev_pts = epoch.ds[kd-1].pcs.pts
            else:
                logger.warning(f"Two display sets at {to_tc(current_pts)}.")
            if kd > 0 and ds.pcs.composition_state == PCS.CompositionState.EPOCH_START:
                logger.error(f"Found an Epoch Start at {to_tc(current_pts)} in the middle of an epoch.")
                compliant = False

            is_dupe = False
            if len(ds) == len(last_ds) and ds.pcs.composition_n == prev_pcs_id:
                ds1 = DisplaySet.from_bytes(bytes(ds))
                ds2 = DisplaySet.from_bytes(bytes(last_ds))
                for seg1, seg2 in zip(ds1, ds2):
                    seg1.tpts = seg1.tdts = 0
                    seg2.tpts = seg2.tdts = 0
                is_dupe = ds1.pcs.composition_state == PCS.CompositionState.ACQUISITION
                is_dupe = is_dupe and ds2.pcs.composition_state in [PCS.CompositionState.ACQUISITION, PCS.CompositionState.EPOCH_START]
                ds2.pcs.composition_state = ds1.pcs.composition_state = PCS.CompositionState.ACQUISITION
                is_dupe = is_dupe and bytes(ds1) == bytes(ds2)
            last_ds = ds

            for ks, seg in enumerate(ds.segments):
                if isinstance(seg, PCS):
                    if not is_dupe and seg.composition_n != (prev_pcs_id + 1) & 0xFFFF and seg.composition_state != PCS.CompositionState.EPOCH_START:
                        logger.warning(f"Displayset does not increment composition number normally at {to_tc(current_pts)}.")
                    prev_pcs_id = seg.composition_n
                    if int(seg.composition_state) != 0:
                        ods_filled.clear()
                        for pal in pals:
                            pal.palette.clear()

                elif isinstance(seg, WDS):
                    if len(windows) == 0:
                        for w in seg.windows:
                            windows[w.window_id] = (w.h_pos, w.v_pos, w.width, w.height)
                        lwdb = list(map(lambda w: Box(w[1], w[3], w[0], w[2]), windows.values()))
                        if len(windows) == 2 and Box.intersect(*lwdb).area > 0:
                            logger.error(f"Overlapping windows in epoch starting at {to_tc(current_pts)}.")
                            compliant = False
                    else:
                        for w in seg.windows:
                            if windows[w.window_id] != (w.h_pos, w.v_pos, w.width, w.height):
                                logger.error(f"Window change mid-epoch at {to_tc(current_pts)}, this is strictly prohibited.")
                                compliant = False

                elif isinstance(seg, PDS):
                    if (pds_vn[seg.p_id] + 1) & 0xFF != seg.p_vn and not is_dupe:
                        logger.warning(f"Palette version not incremented by one, may be discarded by decoder. Palette {seg.p_id} at DTS {to_tc(seg.pts)}.")
                    pds_vn[seg.p_id] = seg.p_vn
                    new_pal = seg.to_palette()
                    pals[seg.p_id] |= new_pal
                    if next(filter(lambda x: not (16 <= x.y <= 235 and 16 <= x.cb <= 240 and 16 <= x.cr <= 240), new_pal), None) is not None:
                        logger.warning(f"Palette is not limited range at {to_tc(current_pts)}.")
                        compliant = False
                    if (pal_ff_entry := pals[seg.p_id].get(0xFF, None)) is not None and pal_ff_entry.alpha != 0:
                        logger.warning(f"Palette entry 0xFF is set and not transparent at {to_tc(current_pts)}.")
                        compliant = False

                elif isinstance(seg, ODS):
                    if seg.flags & ODS.ODSFlags.SEQUENCE_FIRST:
                        ods_data = bytearray()
                        ods_width = seg.width
                        ods_height = seg.height
                        ods_object_id = seg.o_id
                        ods_object_vn = seg.o_vn
                        if 8 > min(ods_width, ods_height) or 4096 < max(ods_width, ods_height):
                            logger.error(f"Illegal object dimensions at {to_tc(current_pts)}, object id={seg.o_id}: {ods_width}x{ods_height}.")
                            compliant = False
                            continue
                        if (slot := buffer.get(seg.o_id)) is None:
                            if not buffer.allocate_id(seg.o_id, seg.width, seg.height):
                                logger.error(f"Object buffer overflow (not enough memory for all object slots) at {to_tc(current_pts)}.")
                                compliant = False
                        elif slot.shape != (seg.width, seg.height):
                            logger.error(f"Object-slot {seg.o_id} dimensions mismatch. Slot: {slot.shape}, object: {(seg.width, seg.height)} at {to_tc(current_pts)}.")
                            compliant = False
                        if cumulated_ods_size > 0:
                            logger.error("A past ODS was not properly terminated! Stream is critically corrupted!")
                            compliant = False
                    elif ods_object_id != seg.o_id or ods_object_vn != seg.o_vn:
                        logger.error("Object definition header mismatch in a chain of segments! Stream is critically corrupted!")
                        compliant = False

                    cumulated_ods_size += len(bytes(seg))
                    ods_data += seg.data

                    if seg.flags & ODS.ODSFlags.SEQUENCE_LAST:
                        data_hash = hash(bytes(ods_data))
                        if cumulated_ods_size >= PGDecoder.CODED_BUF_SIZE-(16*9):
                            logger.error(f"Coded buffer overflow at {to_tc(current_pts)}: coded object size exceed 1 MiB.")
                            compliant = False
                        cumulated_ods_size = 0

                        if seg.o_id in ods_filled and ods_hash.get(seg.o_id, None) != data_hash:
                            if ods_vn[seg.o_id] == seg.o_vn:
                                logger.warning(f"Object {seg.o_id} at {to_tc(current_pts)} differs from previous but does not increment version number. It will be discarded.")
                        ods_filled.add(seg.o_id)
                        ods_vn[seg.o_id] = seg.o_vn
                        ods_hash[seg.o_id] = data_hash

                        try:
                            dec_bitmap = PGraphics.decode_rle(ods_data, width=ods_width, height=ods_height, check_rle=True)
                        except AssertionError:
                            dec_bitmap = PGraphics.decode_rle(ods_data, width=ods_width, height=ods_height, check_rle=False)
                            logger.warning(f"ODS at {to_tc(current_pts)} has too long RLE line(s). Older decoders may have issues.")
                            warnings += 1

                        for pe in np.unique(dec_bitmap):
                            if pe != 0xFF and pe not in pals[ds.pcs.pal_id].palette:
                                logger.warning(f"ODS at {to_tc(current_pts)} uses undefined palette entries (first: {pe:02X}). Some pixels will not display.")
                                warnings += 1
                                break
                elif isinstance(seg, ENDS):
                    if ds.wds:
                        for cobj in ds.pcs.cobjects:
                            obj_dims = buffer.get(cobj.o_id)
                            if obj_dims is None:
                                logger.error(f"Using an unknown slot {cobj.o_id} in buffer at {to_tc(current_pts)}.")
                                compliant = False
                            elif cobj.o_id not in ods_filled:
                                logger.error(f"Using expired memory for object {cobj.o_id} at {to_tc(current_pts)}.")
                                compliant = False
                            else:
                                w, h = obj_dims.shape
                                if cobj.cropped:
                                    if h < cobj.c_h or w < cobj.c_w or h < cobj.c_h + cobj.vc_pos or w < cobj.c_w + cobj.hc_pos:
                                        logger.error(f"Cropped dimension exceeed object {cobj.o_id} size at {to_tc(current_pts)}.")
                                        compliant = False
                                    else:
                                        w, h = cobj.c_w, cobj.c_h
                                        if w == 0 or h == 0:
                                            logger.warning("Zero cropping width or height at {to_tc(current_pts)}.")
                                            warnings += 1
                            wd = ds.wds.windows[cobj.window_id]
                            if cobj.h_pos < wd.h_pos or cobj.h_pos + w > wd.h_pos + wd.width or\
                               cobj.v_pos < wd.v_pos or cobj.v_pos + h > wd.v_pos + wd.height:
                                logger.error(f"Composition object {cobj.o_id} misplaced outside of window {wd.window_id} at {to_tc(current_pts)}.")
                                compliant = False
            if ds.pcs.pal_flag or ds.wds is not None:
                compliant &= pds_vn[ds.pcs.pal_id] >= 0
    return compliant, warnings


def debug_stats(epochs: list[Epoch]) -> str:
    PTS_MASK = (1 << 32) - 1
    cnt_acq = cnt_nc = cnt_es = 0
    cnt_nc_ods = 0
    cnt_pu = cnt_buffered_pu = 0
    for epoch in epochs:
        pts_delta_w = list(map(lambda w: int(np.ceil(w.width*w.height*PGDecoder.FREQ/PGDecoder.RC)), epoch[0].wds))
        pts_delta = sum(pts_delta_w)
        for ds in epoch:
            if ds.pcs.composition_state == PCS.CompositionState.NORMAL:
                cnt_nc += 1
            elif ds.pcs.composition_state & PCS.CompositionState.ACQUISITION:
                cnt_acq += 1
            elif ds.pcs.composition_state & PCS.CompositionState.EPOCH_START:
                cnt_es += 1
            if ds.pcs.pal_flag:
                cnt_pu += 1
                if (ds.pcs.tpts - ds.pcs.tdts) & PTS_MASK > pts_delta+1:
                    cnt_buffered_pu += 1
            elif ds.wds and ds.pcs.composition_state == 0 and ds.ods:
                cnt_nc_ods += 1
    return f"n(ES)={cnt_es}, n(ACQ)={cnt_acq}, n(NC)={cnt_nc} (n(ODS)={cnt_nc_ods}, n(PU)={cnt_pu}). Buffered: n(PU)={cnt_buffered_pu}."


def check_pts_dts_sanity(epochs: list[Epoch], fps: float) -> bool:
    PTS_MASK = (1 << 32) - 1
    PTS_DIFF_BOUND = PTS_MASK >> 1
    is_compliant = True
    prev_pts = prev_dts = -99999999
    min_dts_delta = 99999999

    to_tc = lambda pts: str(TC.s2tc(pts, fps)) + ('' if (float(fps).is_integer() or fps < 25) else (', DF=' + str(TC.s2tc(pts, fps, True))))
    frame_duration = np.floor(PGDecoder.FREQ/fps)

    for k, epoch in enumerate(epochs):
        pts_delta = int(sum(map(lambda w: np.ceil(w.width*w.height*PGDecoder.FREQ/PGDecoder.RC), epoch[0].wds)))
        wipe_duration = int(np.ceil(epoch[0].pcs.width*epoch[0].pcs.height*PGDecoder.FREQ/PGDecoder.RC))
        pts_dts_delta_epoch_start = (epoch[0].pcs.tpts - epoch[0].pcs.tdts) & PTS_MASK > wipe_duration
        diff = (epoch[0].pcs.tdts - prev_pts) & PTS_MASK
        pts_last_dts_epoch_start = diff > 0 and diff < PTS_DIFF_BOUND
        if not pts_last_dts_epoch_start:
            logger.error(f"DTS of {to_tc(epoch[0].pcs.pts)} predates PTS of previous epoch by {epoch[0].pcs.tdts - prev_pts} ticks.")
        if not pts_dts_delta_epoch_start:
            logger.error(f"Incorrect PTS-DTS values for epoch start DS @ {to_tc(epoch[0].pcs.pts)}.")

        is_compliant &= pts_last_dts_epoch_start & pts_dts_delta_epoch_start

        for l, ds in enumerate(epoch):
            ds_comply = (ds.pcs.tdts - prev_dts) & PTS_MASK <= PTS_DIFF_BOUND
            ds_comply &= (ds.pcs.tpts - prev_pts) & PTS_MASK <= PTS_DIFF_BOUND

            if ds.wds:
                diff = (ds.pcs.tpts - prev_pts) & PTS_MASK
                ds_comply &= diff > pts_delta and diff < PTS_DIFF_BOUND
                ds_comply &= (ds.pcs.tpts - ds.wds.tpts) & PTS_MASK <= pts_delta
                ds_comply &= (ds.wds.tpts - ds.wds.tdts) & PTS_MASK <= wipe_duration*2
                ds_comply &= (ds.wds.tpts != ds.wds.tdts)
            else:
                ds_comply &= (ds.pcs.tpts - prev_pts) & PTS_MASK >= frame_duration
            for pds in ds.pds:
                ds_comply &= pds.tpts == pds.tdts
            for ods in ds.ods:
                ds_comply &= ods.tpts != ods.tdts
            for seg in ds:
                ds_comply &= (ds.pcs.tpts - seg.tpts) & PTS_MASK <= PTS_DIFF_BOUND
                ds_comply &= (seg.tdts - ds.pcs.tdts) & PTS_MASK <= PTS_DIFF_BOUND
                ds_comply &= (seg.tpts - ds.pcs.tdts) & PTS_MASK <= PTS_DIFF_BOUND
                ds_comply &= (seg.tpts - seg.tdts) & PTS_MASK <= PTS_DIFF_BOUND

                diff = (seg.tdts - prev_dts) & PTS_MASK
                ds_comply &= diff >= 0 and diff < PTS_DIFF_BOUND
                ds_comply &= (seg.tpts - seg.tdts) & PTS_MASK < PGDecoder.FREQ
                prev_dts = seg.tdts
            prev_pts = ds.pcs.tpts
            if not ds_comply:
                logger.error(f"Incorrect PTS-DTS at {to_tc(ds.pcs.pts)}, DS:S={ds.pcs.composition_state:02X}:PU={ds.pcs.pal_flag > 0}, stream is out of spec.")
            is_compliant &= ds_comply
    return is_compliant
