# -*- mode: python ; coding: utf-8 -*-
"""
PyInstaller spec for OpenSUP - PGS Subtitle Encoder.

Copyright (C) 2024-2026 cibo
This file is part of OpenSUP, based on SUPer <https://github.com/cubicibo/SUPer>.

Usage:
    pyinstaller opensup.spec --noconfirm --clean

Builds:
    dist/OpenSUP.exe              (single-file executable)
    dist/lib/libimagequant_x86_64.dll  (copied externally, not inside .exe)
    dist/bin/pngquant.exe              (copied externally, not inside .exe)
"""

import os
import shutil
from pathlib import Path

# ── Analysis ─────────────────────────────────────────────
a = Analysis(
    ['src/opensup/__main__.py'],
    pathex=['src'],
    binaries=[],     # Vacío: libimagequant.dll se copia por separado (post-process)
    datas=[
        # pyproject.toml necesario para _find_project_root() en modo frozen
        ('pyproject.toml', '.'),
        # icono de la aplicación
        ('assets/OpenSup.ico', 'assets'),
    ],
    hiddenimports=['brule', 'piliq', 'cv2'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=['PyQt5', 'PySide2', 'PySide6', 'numba',
              'matplotlib', 'scipy', 'IPython', 'setuptools'],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    noarchive=False,
)

# ── PYZ (Python bytecode archive) ────────────────────────
pyz = PYZ(a.pure, a.zipped_data)

# ── Executable (onefile: todo dentro del .exe) ───────────
exe = EXE(
    pyz,
    a.scripts,
    a.binaries,      # Vacío (libimagequant.dll va externo)
    a.zipfiles,
    a.datas,
    name='OpenSUP',
    icon='assets/OpenSup.ico',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)

# ── Post-process: copiar DLLs fuera del .exe ─────────────
dist_dir = Path('dist')
lib_src = Path('src/opensup/lib/libimagequant_x86_64.dll')
bin_src = Path('src/opensup/bin/pngquant.exe')

lib_dst = dist_dir / 'lib' / 'libimagequant_x86_64.dll'
bin_dst = dist_dir / 'bin' / 'pngquant.exe'

if lib_src.exists():
    lib_dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(lib_src, lib_dst)
    print(f"Copied {lib_src} -> {lib_dst}")

if bin_src.exists():
    bin_dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(bin_src, bin_dst)
    print(f"Copied {bin_src} -> {bin_dst}")
