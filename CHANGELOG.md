# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- **Per-window ODS emission in multi-window epochs (spec 027)**: windows with relevant
  intersection now get their own ODS/palette per event (cropped from a shared packed
  palette solve instead of one full-event bounding box), OIDs follow the oracle
  interleaved scheme (`o_id = window ordinal + DB flip × N`, single-window `{0,1}`
  unchanged), and the WDS declares every window even when no object covers it.
- **DS timing parity (D3)**: wipe duration is now the ceiling of the summed window
  areas (was sum of per-window ceilings) and the WDS PTS half-tick nudge was replaced
  by the oracle's decode+1 guard.
- **Buffer guard (DB1)**: splits whose post-pad window areas exceed the 16-entry
  bitmap buffer ceiling (`(1<<20)-150`) degrade to a single window in
  `determine_windows` and `encode_epoch`, matching the oracle's
  `may_not_fit_buffer → base_box` fallback.

### Fixed

- **Single-pass multi-window display set parity (spec 028)**: the multi-window merge in
  the single-pass route now emits one display set per event (unique ENDS, per-window
  PDS/ODS with window OIDs, no extra acquisitions), and intermediate multi-window epochs
  end in a WDS clear instead of a palette-end wipe, matching the oracle exactly
  (324/168/156/0, 43 ACQUISITIONs); ghost epochs without PCS are eliminated (52 → 0).
- **MUI PTS/DTS writer (spec 029, D6)**: MUI records rebuild a 39-bit timestamp and
  re-shift it `>>6`; the writer now emits `PTS<<6` so `from_mui` round-trips exactly
  (SUP → MUI → from_mui produces readable, exact PTS/DTS and `check_integrity()` passes).
- **Acquisition PCS re-acquisition (spec 029, D7)**: epoch s with a re-acquired
  palette now re-emits ODS/PDS per window (spec-027 interleaved OIDs, D-029-2 numbering
  by window) and the multi-window single-pass route clears windows via WDS instead of a
  palette-end wipe.
- **Acquisition PCS re-listing (spec 030, D-A)**: the acquisition PCS of a display set
  now lists only the windows the event composites in that set; active-but-unpainted
  windows stay retained and no longer receive a cobj or a re-emitted ODS (the spec-029
  over-generalization to every active window is removed, in both the epoch route and
  the per-window `--overlap` route). ENIGMA-1080p base battery: 11 → 0 divergent
  display sets; non-cropped (ncbd) outputs stay byte-identical. The geometry check on
  ncbd remains a documented skip (known multi-`<Graphic>` divergence, spec 033).

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
