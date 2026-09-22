# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.0.0] - 2026-09-19

### Added

- **Physical core detection** in the Threads selector (UI plus `x/sys`), replacing the numeric
  options with `Auto` and the detected physical cores.
- **Epoch parallelism**: thread pool built on `std::async` and exposed as `-j/--threads`
  (range 0-1024), with hash-based determinism so parallel output matches sequential.
- **Max bitrate limit** (`-m/--max-kbps`): validates the output against the decoder leaky-bucket
  buffer model; covered by `pgstream_test`.
- **Bundle drought parameters**: `compression`, `acqrate`, `ssim_tol` and `extra_acq` (engine
  plus four UI inputs and i18n).
- **Tile-based SSIM 7x7** with no external dependencies, a recursive area-weighted CTU comparator
  and extra acquisitions driven by a decode-margin model.
- **Multi-window emission** with normal-case, ref-first order and partial refresh.
- **`--alternate-oids`**: per-window OID alternation (double buffering in the overlap pipeline).
- **Ahead/overlap pipeline**: `find_acqs` and `shift_forward_overlay`.
- **JSON output** (`--json`) with an NDJSON event protocol for the engine, consumed by the Go
  runner as a subprocess.
- **ETA progress** in the interactive CLI terminal.
- **Window layout engine** with content partitioning and group solving (`solve_group` with
  co-quantization and zero dither).
- **UI redesign**: teal palette, light and dark themes, advanced options in collapsible cards,
  status LED, custom dropdowns, tooltips and a progress bar with `clip-path` clipping and a
  continuous fill.

### Changed

- Migrated the UI from **Qt6 to Wails v2** (HTML/CSS/JS frontend with embedded Go bindings).
- Refactored the encoder: `encode_epoch` split into `quantize_image`, `compute_timings` and
  `emit_event_segments`.
- Rebuilt the Threads selector around physical cores, with the sequential default made explicit.
- Made the SSIM threshold resolution-dependent, with banker's rounding matching the original.
- Split the frontend into **per-component modules** (JS, CSS and assets co-located with their
  component).
- Split CTU fixtures into PNG and moved test output to `temp/test_output`.
- Ported and synchronized the EN/ES translations.

### Removed

- **Qt6 GUI**, replaced by Wails v2.
- **`--prefer-normal`**: dropped from the CLI, UI and bindings. It behaved as a no-op; the C++
  plumbing stays reserved for the faithful implementation (spec 013).
- Simplified **pgraphics** to the RLE encoder (`encode_rle`); the unused decoder, object buffer,
  prospective object and palette manager were deleted.
- Removed the reuse tests and their fixtures.

### Fixed

- Immediate process abort and cancellation event in the runner.
- Video format detection by width/height instead of file extension.
- `FrameRate` parsing against malformed XML.
- Log level read outside the lock.
- Engine logs redirected to **stderr** so they no longer pollute the NDJSON stdout.

[Unreleased]: https://github.com/Lanzoone30/OpenSUP/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/Lanzoone30/OpenSUP/compare/v1.1.0...v2.0.0
