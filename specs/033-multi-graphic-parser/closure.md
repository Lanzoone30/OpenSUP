# Cierre spec 033 (2026-10-19)

### Fixed

- **Parser multi-`<Graphic>` (canvas unión por evento)**: the BDN parser iterates
  every `<Graphic>` of an `<Event>`, builds the union box and a consolidated RGBA
  canvas (paste at relative offset) feeding the pipeline unchanged
  (`determine_windows`/window-split untouched). ncbd goes from the port WDS
  `(200,953,551x43)+(767,953,151x53)` to the oracle's
  `(200,953,718x53)+(1000,913,833x106)`, ODS `718x53`/`833x106`, cobjs per DS
  equal to the oracle across the 4 flag variants. `DFGsReacqParity.Geometry`
  leaves the skip and passes hard. Battery 1 (168 events, N=1) byte-identical —
  D-033-4 not opened.

### Acotaciones

- US2 validation (`declared==real` + no-overlap) applies to N>1 events only:
  applying it to N=1 broke `tests/fixtures/opensup_multiwindow.xml`
  (declares 200x60, bitmap 600x120); the oracle does assert N=1, so full parity
  would need that fixture fixed (outside this boundary).

### Gates

- baseline / final: `baseline_hashes_phase33.txt`, `final_hashes_phase33.txt`.
  Only `nc_cpp_*` changed (`92fbeb09 → 75087816` ×2, `2305a7bf → fd92c6a7` ×2);
  `nc_py_*` and battery-1 artifacts byte-identical to phase31.
