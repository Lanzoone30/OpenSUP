# Cierre spec 029 (2026-10-18)

### Fixed

- **MUI PTS/DTS writer (D6)**: MUI records rebuild a 39-bit timestamp and re-shift
  it `>>6`; the writer now emits `PTS<<6` so `from_mui` round-trips exactly
  (SUP → MUI → from_mui produces readable, exact PTS/DTS and `check_integrity()`
  passes).
- **Acquisition PCS re-acquisition (D7)**: epoch s with a re-acquired palette now
  re-emits ODS/PDS per window (spec-027 interleaved OIDs, D-029-2 numbering by
  window) and the multi-window single-pass route clears windows via WDS instead of
  a palette-end wipe. Known debt at the time: 11 display sets of the ENIGMA-1080p
  battery still over-listed retained windows in the acquisition PCS (closed as
  spec 030).
