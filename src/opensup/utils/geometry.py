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

from typing import TypeVar
from dataclasses import dataclass


MPEGTS_FREQ = np.uint64(90e3)
_BaseEvent = TypeVar('BaseEvent')


@dataclass
class Pos:
    x: int
    y: int

    def __iter__(self):
        return iter((self.x, self.y))


@dataclass
class Shape:
    w: int
    h: int

    def __post_init__(self) -> None:
        assert self.w >= 0 and self.h >= 0

    @classmethod
    def from_box(cls, box: 'Box') -> 'Shape':
        return cls(box.dx, box.dy)

    @classmethod
    def union(cls, *shapes) -> 'Shape':
        w = max(map(lambda dim: dim.w, shapes))
        h = max(map(lambda dim: dim.h, shapes))
        return cls(w, h)

    @property
    def area(self) -> int:
        return self.w*self.h

    @property
    def width(self) -> int:
        return self.w

    @property
    def height(self) -> int:
        return self.h

    def __eq__(self, other):
        if isinstance(other, self.__class__):
            return self.w == other.w and self.h == other.h
        elif isinstance(other, (tuple, list)) and len(other) == 2:
            return self.w == other[0] and self.h == other[1]
        return NotImplemented

    def __ne__(self, other):
        test_eq = self.__eq__(other)
        if isinstance(test_eq, bool):
            return not test_eq
        return NotImplemented

    def __iter__(self):
        return iter((self.w, self.h))


@dataclass(frozen=True)
class Box:
    y : int
    dy: int
    x : int
    dx: int

    @property
    def x2(self) -> int:
        return self.x + self.dx

    @property
    def y2(self) -> int:
        return self.y + self.dy

    @property
    def area(self) -> int:
        return self.dx * self.dy

    @property
    def coords(self) -> tuple[int, int, int, int]:
        return (self.x, self.y, self.x2, self.y2)

    @property
    def dims(self) -> Shape:
        return Shape(self.dx, self.dy)

    @property
    def shape(self) -> tuple[int, int]:
        """
        Return the numpy-like shape (col, row)
        """
        return (self.dy, self.dx)

    @property
    def pos_shape(self) -> tuple[Pos, Shape]:
        return Pos(self.x, self.y), Shape(self.dx, self.dy)

    @property
    def slice(self) -> tuple[slice]:
        return (slice(self.y, self.y+self.dy),
                slice(self.x, self.x+self.dx))

    @property
    def slice_x(self) -> slice:
        return slice(self.x, self.x+self.dx)

    @property
    def slice_y(self) -> slice:
        return slice(self.y, self.y+self.dy)

    def overlap_with(self, other) -> float:
        intersect = __class__.intersect(self, other)
        return intersect.area/min(self.area, other.area)

    @classmethod
    def intersect(cls, *box) -> 'Box':
        x2 = min(map(lambda b: b.x2, box))
        y2 = min(map(lambda b: b.y2, box))
        x1 = max(map(lambda b: b.x, box))
        y1 = max(map(lambda b: b.y, box))
        dx, dy = (x2-x1), (y2-y1)
        return cls(y1, dy * bool(dy > 0), x1, dx * bool(dx > 0))

    @classmethod
    def from_slices(cls, slices: tuple[slice]) -> 'Box':
        if len(slices) == 3:
            slyx = slices[1:]
        else:
            slyx = slices
        f_ZWz = lambda slz : (int(slz.start), int(slz.stop-slz.start))
        return cls(*f_ZWz(slyx[0]), *f_ZWz(slyx[1]))

    @classmethod
    def union(cls, *box) -> 'Box':
        x2 = max(map(lambda b: b.x2, box))
        y2 = max(map(lambda b: b.y2, box))
        x1 = min(map(lambda b: b.x, box))
        y1 = min(map(lambda b: b.y, box))
        return cls(y1, y2-y1, x1, x2-x1)

    @classmethod
    def from_events(cls, events: list[_BaseEvent]) -> 'Box':
        if len(events) == 0:
            raise ValueError("No events given.")

        pxtl, pytl = np.inf, np.inf
        pxbr, pybr = 0, 0
        for event in events:
            pxtl = min(pxtl, event.x)
            pxbr = max(pxbr, event.x + event.width)
            pytl = min(pytl, event.y)
            pybr = max(pybr, event.y + event.height)
        return cls(int(pytl), int(pybr-pytl), int(pxtl), int(pxbr-pxtl))

    @classmethod
    def from_coords(cls, x1: int, y1: int, x2 : int, y2: int) -> 'Box':
        return cls(min(y1, y2), abs(y2-y1), min(x1, x2), abs(x2-x1))

    def __eq__(self, other: 'Box') -> bool:
        if isinstance(other, __class__):
            return self.coords == other.coords
        return NotImplemented
