# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['/home/lanzoone30/Desarrollo/Proyectos_Personales/Testing/OpenSUP/OpenSUP/main.py'],
    pathex=['/home/lanzoone30/Desarrollo/Proyectos_Personales/Testing/OpenSUP/OpenSUP/src'],
    binaries=[],
    datas=[('/home/lanzoone30/Desarrollo/Proyectos_Personales/Testing/OpenSUP/OpenSUP/bin', 'bin'), ('/home/lanzoone30/Desarrollo/Proyectos_Personales/Testing/OpenSUP/OpenSUP/lib', 'lib'), ('/home/lanzoone30/Desarrollo/Proyectos_Personales/Testing/OpenSUP/OpenSUP/tools', 'tools'), ('/home/lanzoone30/Desarrollo/Proyectos_Personales/Testing/OpenSUP/OpenSUP/pyproject.toml', '.')],
    hiddenimports=['piliq', 'brule'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=['PyQt5', 'PySide2', 'PySide6', 'numba', 'matplotlib', 'scipy', 'IPython', 'setuptools'],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='OpenSUP',
    debug=False,
    bootloader_ignore_signals=False,
    strip=True,
    upx=True,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=True,
    upx=True,
    upx_exclude=[],
    name='OpenSUP',
)
