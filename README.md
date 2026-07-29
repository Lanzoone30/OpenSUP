<img src="assets/OpenSUP-GUI.png" alt="OpenSUP Logo" width="180" height="48" align="right"/>

# OpenSUP

### PGS Subtitle Encoder for Blu-ray

**Turn BDN XML subtitles into Blu-ray compliant .sup / .pes streams**

<br>

[![Version](https://img.shields.io/badge/version-1.0.0-blue?style=flat-square)]()
[![License](https://img.shields.io/badge/license-GPLv3-green?style=flat-square)]()
[![C++](https://img.shields.io/badge/c++-17-00599C?logo=cplusplus&style=flat-square)]()
[![CMake](https://img.shields.io/badge/cmake-3.16%2B-064F8C?logo=cmake&style=flat-square)]()
[![Qt6](https://img.shields.io/badge/Qt6-GUI-41CD52?logo=qt&style=flat-square)]()
[![Platform](https://img.shields.io/badge/platform-windows%20%7C%20linux-666666?style=flat-square)]()

<br>

---

## Overview

OpenSUP encodes **PGS** (Presentation Graphic Stream) subtitles for Blu-ray. It takes **BDN XML** files and produces compliant **.sup** or **.pes** streams.

Built on [SUPer](https://github.com/cibo02/SUPer) by cibo. Rewritten from Python to C++17 for performance.

---

## Screenshot

<div align="center">
  <img src="assets/images-preview/GUI-Preview.png" alt="OpenSUP GUI" width="900"/>
  <p><em>OpenSUP GUI — Dark theme, ready to encode</em></p>
</div>

---

## Features

- **BDN XML parsing** — reads standard subtitle interchange format  
- **PGS encoding** — compliant .sup / .pes output for Blu-ray  
- **Qt6 GUI** — dark/light/system theme with palette adaptation  
- **CLI** — 12 flags for scripting and automation  
- **Dual quantizer** — libimagequant (quality) or HexTree (speed fallback)  
- **Multi-threaded** — parallel epoch encoding with OpenMP  
- **Cross-compile** — Windows builds from Linux via MinGW  
- **NSIS / ZIP** — installer or portable package via CPack  
- **75+ tests** — Google Test suite  

---

## Getting Started

### Option 1: Pre-built (Windows)

1. Download `OpenSUP-1.0.0-win64.zip` from [Releases](https://github.com/lanzoone30/OpenSUP/releases)  
2. Extract the ZIP  
3. Run `OpenSUP.exe`  

### Option 2: Build from Source

**Requirements:**  
- CMake 3.16+  
- GCC/G++ 15+ (Linux) or MinGW-w64 (Windows cross-compile)  
- Qt6 Widgets  
- Cargo (for libimagequant)  

**Linux (native):**  
```bash
cmake -B build -DBUILD_GUI=ON
cmake --build build -j$(nproc)
```

**Windows (cross-compile from Linux):**  
```bash
./tools/build_windows.sh
# Output: builds/windows/packages/OpenSUP-1.0.0-win64.zip
```

---

## GUI Reference

### File Selection

| Control | Function |
|---------|----------|
| **Select BDN** | Load a BDN XML file |
| **Set Output** | Choose the output path for the .sup file |

### Parameters

| Control | Options | Function |
|---------|---------|----------|
| **Color Space** | BT.709 / BT.601 / BT.2020 | Color matrix for YCbCr conversion |
| **Quantizer** | libimagequant / HexTree | Quantization backend: libimagequant for quality, HexTree for speed |
| **Ignore Resolution Validation** | On / Off | Skip XML resolution validation at your own risk |

### Engine Options

| Checkbox | Function |
|----------|----------|
| **Allow Normal Case** | Enables normal-case subtitle optimization. Reduces file size by encoding repeated characters more efficiently. Best for dialog-heavy subtitles. |
| **Prefer Normal Case** | Prioritizes normal-case optimization even when the analyzer detects mixed case. Overrides the default heuristic. |
| **Write Full Palette** | Writes the complete palette (256 entries) instead of an optimized subset. Safer for player compatibility. |
| **Both SUP + PES/MUI** | Generates both .sup and .pes/.mui output formats in a single run. |
| **Overlap Buffering** | Allows overlapping display regions in the output. Required for certain Blu-ray authoring workflows. |

### Theme & Language

## CLI Reference

```bash
./opensup_cli -i input.xml -o output.sup -c bt709 -a
```

| Flag | Description |
|------|-------------|
| `-i, --input` | BDN XML input file |
| `-o, --output` | Output .sup file path |
| `-c, --colorspace` | Color matrix: bt709, bt601, or bt2020 |
| `-a, --allow-normal` | Enable normal-case optimization |
| `-q, --quantizer` | 0 = libimagequant, 1 = HexTree |
| `-b, --both-formats` | Generate both .sup and .pes |
| `-t, --overwrite` | Overwrite existing output |
| `--ssim-tol` | SSIM tolerance (default 0.0) |
| `-p, --prefer-full-palette` | Full palette mode |
| `--redraw-period` | Redraw period in seconds |
| `--ignore-resolution` | Skip resolution validation |
| `-y, --overlap` | Enable overlap buffering |

---

## Architecture

```
src/opensup/
├── common/          Shared utilities (error, memory, geometry, logger, ssim)
├── media/           Media layer (palette, PGS graphics, streams, optimizer)
├── core/            Core engine (BDN parser, renderer, segments, file I/O)
├── cli/             CLI entry point (CLI11)
├── gui/             Qt6 GUI (main window, worker, theme, translations)
└── pch.h            Precompiled header
```

**Pipeline:**  

```
BDN XML → Parser → Epoch Splitter → Encoder → PGS Segments → .sup Writer
                                    ↓
                            Quantizer (libimagequant / HexTree)
```

**Key components:**  
- `bdn_render_c` — orchestrates the full encode pipeline  
- `epoch_encoder_c` — encodes individual epochs with quantization  
- `encode_worker_c` — runs encoding on a background QThread  
- `theme_manager` — manages dark/light/system palette switching  

---

## Project Structure

```
OpenSUP-dev/
├── assets/              Icons, logos, and preview screenshots
├── cmake/               CMake toolchain and modules
│   ├── mingw-w64-x86_64.cmake   MinGW cross-compile toolchain
│   ├── deploy_windows.cmake      Windows DLL deployment
│   └── CompilerWarnings.cmake    Compiler flags
├── extern/              External dependencies (libimagequant)
├── src/opensup/         Source code (see Architecture)
├── tests/               Google Test suite (75+ tests)
├── tools/
│   └── build_windows.sh Windows cross-compile script
├── builds/              Build output (gitignored)
│   ├── linux/
│   └── windows/
├── Referencias/         Reference material
├── CMakeLists.txt       Root build configuration
├── LICENSE              GPLv3
└── README.md
```

---

## Configuration

GUI settings persist automatically via QSettings (Windows registry / Linux config files):  
- Theme preference (System / Light / Dark)  
- Language preference (EN / ES)  

CLI flags override defaults per-invocation. See [CLI Reference](#cli-reference) above.

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `Qt6Core.dll not found` | Keep all DLLs in the same directory as the .exe |
| `platforms/qwindows.dll not found` | Keep the `platforms/` folder next to the .exe |
| NSIS installer fails on Linux | Install Wine or use the ZIP package |
| `stb_image.h not found` | Run `curl -sL -o extern/stb_image.h https://raw.githubusercontent.com/nothings/stb/master/stb_image.h` |
| Build fails with MinGW | Verify `x86_64-w64-mingw32-g++` is in PATH |
| Encoding is slow | Use HexTree quantizer (faster than libimagequant) |
| Output has color artifacts | Switch to BT.601 color space for standard-definition content |

---

## License

GPL v3 — see [LICENSE](LICENSE).  

Copyright (C) 2024-2026. Based on [SUPer](https://github.com/cibo02/SUPer) by cibo.

---

## Credits

- **[SUPer](https://github.com/cubicibo/SUPer)** — original Python implementation by cubicibo  
- **[libimagequant](https://github.com/imageoptim/libimagequant)** — high-quality color quantization