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

OpenSUP encodes **PGS** (Presentation Graphic Stream) subtitles for Blu-ray. It takes **BDN XML** files and produces compliant **.sup** or **.pes** streams ready for authoring.

Built on [SUPer](https://github.com/cibo02/SUPer) by cibo. Rewritten from Python to C++17 for performance and native distribution.

---

<div align="center">

| <img src="assets/images-preview/GUI-Preview.png" alt="OpenSUP GUI" width="900"/> | **Features** |
| :-------------------------------------------------------------------------------: | :----------- |
| | **Qt6 GUI** — dark/light/system theme with palette adaptation |
| | **Language** — EN/ES toggle, persists between sessions |
| | **Quantizers** — libimagequant (quality) + HexTree (speed) |
| | **Color Space** — BT.709, BT.601, BT.2020 |
| | **Overlap** — overlapping objects in output |
| | **Real-time Log** — monospace panel with progress and ETA |
| | **CLI** — 12 flags for scripting and automation |
| | **Cross-compile** — Windows builds from Linux via MinGW |
| | **NSIS Installer** — one-click Windows setup via CPack |
| | **Portable ZIP** — extract and run, no installation required |

</div>

## Features

- **BDN XML parsing** — reads standard subtitle interchange format
- **PGS encoding** — compliant .sup / .pes output for Blu-ray authoring
- **Multi-threaded** — parallel epoch encoding with OpenMP
- **75+ tests** — Google Test suite covering common, media, and core modules

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

## Installation

### Windows

Download the ZIP from Releases. Extract and run `OpenSUP.exe`. All DLLs are included.

To build an NSIS installer, install [NSIS](https://nsis.sourceforge.io/) and run:
```bash
cd builds/windows
cpack -G NSIS
```

### Linux

Build from source (see Getting Started above). The GUI requires Qt6 Widgets:
```bash
# Fedora
sudo dnf install qt6-qtbase-devel

# Ubuntu/Debian
sudo apt install qt6-base-dev
```

---

## Usage

### GUI

Run `OpenSUP.exe` (Windows) or `./build/src/opensup/gui/OpenSUP` (Linux).

1. Click **Select BDN** to load your BDN XML file
2. Set the output path (auto-generated if empty)
3. Configure options (quantizer, color space, overlap, etc.)
4. Click **ENCODE**
5. Monitor progress in the log panel

**Options:**
- **Theme** — System / Light / Dark (persists between sessions)
- **Language** — EN / ES (persists between sessions)
- **Quantizer** — libimagequant (quality) or HexTree (speed)
- **Color Space** — BT.709, BT.601, or BT.2020

### CLI

```bash
./opensup_cli -i input.xml -o output.sup -c bt709 -a
```

| Flag | Description |
|------|-------------|
| `-i, --input` | BDN XML input file |
| `-o, --output` | Output .sup file path |
| `-c, --colorspace` | Color matrix (bt709, bt601, bt2020) |
| `-a, --allow-normal` | Allow normal-case optimization |
| `-q, --quantizer` | 0=libimagequant, 1=HexTree |
| `-b, --both-formats` | Output both .sup and .pes |
| `-t, --overwrite` | Overwrite existing output |
| `-y, --ignore-resolution` | Ignore resolution mismatch |
| `--ssim-tol` | SSIM tolerance (default 0.0) |
| `-p, --prefer-full-palette` | Full palette mode |
| `--redraw-period` | Redraw period in seconds |
| `--ignore-resolution` | Skip resolution validation |

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
├── assets/              Icons and logos
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

**GUI settings** are stored via QSettings:
- Theme preference (System/Light/Dark)
- Language preference (EN/ES)

Settings persist automatically between sessions.

**CLI flags** override defaults per-invocation. See [CLI Options Reference](#cli-options-reference) above.

---

## Testing

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j$(nproc)
cd build && ctest
```

75+ tests covering common utilities, media encoding, and core engine.

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `Qt6Core.dll not found` | Ensure all DLLs are in the same directory as the .exe |
| `platforms/qwindows.dll not found` | Ensure `platforms/` folder exists next to the .exe |
| NSIS installer fails on Linux | Install Wine (`sudo dnf install wine`) or use ZIP instead |
| `stb_image.h not found` | Run `curl -sL -o extern/stb_image.h https://raw.githubusercontent.com/nothings/stb/master/stb_image.h` |
| Build fails with MinGW | Verify `x86_64-w64-mingw32-g++` is installed and in PATH |

---

## License

GPL v3 — see [LICENSE](LICENSE).

Copyright (C) 2024-2026. Based on [SUPer](https://github.com/cibo02/SUPer) by cibo.

---

## Credits

- **[SUPer](https://github.com/cibo02/SUPer)** — original Python implementation by cibo
- **[libimagequant](https://github.com/ColorMind/libimagequant)** — high-quality color quantization
- **[Qt6](https://www.qt.io/)** — cross-platform GUI framework
- **[CLI11](https://github.com/CLIUtils/CLI11)** — header-only CLI parser
- **[pugixml](https://github.com/zeux/pugixml)** — lightweight XML parser
- **[Google Test](https://github.com/google/googletest)** — C++ testing framework
