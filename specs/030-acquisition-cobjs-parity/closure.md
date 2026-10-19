# Cierre spec 030 (2026-10-18)

### Fixed

- **Acquisition PCS re-listing (D-A)**: the acquisition PCS of a display set now
  lists only the windows the event composites in that set; active-but-unpainted
  windows stay retained and no longer receive a cobj or a re-emitted ODS (the
  spec-029 over-generalization to every active window is removed, in both the
  epoch route and the per-window `--overlap` route). ENIGMA-1080p base battery:
  11 → 0 divergent display sets; non-cropped (ncbd) outputs stay byte-identical.
  The geometry check on ncbd remains a documented skip (known multi-`<Graphic>`
  divergence, spec 033).

### Gates

- baseline / final: `baseline_hashes_phase30.txt`, `final_hashes_phase30.txt`
  (only `cpp_base`/`cpp_an` → `2b1ed728…` and `cpp_an_ov` → `997cc0bb…` changed;
  all `py_*` and `nc_py_*` intact).
