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

from enum import Enum, IntEnum
from fractions import Fraction
from typing import Optional, Union


class BDVideo:
    _LUT_PCS_FPS = {
        23.976:0x10,
        24:    0x20,
        25:    0x30,
        29.97: 0x40,
        50:    0x60,
        59.94: 0x70,
        60:    0x80,
    }

    class FPS(Enum):
        HFR_60 = Fraction(60, 1)
        NTSCi  = Fraction(60000, 1001)
        PALi   = Fraction(50, 1)
        NTSCp  = Fraction(30000, 1001)
        PALp   = Fraction(25, 1)
        FILM   = Fraction(24, 1)
        FILM_NTSC = Fraction(24000, 1001)

        @classmethod
        def from_pcsfps(cls, pcsfps: int) -> 'BDVideo.FPS':
            return cls(next(filter(lambda v: v[1] == pcsfps, BDVideo._LUT_PCS_FPS.items()))[0])

        def to_pcsfps(self) -> 'BDVideo.PCSFPS':
            rfps = round(float(self), 2)
            return BDVideo.PCSFPS(next(filter(lambda v: rfps == round(v[0],2), BDVideo._LUT_PCS_FPS.items()))[1])

        @property
        def exact_value(self):
            if int(self.value) != self.value:
                return (np.ceil(self.value)*1e3)/1001
            return self.value

        @classmethod
        def _missing_(cls, value: Union[float, int]) -> 'BDVideo.FPS':
            candidates = [fps.value for fps in __class__]
            best_fit = list(map(lambda x: abs(x-value), candidates))
            best_idx = best_fit.index(min(best_fit))
            if best_fit[best_idx] < 0.07:
                return cls(candidates[best_idx])
            raise ValueError("Framerate is not BD compliant.")

        def __float__(self) -> float:
            return float(self.value)

        def __int__(self) -> int:
            return int(self.value)

        def __round__(self, ndigits: int = 0):
            return round(self.value, ndigits)

        def __truediv__(self, other: Union[Fraction, float, int]) -> Union[Fraction, float]:
            return self.value/other

        def __rtruediv__(self, other: Union[Fraction, float, int]) -> Union[Fraction, float]:
            return other/self.value

        def __mul__(self, other: Union[Fraction, float, int]) -> Union[Fraction, float]:
            return self.value*other

        def __rmul__(self, other: Union[Fraction, float, int]) -> Union[Fraction, float]:
            return self.__mul__(other)

        def __float__(self) -> float:
            return float(self.value)

        def __gt__(self, other):
            if isinstance(other, __class__):
                return self.value > other.value
            elif isinstance(other, (int, float, Fraction)):
                return self.value > other
            return NotImplemented

        def __lt__(self, other):
            if isinstance(other, __class__):
                return self.value < other.value
            elif isinstance(other, (int, float, Fraction)):
                return self.value < other
            return NotImplemented

        def __ne__(self, other) -> bool:
            test_eq = self.__eq__(other)
            if test_eq == NotImplemented:
                return test_eq
            return not test_eq

        def __eq__(self, other) -> bool:
            if isinstance(other, (float, int, Fraction)):
                try:
                    return __class__(other).value == self.value
                except ValueError:
                    return False
            elif isinstance(other, __class__):
                return other.value == self.value
            else:
                return NotImplemented

    class VideoFormat(Enum):
        HD1080    = (1920, 1080)
        HD720     = (1280, 720)
        SD576_43  = (720,  576)
        SD480_43  = (720,  480)

        @property
        def area(self) -> int:
            return self.value[0]*self.value[1]

        @classmethod
        def from_height(cls, height: int) -> 'BDVideo.VideoFormat':
            for fmt in cls:
                if fmt[1] == height:
                    return cls(*fmt)
            raise ValueError(f"Unknown video format with height '{height}'.")

    class PCSFPS(IntEnum):
        FILM_NTSC_P = 0x10
        FILM_24P    = 0x20
        PAL_P       = 0x30
        NTSC_P      = 0x40
        PAL_I       = 0x60
        NTSC_I      = 0x70
        HFR_60      = 0x80

    def __init__(self, fps: float, height: int, width: Optional[int] = None) -> None:
        self.fps = __class__.FPS(fps)
        self.pcsfps = self.fps.to_pcsfps()
        if width is None:
            self.format = None
            for vf in __class__.VideoFormat:
                if vf.value[1] == height:
                    self.format = vf
                    break
            assert self.format is not None
        else:
            self.format = __class__.VideoFormat((width, height))

    @classmethod
    def check_format_fps(cls, _format: 'BDVideo.VideoFormat', fps: Union[float, 'BDVideo.FPS', Fraction]) -> bool:
        valid = True
        fps = cls.FPS(fps)
        expected = [_fps for _fps in cls.FPS]
        if _format == cls.VideoFormat.HD720:
            expected = [cls.FPS.FILM_NTSC, cls.FPS.FILM, cls.FPS.PALi, cls.FPS.NTSCi]
            valid &= fps in expected
        elif _format == cls.VideoFormat.SD576_43:
            expected = [cls.FPS.PALp]
            valid &= fps in expected
        elif _format == cls.VideoFormat.SD480_43:
            expected = [cls.FPS.NTSCp]
            valid &= fps in expected
        return valid, list(map(lambda x: round((float if x.value.denominator == 1001 else int)(x), 3), expected))
