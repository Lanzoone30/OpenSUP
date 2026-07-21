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

from numpy import typing as npt
from functools import lru_cache


@lru_cache(maxsize=6)
def get_matrix(matrix: str, to_rgba: bool) -> npt.NDArray[np.uint8]:
    """
    Getter of colorspace conversion matrix, BT ITU, limited or full
    :param matrix:       Conversion (BTxxx)
    :param range:        'limited' or 'full'
    :return:             Matrix
    """

    cc_matrix = {
        'bt601': {'y2r':   np.array([[1.164,       0,  1.596, 0],
                                     [1.164,  -0.392, -0.813, 0],
                                     [1.164,   2.017,      0, 0],
                                     [    0,       0,      0, 1]]),
                  'r2y':   np.array([[ 0.257,  0.504,  0.098, 0],
                                     [-0.148, -0.291,  0.439, 0],
                                     [ 0.439, -0.368, -0.071, 0],
                                     [     0,      0,      0, 1]]),
        },
        'bt709': {'y2r':   np.array([[1.164,      0,   1.793, 0],
                                     [1.164, -0.213,  -0.533, 0],
                                     [1.164,  2.112,       0, 0],
                                     [    0,      0,       0, 1]]),
                  'r2y':   np.array([[ 0.183,  0.614,  0.062, 0],
                                     [-0.101, -0.339,  0.439, 0],
                                     [ 0.439, -0.399, -0.040, 0],
                                     [     0,      0,      0, 1]]),
        },
        'bt2020': {'y2r':  np.array([[1.16439,      0,1.67867,0],
                                     [1.16439,-.18734,-.65042,0],
                                     [1.16439,2.14175,      0,0],
                                     [     0,      0,       0,1]]),
                   'r2y':  np.array([[0.22561,0.58228,0.05093,0],
                                     [-.12266,-.31656,0.43922,0],
                                     [0.43922,-.40389,-.03533,0],
                                     [      0,      0,      0,1]]),
        },
    }
    mat = cc_matrix.get(matrix, None)
    if mat is None:
        raise NotImplementedError("Unknown/Not implemented conversion standard.")
    return mat["y2r" if to_rgba else "r2y"]
