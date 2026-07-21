#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
optimizer.py

Image quantisation backends for palette-based PGS subtitles.

Supports multiple quantisation strategies: libimagequant (via
piliq), HexTree, Pillow, and Qtzr. Each backend reduces full-
colour bitmaps to 8-bit (or fewer) palette images suitable for
the PGS constraint of 256-colour per composition.

Why multiple backends: different source material responds better
to different algorithms. libimagequant gives best quality/speed
for photographic content, while HexTree/Pillow are useful fallbacks
when the native library is unavailable.
"""

# Copyright (C) 2024-2026 cibo
# This file is part of OpenSUP, based on SUPer <https://github.com/cubicibo/SUPer>.
#
# OpenSUP is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# OpenSUP is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with OpenSUP.  If not, see <http://www.gnu.org/licenses/>.

import os
import sys
import shutil
import ctypes
import ctypes.util
import numpy as np
import numpy.typing as npt
import cv2

from functools import lru_cache
from PIL import Image
from pathlib import Path
from typing import Optional, Union, Any
from collections.abc import Iterable
from enum import IntEnum, auto
try:
    from piliq import PILIQ, PNGQuantWrapper
except ImportError:
    PILIQ = None  # type: ignore
    PNGQuantWrapper = None  # type: ignore

try:
    from brule import HexTree, QtzrUTC
except ImportError:
    HexTree = None  # type: ignore
    QtzrUTC = None  # type: ignore

from opensup.media.palette import Palette, PaletteEntry
from opensup.utils.logging import LogFacility
from opensup.utils.color_matrix import get_matrix
from opensup.utils.ssim import SSIMPW

logger = LogFacility.get_logger('OpenSUP')


@lru_cache(maxsize=1)
def _find_project_root() -> Path:
    """Find project root by locating pyproject.toml upwards from this file."""
    current = Path(__file__).resolve().parent
    for _ in range(15):
        if (current / 'pyproject.toml').exists():
            return current
        current = current.parent
    # Fallback para modo compilado (PyInstaller/Nuitka)
    if getattr(sys, 'frozen', False):
        # PyInstaller: data files pueden estar en sys._MEIPASS o _internal/
        if hasattr(sys, '_MEIPASS'):
            meipass = Path(sys._MEIPASS)
            # --onefile: pyproject.toml está en MEIPASS
            if (meipass / 'pyproject.toml').exists():
                return meipass
            # --onedir: los data files están en _internal/
            if (meipass / '_internal' / 'pyproject.toml').exists():
                return meipass / '_internal'
            return meipass
        return Path(sys.executable).resolve().parent
    # Fallback: try to locate project root via piliq module location (pip install)
    try:
        import piliq
        piliq_dir = Path(piliq.__file__).resolve().parent.parent
        if (piliq_dir / 'pyproject.toml').exists():
            return piliq_dir
    except ImportError:
        pass
    return Path(__file__).resolve().parent.parent.parent.parent.parent


class FadeCurve(IntEnum):
    LINEAR = auto()
    QUADRATIC = auto()
    EXPONENTIAL = auto()

def resolve_resource_path(filename: str, subdir: str = '') -> Optional[Path]:
    """Resolve path for external resource (DLL, binary) in frozen and dev modes.
    
    In frozen --onefile mode, sys.argv[0] is the real .exe path (sys.executable
    points to a %TEMP% extraction), so we search next to sys.argv[0] first.
    Falls back to sys._MEIPASS, then development paths.
    """
    if getattr(sys, 'frozen', False):
        # --onefile mode: real .exe location
        exe_dir = Path(sys.argv[0]).resolve().parent
        external = exe_dir / subdir / filename if subdir else exe_dir / filename
        if external.exists():
            return external
        # Fallback: inside the .exe extraction (sys._MEIPASS)
        if hasattr(sys, '_MEIPASS'):
            internal = Path(sys._MEIPASS) / subdir / filename if subdir else Path(sys._MEIPASS) / filename
            if internal.exists():
                return internal
        return None
    # Development mode: search relative to this file
    base = Path(__file__).resolve().parent  # opensup/media/
    candidates = [
        base.parent / subdir / filename,           # opensup/{subdir}/filename
        base.parent.parent.parent / subdir / filename,  # project_root/{subdir}/filename
    ]
    if subdir:
        candidates.append(base / subdir / filename)  # opensup/media/{subdir}/filename
    for c in candidates:
        if c.exists():
            return c
    return None


class Quantizer:
    class Libs(IntEnum):
        QTZR  = 0
        PIL_KM  = 1
        HEXTREE = 2
        PILIQ   = 3
        PNGQNT  = 4

        @classmethod
        def _missing_(cls, v: Any) -> 'Quantizer.Libs':
            if isinstance(v, cls):
                v = v.value
            else:
                try:
                    v = int(v)
                except ValueError:
                    ...
            if v in [ev.value for ev in cls]:
                return cls(v)
            return cls(cls.HEXTREE.value)

    _opts = {}
    _piliq = None          # PILIQ instance (libimagequant binding)
    _alt_piliq = None      # PNGQuantWrapper-based instance
    _pngquant_path = None  # Standalone pngquant binary path (no piliq needed)
    @classmethod
    def get_options(cls) -> dict[int, (str, str)]:
        if cls._opts == {}:
            cls.find_options()
        return cls._opts

    @classmethod
    def get_option_id(cls, option_str: str) -> 'Quantizer.Libs':
        algo = option_str.strip().split(' ')[0]
        for opt_id, opt in cls._opts.items():
            if opt[0] == algo:
                return opt_id
        logger.error("Unknown quantizer library requested, returning default.")
        return 0

    @classmethod
    def find_options(cls) -> None:
        # Opciones en orden de enum (0..4)
        # QTZR (0)
        if QtzrUTC is not None:
            qtzr_info = '(better, fast)' if 'C' in QtzrUTC.get_capabilities() else '(good, slow)'
            cls._opts[cls.Libs.QTZR] = ('Qtzr', qtzr_info)
        else:
            cls._opts[cls.Libs.QTZR] = ('Qtzr (C)', '(requires brule)')
        # PIL_KM (1) - siempre disponible
        cls._opts[cls.Libs.PIL_KM] = ('Pillow', '(average, turbo)')
        # HEXTREE (2) - only show if C extension is available (no Python fallback)
        if HexTree is not None:
            hcaps = HexTree.get_capabilities()
            if hcaps and 'C' in hcaps:
                cls._opts[cls.Libs.HEXTREE] = ("HexTree", '(good, very fast)')
        # PILIQ (3) — via piliq (libimagequant binding)
        if cls._piliq is not None:
            cls._opts[cls.Libs.PILIQ] = (cls.get_piliq().lib_name,'(best, fast)')
        # PNGQNT (4) — via pngquant binary (piliq wrapper)
        if cls._alt_piliq is not None:
            cls._opts[cls.Libs.PNGQNT] = (cls._alt_piliq.lib_name,'(best, fast)')

    @classmethod
    def _find_libimagequant(cls) -> Optional[str]:
        """Find libimagequant shared library path (cross-platform).
        
        First checks external path (exe_dir/lib/) via resolve_resource_path
        (for --onefile with external DLLs), then system paths (Linux),
        then project tree fallback.
        """
        # Phase 0: External path (--onefile: junto al .exe)
        liq_candidates = (['libimagequant_x86_64.dll'] if os.name == 'nt'
                          else ['libimagequant.so', 'libimagequant.so.0'])
        for lc in liq_candidates:
            ext = resolve_resource_path(lc, 'lib')
            if ext is not None:
                return str(ext)

        project_root = Path(__file__).resolve().parent.parent.parent.parent

        # Phase A: System-level search (Linux: ldconfig, /usr/lib64, etc.)
        try:
            lib_name = ctypes.util.find_library('imagequant')
            if lib_name is not None:
                import subprocess as _sp
                try:
                    r = _sp.run(['ldconfig', '-p'], capture_output=True, text=True)
                    for line in r.stdout.split('\n'):
                        if 'libimagequant' in line:
                            parts = line.split('=>')
                            if len(parts) > 1:
                                lib_path = parts[1].strip()
                                if Path(lib_path).exists():
                                    return lib_path
                except FileNotFoundError:
                    pass
                for lib_dir in ['/usr/lib64', '/usr/lib', '/lib64', '/lib']:
                    for lib_file in sorted(Path(lib_dir).glob('libimagequant.so*')):
                        if lib_file.exists():
                            return str(lib_file)
        except Exception:
            pass

        # Phase B: Project tree search (cross-platform, for bundled DLLs/SOs)
        try:
            search_pattern = 'libimagequant*.dll' if os.name == 'nt' else 'libimagequant.so*'
            search_dirs = [
                project_root,
                project_root / 'bin',
                project_root / 'lib',
                Path(__file__).resolve().parent.parent / 'lib',
                project_root / 'Referencias' / 'GUI_win_x64_SUPer',
                project_root / 'Referencias' / 'GUI_win_x64_SUPer' / 'lib',
            ]
            for sdir in search_dirs:
                if sdir.is_dir():
                    for lib_file in sorted(sdir.glob(search_pattern)):
                        if lib_file.exists():
                            return str(lib_file)
        except Exception:
            pass

        return None

    @classmethod
    def _find_pngquant_binary(cls) -> Optional[str]:
        """Find pngquant binary in PATH, external dir, or project root."""
        try:
            # Phase 0: External path (--onefile: junto al .exe)
            png_candidates = ['pngquant.exe', 'pngquant'] if os.name == 'nt' else ['pngquant']
            for pc in png_candidates:
                ext = resolve_resource_path(pc, 'bin')
                if ext is not None:
                    return str(ext)

            found = shutil.which('pngquant')
            if found:
                return found
            project_root = _find_project_root()
            search_roots = [project_root, project_root / 'bin', Path(__file__).resolve().parent.parent / 'bin']
            candidates = ['pngquant.exe', 'pngquant'] if os.name == 'nt' else ['pngquant']
            for sroot in search_roots:
                for candidate in candidates:
                    cp = sroot / candidate
                    if cp.exists():
                        return str(cp)
            try:
                import piliq
                piliq_dir = Path(piliq.__file__).resolve().parent
                for sroot in [piliq_dir, piliq_dir.parent, piliq_dir / 'bin']:
                    for candidate in candidates:
                        cp = sroot / candidate
                        if cp.exists():
                            return str(cp)
            except ImportError:
                pass
        except Exception:
            pass
        return None

    @classmethod
    def init_piliq(cls,
        qpath: Optional[Union[str, Path]] = None,
        quality: Optional[int] = 100,
        speed: Optional[int] = 4,
        dither: Optional[int] = 100,
    ) -> bool:
        # Always try to find standalone pngquant binary first (works without piliq)
        cls._pngquant_path = cls._find_pngquant_binary()
        if cls._pngquant_path is not None:
            logger.debug(f"Found pngquant binary at: {cls._pngquant_path}")

        if PILIQ is None:
            logger.debug("piliq package not installed (advanced quantizers limited).")
            return False

        piliq = None

        # Phase 1: Pre-bind libimagequant if found (workaround for Fedora etc.)
        # This must happen BEFORE PILIQ() auto-detection so _LIQWrapper.is_ready() returns True
        if qpath is None:
            lib_path = cls._find_libimagequant()
            if lib_path is not None:
                try:
                    from piliq import _LIQWrapper as _liq
                    # Check if already bound
                    if not _liq.is_ready():
                        _liq.bind(lib_path)
                        logger.debug(f"Bound libimagequant from: {lib_path}")
                except Exception as exc:
                    logger.debug(f"Failed to bind libimagequant: {exc}")

        # Phase 2: Try with provided path or auto-detection
        try:
            piliq = PILIQ(qpath)
        except (FileNotFoundError, AssertionError):
            logger.debug(f"Failed to load advanced quantizer at '{qpath}'.")

        # Phase 3: If auto-detection failed, try already-found pngquant binary
        if piliq is None and qpath is None and cls._pngquant_path is not None:
            try:
                piliq = PILIQ(cls._pngquant_path)
                logger.debug(f"Loaded pngquant from: {cls._pngquant_path}")
            except (FileNotFoundError, AssertionError):
                logger.debug(f"Failed to load pngquant at '{cls._pngquant_path}'.")

        # Phase 4: If explicit path failed, try auto-detection as fallback
        if piliq is None and qpath is not None:
            try:
                piliq = PILIQ()
            except:
                logger.debug("Failed to load advanced quantizer with auto look-up.")

        cls._piliq = piliq
        success = False
        if piliq is not None and piliq.is_ready():
            logger.debug(f"Configuring {piliq.lib_name} with: speed={speed}:quality={quality}:dither={dither/100.0}")
            cls.write_piliq_config(piliq, speed, quality, dither)
            success = True
        if success and piliq.lib_name != 'pngquant':
            if PNGQuantWrapper is not None and PNGQuantWrapper.is_ready():
                cls._alt_piliq = PILIQ(_wrapper=PNGQuantWrapper())
                cls.write_piliq_config(cls._alt_piliq, speed, quality, dither)
        return success

    @staticmethod
    def write_piliq_config(piq_inst: 'PILIQ', speed: int, quality: int, dither: float) -> None:
        piq_inst.return_pil = False
        piq_inst.set_speed(speed)
        piq_inst.set_quality(quality)
        piq_inst.set_dithering_level(dither/100.0)

    @classmethod
    def select_quantizer(cls, option_id: int) -> int:
        if option_id > cls.Libs.PNGQNT:
            logger.error("Unknown quantizer ID '{option_id}', attempting to use piliq library.")
            option_id = cls.Libs.PILIQ

        if option_id == cls.Libs.PNGQNT:
            if cls._piliq is not None:
                if cls._piliq.lib_name != 'pngquant' and cls._alt_piliq is not None:
                    cls._piliq.destroy()
                    cls._piliq = cls._alt_piliq
                option_id = cls.Libs.PILIQ
            else:
                logger.error("Requesting specifically pngquant, but no quantizer available.")
                option_id = cls.get_brule_fallback()
        if option_id == cls.Libs.PILIQ and not cls.get_piliq():
            fallback = cls.get_brule_fallback()
            logger.error(f"Unable to find an advanced quantizer (pngquant, libimagequant). Using lower quality: {fallback.name}.")
            option_id = fallback
        return int(option_id)

    @classmethod
    def get_brule_fallback(cls) -> 'Quantizer.Libs':
        if HexTree is not None and 'C' in HexTree.get_capabilities():
            return cls.Libs.HEXTREE
        return cls.Libs.QTZR

    @classmethod
    def get_piliq(cls) -> Optional['PILIQ']:
        return cls._piliq

    @classmethod
    def log_selection(cls, idx: Union[int, 'Quantizer.Libs']) -> None:
        idxi = cls.Libs(idx)
        if idxi in [cls.Libs.QTZR, cls.Libs.HEXTREE]:
            lib = QtzrUTC if idxi == cls.Libs.QTZR else HexTree
            caps = [c for c in lib.get_capabilities() if c is not None] if lib is not None else []
            cap_string = f', capabilities: {", ".join(caps)}' if caps else ', (no capabilities)' if lib is not None else ', (library not available)'
        else:
            cap_string = ''
        logger.debug(f"RGBA quantizer '{idxi.name}'{cap_string}.")


class Preprocess:
    @classmethod
    def quantize(cls, img: Image.Image, colors: int = 256, **kwargs) -> tuple[npt.NDArray[np.uint8], npt.NDArray[np.uint8]]:
        quant_method = Quantizer.Libs(kwargs.pop('quantize_lib', Quantizer.Libs.HEXTREE))
        single_bitmap = kwargs.get('single_bitmap', False)

        if Quantizer.Libs.PILIQ == quant_method:
            if single_bitmap:
                nc = colors
            else:
                nc = len(img.quantize(colors, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE).palette.colors)

            lib_piq = Quantizer.get_piliq()
            assert lib_piq is not None

            if not single_bitmap:
                original_quality = lib_piq.get_quality()
                original_dither = lib_piq.get_dithering_level()
                lib_piq.set_quality(max(1, int(np.ceil(original_quality*0.975))))
                lib_piq.set_dithering_level(original_dither*0.9)

            pal, qtz_img = lib_piq.quantize(img, min(colors, int(np.ceil(20+nc*235/255))))
            if not single_bitmap:
                lib_piq.set_dithering_level(original_dither)
                lib_piq.set_quality(original_quality)
            return qtz_img, pal

        elif Quantizer.Libs.QTZR == quant_method:
            nk = len(img.quantize(colors, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE).palette.colors)
            nk = min(colors, int(np.ceil(20+nk*235/255)))
            return QtzrUTC.quantize(np.asarray(img, dtype=np.uint8), nk)

        elif Quantizer.Libs.HEXTREE == quant_method:
            nc = colors if single_bitmap else len(img.quantize(colors, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE).palette.colors)
            npimg = np.asarray(img, dtype=np.uint8)
            npbm, nppal = HexTree.quantize(npimg, max(16, min(colors, int(np.ceil(20+nc*235/255)))))
            return npbm, nppal

        else:
            odim = (img.height, img.width)
            oimg = img
            if min(odim) < 8:
                img_padded = Image.new('RGBA', (max(img.width, 8), max(img.height, 8)), (0, 0, 0, 0))
                img_padded.paste(img, (0, 0))
                img = img_padded
            img_out = img.quantize(colors, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE)
            npimg = np.asarray(img_out, dtype=np.uint8)
            nppal = np.asarray(list(img_out.palette.colors.keys()), dtype=np.uint8)

            pil_failed = len(img_out.palette.colors) != 1+max(img_out.palette.colors.values())

            pil_failed = pil_failed or SSIMPW.compare(Image.fromarray(nppal[npimg], 'RGBA'), img) < 0.95

            if pil_failed:
                logger.ldebug("Pillow failed to palettize image, falling back to HexTree.")
                return cls.quantize(oimg, colors, quantize_lib=Quantizer.Libs.HEXTREE, **kwargs)

            return npimg[:odim[0], :odim[1]], nppal

    @staticmethod
    def find_most_opaque(events: list[Image.Image]) -> int:
        if not isinstance(events, Iterable):
            return events

        a_max, idx = 0, -1

        for k, event in enumerate(events):
            tmp = np.linalg.norm(np.asarray(event)[:,:,3], ord=1)
            if tmp > a_max:
                a_max = tmp
                idx = k
        return idx

    @staticmethod
    def palettize_img(img: 'Image.Image', pal: npt.NDArray[np.uint8], *,
                      _return_mode: str = 'P') -> tuple['Image.Image', npt.NDArray[np.uint8]]:
        imga = np.asarray(img, np.int16)
        if pal is None:
            pal = np.asarray(list(img.convert('P').palette.colors.keys()), np.uint8)
        subs = np.asarray(imga - pal[:, None, None], dtype=np.int64)
        out = pal[np.einsum('ijkl,ijkl->ijk', subs, subs).argmin(0)].astype(np.uint8)
        out = Image.fromarray(out, 'RGBA').convert(_return_mode)
        return out, pal


class Optimise:
    @staticmethod
    def solve_sequence_fast(events, colors: int = 256, **kwargs) -> tuple[npt.NDArray[np.uint8], npt.NDArray[np.uint8]]:
        if 1 == len(events):
            img, clut = Preprocess.quantize(events[0], colors, single_bitmap=True, **kwargs)
            return img.copy(), np.expand_dims(clut, 1).copy()

        sequences = np.zeros((len(events), *events[0].size[::-1], 4), np.uint8)
        for ke, event in enumerate(events):
            img, clut = Preprocess.quantize(event, colors, single_bitmap=False, **kwargs)
            sequences[ke, :, :, :] = clut[img]
        sequences = np.moveaxis(sequences, 0, 2)

        seq_occ: dict[int, tuple[int, npt.NDArray[np.uint8]]] = {}
        for i in range(sequences.shape[0]):
            for j in range(sequences.shape[1]):
                seq = sequences[i, j, :, :]
                hsh = hash(seq.tobytes())
                try:
                    seq_occ[hsh][0] += 1
                except KeyError:
                    seq_occ[hsh] = [1, seq]

        seq_sorted = {k: x[1] for k, x in sorted(seq_occ.items(), key=lambda item: item[1][0], reverse=True)}
        seq_ids = {k: z for z, k in enumerate(seq_sorted.keys())}

        norm_mat = np.ndarray((colors, *sequences[i,j,:,:].shape[0:2]))

        remap: dict[int, int] = {}
        for cnt, v in enumerate(seq_sorted.values()):
            if cnt < colors:
                norm_mat[cnt, :, :] = v
            else:
                nm = np.linalg.norm(norm_mat - v[None, :], 2, axis=2)

                id1 = np.argsort(np.sum(nm, axis=1))
                id2 = np.argsort(np.sum(nm, axis=1)/np.sum(nm != 0, axis=1))

                best_fit = np.abs(id1 - id2[:, None])
                remap[cnt] = id1[best_fit.argmin() % id1.size]
        del norm_mat

        bitmap = np.zeros(sequences.shape[0:2], dtype=np.uint8)
        for i in range(sequences.shape[0]):
            for j in range(sequences.shape[1]):
                seq = sequences[i, j, :, :]
                hsh = hash(seq.tobytes())
                if seq_ids[hsh] < colors:
                    bitmap[i, j] = seq_ids[hsh]
                else:
                    bitmap[i, j] = remap[seq_ids[hsh]]
        return bitmap, np.asarray([seq for seq, _ in zip(seq_sorted.values(), range(colors))], dtype=np.uint8)


    @classmethod
    def solve_and_remap(cls, events: list[Image.Image], colors: int = 255, first_index: int = 1, **kwargs) -> tuple[npt.NDArray[np.uint8], npt.NDArray[np.uint8]]:
        assert 0 < first_index + colors <= 256, "8-bit ID out of range."
        assert first_index > 0, "Usage of palette ID zero."

        bitmap, cluts = cls.solve_sequence_fast(events, colors, **kwargs)
        transparent_id = np.nonzero(np.all(cluts[:,:,-1] == 0, axis=1))[0]

        kwargs_diff = {'matrix': kwargs.get('bt_colorspace', 'bt709')}

        if 0 == len(transparent_id):
            if np.max(bitmap) == colors - 1:
                logger.ldebug("Too many colours used, lowering count.")
                bitmap, cluts = cls.solve_sequence_fast(events, colors-1, **kwargs)
            palettes = cls.diff_cluts(cluts, **kwargs_diff)
            bitmap += first_index
        else:
            if max(transparent_id) == (0xFF - first_index):
                transparent_id = 0xFF - first_index
                bitmap += first_index
            else:
                transparent_id = int(transparent_id[0])
                tsp_mask = (bitmap == transparent_id)
                smaller = bitmap < transparent_id
                larger = bitmap > transparent_id
                bitmap[smaller] += first_index
                bitmap[larger] += (first_index - 1)
                bitmap[tsp_mask] = 0xFF
            cluts = np.delete(cluts, [transparent_id], axis=0)
            palettes = cls.diff_cluts(cluts, **kwargs_diff)

        for pal in palettes:
            pal.offset(first_index)
        assert len(palettes[0]) < colors
        return bitmap, palettes

    @staticmethod
    def diff_cluts(cluts: npt.NDArray[np.uint8], /, *,
                   matrix: str = 'bt709') -> list[Palette]:
        stacked_cluts = np.swapaxes(cluts, 1, 0).astype(np.int32)
        matrix = get_matrix(matrix, False)

        shape = stacked_cluts.shape
        stacked_cluts = np.round(np.matmul(stacked_cluts.reshape((-1, 4)), matrix.T))
        stacked_cluts += np.asarray([[16, 128, 128, 0]])
        clip_vals = (np.array([[16, 16, 16, 0]]), np.asarray([[235, 240, 240, 255]]))
        stacked_cluts = np.clip(stacked_cluts, *clip_vals).astype(np.uint8).reshape(shape)
        stacked_cluts = stacked_cluts[:, :, [0, 2, 1, 3]]
        l_pal = []
        for j, clut in enumerate(stacked_cluts):
            pal = Palette()
            for k, pal_entry in enumerate(clut):
                n_e = PaletteEntry(*pal_entry)
                if j == 0:
                    pal[k] = n_e
                    continue

                for bw in range(j-1, -1, -1):
                    p_e = l_pal[bw].get(k, None)
                    if p_e == n_e:
                        break
                    if p_e is not None and p_e != n_e:
                        pal[k] = n_e
                        break
            l_pal.append(pal)
        return l_pal

    @staticmethod
    def eval_animation(cmap: npt.NDArray[np.uint8], sequence: npt.NDArray[np.uint8],
                       ret_array: bool = False) -> Union[Optional[list[Image.Image]],
                                                   Optional[npt.NDArray[np.uint8]]]:
        anim = np.moveaxis(sequence[cmap], [2,], [0,]).astype(np.uint8)
        if ret_array:
            return anim
        return [Image.fromarray(anim[k], 'RGBA') for k in range(len(anim))]

    @classmethod
    def show(cls, ri = True, cmap: Optional[npt.NDArray[np.uint8]] = None,
             cluts: Optional[npt.NDArray[np.uint8]] = None,
             imgs: Optional[list[Image.Image]] = None) -> None:
        if cmap is not None or cluts is not None:
            if not(cmap is not None and cluts is not None):
                raise ValueError("Missing color map or CLUT sequence.")
            ret = cls.eval_animation(cmap, cluts)
        elif imgs:
            ret = imgs

        wt, ht = 0, 0
        for event in ret:
            if event.width > wt:
                wt = event.width
            ht += event.height

        stack = Image.new('RGBA', (wt,ht), (0, 0, 0, 0))
        heights = []
        for k, event in enumerate(ret):
            stack.paste(event, (0, sum(heights)))
            heights.append(event.height)
        if ri:
            return stack
        else:
            stack.show()
