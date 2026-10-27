<img src="assets/OpenSUP-GUI.png" alt="OpenSUP" width="180" height="48" align="right" style="margin-top: 60px"/>

### PGS Subtitle Encoder

**Turn BDN XML subtitles into Blu-ray compliant .sup / .pes streams**

[![Version](https://img.shields.io/badge/version-2.2.0-blue?style=flat-square)](https://github.com/Lanzoone30/OpenSUP/releases)
[![License](https://img.shields.io/badge/license-GPLv3-green?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-windows%20%7C%20linux-666666?style=flat-square)](https://github.com/Lanzoone30/OpenSUP/releases)

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Screenshot](#screenshot)
- [Download](#download)
- [Quick Start](#quick-start)
- [GUI Reference](#gui-reference)
- [CLI Reference](#cli-reference)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Troubleshooting](#troubleshooting)
- [Technical notes](#technical-notes)
- [License](#license)
- [Credits](#credits)

---

## Overview

OpenSUP converts **BDN XML** subtitle files, images included, into **PGS** (Presentation Graphic
Stream) subtitles for Blu-ray. It produces compliant **.sup** streams, and **.pes/.mui** when
needed.

The engine handles the whole process in a single run: parsing the XML, loading the embedded images,
quantizing colors, and assembling the final stream. A bilingual GUI and a full-featured CLI share
the same pipeline.

The encoder core is **C++17**, rewritten from [SUPer](https://github.com/cubicibo/SUPer) by
cubicibo. The desktop app is **Go + Wails v2** with a vanilla HTML/CSS/JS frontend that drives the
embedded engine.

---

## Features

- **Blu-ray ready**: converts BDN XML subtitles, images included, into compliant .sup streams
- **Two output formats**: .sup and .pes/.mui, or both in a single run
- **Quality or speed**: two quantization engines, one tuned for maximum quality and one for faster
  encoding
- **Color accuracy**: BT.709 / BT.601 / BT.2020 color space presets
- **Bitrate control**: validate the output against a maximum bitrate and tune quality/speed
  trade-offs
- **Activity log**: see every step with color-coded results; copy or clear it
- **Command line support**: for scripting and batch jobs, with NDJSON output for automation

---

## Screenshot

<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/images-preview/GUI-Preview_light.png">
    <source media="(prefers-color-scheme: light)" srcset="assets/images-preview/GUI-Preview_dark.png">
    <img src="assets/images-preview/GUI-Preview_dark.png" alt="OpenSUP GUI" width="700"/>
  </picture>
  <p><em>OpenSUP GUI</em></p>
</div>

---

## Download

Prebuilt builds are available on the
[Releases](https://github.com/Lanzoone30/OpenSUP/releases) page.

**Windows** (x86_64)

- **GUI ZIP**: `OpenSUP-GUI-2.2.0-windows-amd64.zip` — extract and run `OpenSUP.exe`

**Linux** (x86_64)

- **GUI RPM**: `sudo dnf install ./opensup-gui-2.2.0-1.x86_64.rpm`
- **GUI DEB**: `sudo apt install ./opensup-gui_2.2.0_amd64.deb`

Both Linux packages register OpenSUP in your application menu. They require
`webkit2gtk 4.1` and `GTK 3`, installed automatically as dependencies.

**CLI** (Windows and Linux, x86_64)

- `OpenSUP-CLI-2.2.0-windows-amd64.zip` and `OpenSUP-CLI-2.2.0-linux-amd64.zip`
  contain the standalone `opensup_cli`; no GUI needed.

---

## Quick Start

1. **Select BDN XML**: load the subtitle file (images included).
2. **Set SUP Output**: choose where the `.sup` will be written.
3. **Tune parameters**: pick a quantizer and color space; adjust advanced options if needed.
4. **Press ENCODE**: follow progress and the activity log, or ABORT at any time.

A BDN XML file can also be dragged and dropped onto the left panel.

---

## GUI Reference

### File Selection

| Control            | Function                                                                 |
| ------------------ | ------------------------------------------------------------------------ |
| **Select BDN**     | Load a BDN XML file                                                      |
| **Set SUP Output** | Choose the destination path; `.sup` is appended automatically if missing |

### Parameters

| Control         | Options                   | Function                                                           |
| --------------- | ------------------------- | ------------------------------------------------------------------ |
| **Quantizer**   | libimage (best, fast) / HexTree (good, very fast) | Quantization backend: libimage for quality, HexTree for speed |
| **Color Space** | BT.709 / BT.601 / BT.2020 | Color matrix for YCbCr conversion                                  |

<details open>
<summary><strong>Advanced options</strong></summary>

| Control             | Range         | Default | Function                                                                                                |
| ------------------- | ------------- | ------- | ------------------------------------------------------------------------------------------------------- |
| **Threads**         | Auto / 1.._N_ | Auto    | Parallel epoch encoding workers. _N_ is the number of detected physical cores; 0 = Auto, 1 = sequential |
| **Anchor interval** | ≥ 0 s         | 0       | Anchor a redraw every N seconds (0 = disable)                                                           |
| **Max bitrate**     | 0..48000 Kbps | 0       | Validate the output against a maximum bitrate (0 = off)                                                 |
| **Compression**     | 0..100        | 80      | Compression/quality factor (0 = force all acquisitions)                                                 |
| **Acq. Rate**       | 0..100        | 100     | Acquisition rate / drought scaling                                                                      |
| **SSIM Tol.**       | -100..100     | 0       | SSIM tolerance, adjusted per resolution                                                                 |
| **Extra Acq**       | 0..100        | 2       | Insert an acquisition after N palette updates (0 = off)                                                 |

</details>

### Engine Options

<details open>
<summary><strong>Output and authoring flags</strong></summary>

| Checkbox                         | Function                                                                                                                                                                                          |
| -------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Allow Normal Case**            | Updates only one of the two PGS composition objects when there is not enough time to update both. Exploits the PG object buffer. The stream shall NOT be Built or Rebuilt at the authoring stage. |
| **Prefer Normal Case**           | Updates only one composition object even when decoding time allows both. Lowers the bitrate, but the palette is not shared across composition objects when it happens. Implies Allow Normal Case. |
| **Overlap Buffering**            | Buffers palette updates whenever possible so fewer events are dropped. The stream shall NOT be Built or Rebuilt at the authoring stage.                                                           |
| **Write Full Palette**           | Writes the full 256-entry palette for every epoch instead of an optimized subset. Can improve quality at the cost of a larger output.                                                             |
| **Fit ODS Box to Text**          | Sets the ODS box to the event content instead of the whole window slot, for authoring tools that crop or rescale graphics.                                                                        |
| **One Object per Display Set (Android TV compatibility)** | Collapses each event into a single window so every display set carries one object. Enable it if Media3-based Android TV clients (Wholphin, Moonfin) render PGS cut or incomplete. Forces Full Palette and disables Overlap Buffering and the Normal Case options while on. | 
| **Alternate OIDs**               | Forces legacy global object ids 0/1. The default is per-window ids, which keeps multi-window compositions out of trouble.                                                                         |
| **Both SUP + PES/MUI**           | Also exports a .pes/.mui file alongside the .sup file in a single run.                                                                                                                            |
| **Ignore Resolution Validation** | By default, encoding fails if the video resolution is not one of the standard Blu-ray resolutions (1920×1080, 1280×720, 720×576, 720×480). Enable to ignore Blu-ray standards and encode anyway.  |

</details>

> [!WARNING]
> The **Ignore Resolution Validation** option skips checks for Blu-ray format compliance. Some
> events may not play correctly. Use with caution.

### Theme & Language

| Setting      | Options               |
| ------------ | --------------------- |
| **Theme**    | System / Light / Dark |
| **Language** | English / Spanish     |

---

## CLI Reference

The CLI shares the exact same engine as the GUI.

```bash
./opensup_cli -i input.xml output.sup -b bt709 -y
```

| Flag                  | Description                                                           |
| --------------------- | --------------------------------------------------------------------- |
| `-i, --input`         | BDN XML input file (required)                                         |
| `output`              | Output .sup file path (positional, required)                          |
| `-q, --quantizer`     | Quantizer: `0` QtzrUTC, `1` Pillow, `2` HexTree, `3` PNGQ/LIQ (default 3; only 2 and 3 are built in) |
| `-b, --bt`            | Color space: bt601, bt709, bt2020 (default bt709)                     |
| `-y, --yes`           | Overwrite existing output                                             |
| `-w, --withsup`       | Generate both .sup and .pes/.mui                                      |
| `-p, --palette`       | Write full palette for all epochs                                     |
| `-n, --allow-normal`  | Allow normal-case object redefinition                                 |
| `-k, --prefer-normal` | Prefer normal-case redefinition (implies `-n`)                        |
| `-d, --ahead`         | Allow palette update buffering (alias `--overlap`, deprecated)        |
| `--alternate-oids`    | Force legacy global object ids 0/1 (default: per-window ids)          |
| `--ignore-resolution` | Accept non-standard video resolutions                                  |
| `--tight-ods`         | Crop each event's ODS box tight to its text (default: off)            |
| `--single-object-ds`  | Emit at most one object per display set (Media3/Android TV compat)    |
| `--redraw-period`     | Anchor interval in seconds (0 = disable, minimum 1)                   |
| `-m, --max-kbps`      | Validate against a max bitrate in Kbps [0-48000, 0 = off] (default 0) |
| `-t, --threads`       | Parallel epoch workers [0 = auto, 1 = sequential] (default 0; `-j` deprecated) |
| `-c, --compression`   | Compression/quality factor [0-100] (default 80)                       |
| `-a, --acqrate`       | Acquisition rate [0-100] (default 100)                                |
| `--ssim-tol`          | SSIM tolerance [-100..100] (default 0)                                |
| `-e, --extra-acq`     | Insert acquisition after N palette updates [0 = off] (default 2)      |
| `--config`            | INI overrides for libimagequant ([PILIQ] quality/speed/dither)        |
| `--json`              | Emit NDJSON events to stdout (log / progress / done)                  |
| `-v, --version`       | Print version and exit                                                |

### Inspector

The CLI can read a finished `.sup` without re-encoding it:

```bash
./opensup_cli inspect movie.sup --ds 12      # dump display set 12 (default: all)
./opensup_cli validate movie.sup --json      # compliance findings as NDJSON
./opensup_cli stats movie.sup                # segment census
```

| Subcommand | Options                | Description                                                        |
| ---------- | ---------------------- | ------------------------------------------------------------------ |
| `inspect`  | `--ds N`               | Dump the structure of a `.sup` file, one display set or all        |
| `validate` | `--json`, `--max-kbps` | Check PTS/DTS ordering, truncation and (optionally) decoder buffer |
| `stats`    | `--json`               | Census counters: segments, display sets, epochs, object sizes      |

**Exit codes:** `0` on success, `1` on failure (parse error, output exists without `-y`, encode
failure), `2` when the inspector cannot read the input file.

---

## Architecture

The Go runtime spawns the C++ engine as a child process and talks to it over **NDJSON** on stdio, so
the GUI never links the encoder directly.

![OpenSUP architecture](assets/architecture.svg)

The Wails v2 desktop app hosts the frontend and the Go binding layer; the engine runner spawns
`opensup_cli`, whose orchestrator (`bdn_render_c`) parses the BDN XML, splits it into epochs encoded
in parallel, and assembles the PGS segments written as `.sup` / `.pes` / `.mui`. The same binary
serves the read-only inspector subcommands (`inspect` / `validate` / `stats`), which walk a finished
`.sup` through the reader in `read/` and report structure and validation findings.

---

## Project Structure

```text
.
├── src/opensup/          C++17 engine
│   ├── common/           Shared utilities
│   │   ├── bdvideo       Blu-ray video formats and frame rates
│   │   ├── color_matrix  BT.601 / BT.709 / BT.2020 YCbCr conversion
│   │   ├── error         Exception hierarchy
│   │   ├── geometry      Point / shape / box helpers
│   │   ├── logger        Logging facility
│   │   ├── memory        Reference-counted byte buffers
│   │   ├── ssim          SSIM comparison
│   │   └── timecode      Timecode / PTS helpers
│   ├── media/            PGS media layer
│   │   ├── hextree_impl  HexTree quantizer core (ported from brule, MIT)
│   │   ├── optimizer     Quantization backends (libimagequant / HexTree)
│   │   ├── palette       Palette and palette-entry handling
│   │   ├── pgraphics     PGS graphics RLE encoder
│   │   └── rle_decode    RLE decoder for the inspector
│   ├── core/             Core engine
│   │   ├── ctu           Recursive area-weighted SSIM comparison
│   │   ├── filestreams   BDN XML parser, .sup / .pes writers
│   │   ├── interface     Encode pipeline orchestration (bdn_render_c)
│   │   ├── layout_engine Window split layout (1 or 2 windows)
│   │   ├── pgstats       Stream census and compliance statistics
│   │   ├── pgstream      Bitrate / decoder buffer model (leaky bucket)
│   │   ├── render/       Epoch encoder: compose, quantize, palette, timings, overlap
│   │   └── segments      PGS segment types (PCS, WDS, PDS, ODS, ENDS)
│   ├── read/             SUP reader and stream inspector core
│   └── cli/              CLI entry point (CLI11), NDJSON emitter, inspector subcommands
├── ui/                   Wails v2 desktop app
│   ├── app.go            Go binding layer
│   ├── config/           Settings persistence + physical-core detection
│   ├── engine/           Engine runner and embedded per-OS binaries
│   ├── i18n/             English / Spanish strings
│   └── frontend/         Vanilla HTML/CSS/JS frontend (Vite)
├── cmake/                Toolchains and version header template
└── CMakeLists.txt        Build entry point
```

---

## Troubleshooting

- **Encoding fails on an unusual resolution**: only standard Blu-ray resolutions are accepted by
  default. Enable **Ignore Resolution Validation** (`--ignore-resolution`) to override, accepting
  the compliance risk.
- **Some hardware decoders reject the stream**: disable palette update buffering (`-d/--ahead`);
  buffering drops fewer events but is not universally supported.
- **Parallel output differs from sequential**: it should not. `-t N` is verified to produce
  byte-identical output to `-t 1`; if you observe otherwise, please report it.

---

## Technical notes

- **Determinism**: multithreaded encoding (`-t N`) is byte-identical to sequential (`-t 1`).
- **Parity**: the generated `.sup` structure matches the original SUPer output; the CLI is verified
  against a SubtitleEdit reference stream.
- **Compactness**: output was measured smaller than the SubtitleEdit reference on the test corpus
  used during development.

---

## License

GPL v3, see [LICENSE](LICENSE).
Copyright (C) 2026 Lanzoone30. Based on [SUPer](https://github.com/cubicibo/SUPer) by cubicibo.

---

## Credits

- **[SUPer](https://github.com/cubicibo/SUPer)**: original Python implementation by cubicibo; design
  adapted into OpenSUP
- **[libimagequant](https://github.com/ImageOptim/libimagequant)**: high-quality palette
  quantization
- **[brule](https://github.com/cubicibo/brule)**: HexTree quantizer and layout engine core (MIT)
- **[CLI11](https://github.com/CLIUtils/CLI11)** and **[pugixml](https://github.com/zeux/pugixml)**:
  CLI parsing and XML
- **[Wails](https://wails.io)**: Go desktop application framework
