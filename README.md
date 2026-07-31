<img src="assets/OpenSUP-GUI.png" alt="OpenSUP" width="180" height="48"/>

### PGS Subtitle Encoder for Blu-ray

**Turn BDN XML subtitles into Blu-ray compliant .sup / .pes streams**

<br>

[![Version](https://img.shields.io/badge/version-1.0.0-blue?style=flat-square)]()
[![License](https://img.shields.io/badge/license-GPLv3-green?style=flat-square)]()
[![Platform](https://img.shields.io/badge/platform-windows%20%7C%20linux-666666?style=flat-square)]()

<br>

---

## Overview

OpenSUP converts **BDN XML** subtitle files — images included — into **PGS** (Presentation Graphic Stream) subtitles for Blu-ray: compliant **.sup** or **.pes/.mui** streams ready for authoring.

For Blu-ray authors and subtitle encoders who work with BDN XML workflows and need a reliable, fast encoder. Built on [SUPer](https://github.com/cubicibo/SUPer) by cubicibo, rewritten from Python to C++17 for performance.

---

## Features

- **Blu-ray ready** — converts BDN XML subtitles, images included, into compliant .sup streams
- **Two output formats** — .sup and .pes/.mui, or both in a single run
- **Easy to use** — pick your subtitle file, choose a destination, press ENCODE
- **Dark / Light / System themes** — match your preference
- **English and Spanish** — switch languages at any time
- **Live progress** — progress bar and time remaining, abort whenever you want
- **Activity log** — see every step with color-coded results; copy or clear it
- **Two quality modes** — maximum quality or faster encoding
- **Command line support** — for scripting and batch jobs
- **Portable ZIP or Windows installer** — no extra dependencies to install
- **Remembers your settings** — theme and language persist between sessions
- **Free and open source** — GPLv3

---

## Screenshot

<div align="center">
  <img src="assets/images-preview/GUI-Preview.png" alt="OpenSUP GUI" width="900"/>
  <p><em>OpenSUP GUI — Light theme, ready to encode</em></p>
</div>

---

## Download

Windows builds are available on the [Releases](https://github.com/Lanzoone30/OpenSUP/releases) page:

- **ZIP** — extract and run `OpenSUP.exe`

---

## GUI Reference

### File Selection

| Control        | Function                                                                 |
| -------------- | ------------------------------------------------------------------------ |
| **Select BDN** | Load a BDN XML file                                                      |
| **Set Output** | Choose the destination path; `.sup` is appended automatically if missing |

### Parameters

| Control         | Options                   | Function                                                           |
| --------------- | ------------------------- | ------------------------------------------------------------------ |
| **Color Space** | BT.709 / BT.601 / BT.2020 | Color matrix for YCbCr conversion                                  |
| **Quantizer**   | libimagequant / HexTree   | Quantization backend: libimagequant for quality, HexTree for speed |

### Engine Options

| Checkbox                         | Function                                                                                                                                                                                                                                                                                               |
| -------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Allow Normal Case**            | Updates only one of the two PGS composition objects when there is not enough time to update both. Exploits the PG object buffer as intended by the format designers. Stream shall NOT be Built or Rebuilt at the authoring stage.                                                                      |
| **Prefer Normal Case**           | Updates only one of the two compositions even when decode time is sufficient to refresh both. Can reduce the bitrate, but the palette is not shared across composition objects when it occurs. Checking it forces **Allow Normal Case** on and locks it.                                               |
| **Write Full Palette**           | Uses the full 256-entry palette instead of an optimized subset when there are too many colors. May improve quality in rare cases at the cost of a larger output.                                                                                                                                       |
| **Both SUP + PES/MUI**           | Also exports a .pes/.mui file alongside the .sup file in a single run.                                                                                                                                                                                                                                 |
| **Overlap Buffering**            | Allows generating overlapping objects in the output stream. More efficient, but not well supported by some hardware decoders.                                                                                                                                                                          |
| **Ignore Resolution Validation** | By default, encoding fails if the video resolution is not one of the standard Blu-ray resolutions (1920×1080, 1280×720, 720×576, 720×480). Enable to ignore Blu-ray standards and encode anyway. **Use at your own risk** — the resulting stream may not be Blu-ray compliant or may play incorrectly. |

### Theme & Language

| Setting      | Options               |
| ------------ | --------------------- |
| **Theme**    | System / Light / Dark |
| **Language** | English / Spanish     |

---

## CLI Reference

```bash
./opensup_cli input.xml output.sup -b bt709 -y
```

| Flag                  | Description                                           |
| --------------------- | ----------------------------------------------------- |
| `-i, --input`         | BDN XML input file (required)                         |
| `output`              | Output .sup file path (positional, required)          |
| `-c, --compression`   | Compression rate [0-100] (default 80)                 |
| `-a, --acqrate`       | Acquisition rate [0-100] (default 100)                |
| `-q, --quantizer`     | Quantizer: 0 = libimagequant, 1 = HexTree (default 0) |
| `-b, --bt`            | Color matrix: bt601, bt709, bt2020 (default bt709)    |
| `-y, --yes`           | Overwrite existing output                             |
| `-t, --threads`       | Thread count (0 = auto)                               |
| `--ssim-tol`          | SSIM tolerance [0-100] (default 0)                    |
| `--ignore-resolution` | Accept non-standard video resolutions                 |
| `-w, --withsup`       | Generate both .sup and .pes/.mui                      |
| `-p, --palette`       | Write full 256-entry palette                          |
| `--allow-normal`      | Enable normal-case optimization                       |
| `--overlap`           | Enable overlap buffering                              |
| `--redraw-period`     | Redraw period in seconds                              |
| `-v, --version`       | Print version and exit                                |

> **Note:** `-c`, `-a` and `-t` are parsed but not yet wired to the encode pipeline (stubs).

**Exit codes:** `0` on success, `1` on failure (parse error, output exists without `-y`, encode failure).

---

## Project Structure

```
OpenSUP-dev/
├── assets/              Icons, logos, and preview screenshots
├── src/opensup/         Source code
│   ├── common/          Shared utilities
│   ├── media/           Media layer (palette, PGS graphics, streams, optimizer)
│   ├── core/            Core engine (BDN parser, renderer, segments, file I/O)
│   ├── cli/             CLI entry point
│   └── gui/             Qt6 GUI (main window, worker, theme, translations)
├── tests/               Google Test suite
├── tools/               Build scripts
├── CMakeLists.txt       Root build configuration
├── LICENSE              GPLv3
└── README.md
```

---

## License

GPL v3 — see [LICENSE](LICENSE).
Copyright (C) 2024-2026 Lanzoone30. Based on [SUPer](https://github.com/cubicibo/SUPer) by cubicibo.

---

## Credits

- **[SUPer](https://github.com/cubicibo/SUPer)** — original Python implementation by cubicibo; design adapted into OpenSUP
- **[libimagequant](https://github.com/imageoptim/libimagequant)** — high-quality color quantization (GPL-3.0-or-later)
- **[stb_image](https://github.com/nothings/stb)** — public-domain image loading
- **[pugixml](https://github.com/zeux/pugixml)** — BDN XML parsing (MIT)
