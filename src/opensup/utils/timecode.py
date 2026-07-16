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

import numpy as np

from typing import Union
from timecode import Timecode

from opensup.utils.bdvideo import BDVideo
from opensup.utils.geometry import MPEGTS_FREQ


class TC(Timecode):
    def __init__(self, fps, *args, **kwargs) -> None:
        if not isinstance(fps, BDVideo.FPS):
            fps = BDVideo.FPS(fps)
        super().__init__(fps.value, *args, **kwargs)
        self.fractional_fps = fps

    @classmethod
    def s2tc(cls, s: float, fps: float, drop_frame: bool = False) -> 'TC':
        s = s/(1 if float(fps).is_integer() else 1.001)
        r_tc = cls(round(fps, 2), start_seconds=s+1/fps+1e-8, force_non_drop_frame=True)
        r_tc.drop_frame = drop_frame
        return r_tc

    def to_pts(self) -> float:
        tpts = ((self.frames - 1)/self.fractional_fps.value)*MPEGTS_FREQ
        return (tpts.numerator//tpts.denominator)/MPEGTS_FREQ

    def __add__(self, other: Union['TC', int]) -> 'TC':
        tc = __class__(self.fractional_fps, frames=self.frames)
        tc.drop_frame = self.drop_frame

        if isinstance(other, __class__):
            assert other.fractional_fps == self.fractional_fps
            assert self.drop_frame == other.drop_frame == False
            tc.add_frames(other.frames)
        else:
            assert isinstance(other, int)
            tc.add_frames(other)
        return tc
