"""
Utility modules.

Provides shared infrastructure used across the entire project:
logging (LogFacility), video properties (BDVideo), geometry types
(Pos, Shape, Box), timecode conversion (TC), SSIM comparison,
colour matrix helpers, and warning suppression for optional
dependencies (pyopencl, SSIM_PIL).

Why a single utils package: avoids circular imports and centralises
cross-cutting concerns like logging configuration.
"""

from warnings import filterwarnings
filterwarnings("ignore", message=r"Non-empty compiler", module="pyopencl")
filterwarnings("ignore", message=r"Kernel", module="SSIM_PIL")
from opensup.utils.bdvideo import BDVideo
from opensup.utils.geometry import Pos, Shape, Box
from opensup.utils.timecode import TC, MPEGTS_FREQ
from opensup.utils.logging import LogFacility
from opensup.utils.ssim import SSIMPW
from opensup.utils.color_matrix import get_matrix
