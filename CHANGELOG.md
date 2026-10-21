# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.1.0] - 2026-10-21

### Added

- **`--tight-ods` engine flag with a "Fit ODS box to text" checkbox**: the
  object box now fits the subtitle content instead of the whole window slot.
- **`--ignore-resolution` engine flag with an "Ignore resolution validation"
  checkbox**: encodes BDN files with non-standard (non-Blu-ray) canvas sizes.
  The engine warns instead of failing, and rejects zero or negative dimensions
  so a bad BDN cannot produce an unplayable stream.
- **SUP inspector**: new `inspect`, `validate`, and `stats` subcommands read
  finished `.sup` streams, with human or `--json` output per display set,
  compliance checks (PTS/DTS ordering, bitrate, truncation), and a segment
  census.
- **Engine Options persistence**: the eight checkboxes save to and restore
  from the settings file. "Prefer normal case" turns on "Allow normal case"
  when you check it first, like the original GUI behavior.
- **`-k/--prefer-normal` flag and CLI surface parity**: quantizer IDs
  (`-q 0..4`) fail fast for backends not built in, plus new short flags and
  aliases.

### Changed

- **Per-window objects in multi-window events**: each window now gets its own
  graphic object and palette cropped from a shared palette solve, instead of
  one box for the whole event. Object IDs interleave per window, and
  single-window events keep their old scheme.
- **Wipe and clear timing**: wipe durations use the ceiling of summed window
  areas (was the sum of per-window ceilings), the WDS timestamp guard prevents
  half-tick drift, and clears during overlap gaps and at the end of an epoch
  declare the full epoch layout, so subtitles no longer linger on screen
  between events.
- **Buffer-guard fallback**: splits whose padded windows would exceed the
  16-entry bitmap buffer limit fall back to a single window.
- **Geometry parity**: vertical padding follows the reference branch, and
  per-window object buffers double-buffer with an epoch-start reset, so
  re-acquired epochs keep stable IDs.

### Fixed

- **Palettes in hard cases**: full palettes are written in non-normal display
  sets and palette rebakes, so players that empty their palette on normal
  events no longer show invisible text.
- **Multi-window and overlap streams**: one merged PDS per display set with
  re-anchored ODS/ENDS timestamps, and windows keep their object IDs across
  re-acquisitions.
- **Multi-Graphic BDN files**: events built from several `<Graphic>` blocks now
  compute a union canvas, and acquisition display sets list only the windows
  the event actually paints, so windows without content stop re-acquiring
  objects every event.
- **MUI timestamps round-trip exactly**: the writer emits `PTS<<6`, which
  reconstructs the original 39-bit PTS/DTS values.
- **libimagequant**: handles are released when remapping fails, instead of
  leaking on error paths.
- **CLI error messages**: a non-standard-resolution BDN is rejected with a
  clear explanation, and the quantizer help no longer lists backends
  unavailable in that build.

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
  plumbing stays reserved for a faithful future implementation.
- Simplified **pgraphics** to the RLE encoder (`encode_rle`); the unused decoder, object buffer,
  prospective object and palette manager were deleted.
- Removed the reuse tests and their fixtures.

### Fixed

- Immediate process abort and cancellation event in the runner.
- Video format detection by width/height instead of file extension.
- `FrameRate` parsing against malformed XML.
- Log level read outside the lock.
- Engine logs redirected to **stderr** so they no longer pollute the NDJSON stdout.

[Unreleased]: https://github.com/Lanzoone30/OpenSUP/compare/v2.1.0...HEAD
[2.1.0]: https://github.com/Lanzoone30/OpenSUP/compare/v2.0.0...v2.1.0
[2.0.0]: https://github.com/Lanzoone30/OpenSUP/compare/v1.1.0...v2.0.0
