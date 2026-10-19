# Cierre spec 031 (2026-10-19)

### Fixed

- **Overlap route gap-clears**: the overlap builder now marks intra-epoch gaps
  (`>=1` frame) and emits the oracle's gap-clear display sets (PCS NORMAL 0 cobjs,
  WDS with the full epoch layout, ENDS), and every clear in the overlap route
  (including end-of-epoch) uses the full layout with summed wipe area instead of a
  single last-object bbox. ENIGMA-1080p with `--overlap`: 304 → 324 display sets
  matching the oracle; base-mode and ncbd outputs stay byte-identical, and
  `cpp_ov` matches `cpp_an_ov` exactly like the oracle pair.

### Residuales

- 1 post-clear ACQ pre-existing oid-phase difference (PTS 5079600, window 0 oid
  2 vs 0; double-buffer reset test worsened 1 → 7, left out of scope).
- ncbd WDS/ODS geometry divergence owned by spec 033 (kept as test skip).

### Gates

- baseline / final: `baseline_hashes_phase31.txt`, `final_hashes_phase31.txt`
  (only overlap-family artifacts changed: `cpp_ov` `c5754228… → 97badc03…`,
  `cpp_an_ov` `997cc0bb… → 97badc03…`; base and ncbd byte-identical).
