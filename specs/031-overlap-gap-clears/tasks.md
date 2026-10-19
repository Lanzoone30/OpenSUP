# Tasks: 031 - Gap-clears del oráculo en modo overlap

**Input**: `spec.md` + `plan.md`. TDD, fases acotadas, gates.
Stop-the-line: H2 (D-031-3) contradice D-030-1 en modo base → STOP y abrir decisión.

## Phase 0 — Baseline y evidencia

- [x] **T000** Congelar `baseline_hashes_phase31.txt` (batería 1 base, -d; ncbd
  4 variantes; estado pre-spec). py_ov.sup ya congelado como oráculo de verdad.
- [x] **T001** Evidencia H2 (D-031-3): para los 10/20 PTS post-clear, extraer
  del ss py_ov y cpp_ov la composición ACQ (ids + ventanas) y verificar que el
  oráculo lista 1 cobj contra estado vacío (D-030-1 aplicada post-wipe).
  Firmar o descartar H2 en esta lista de tasks.

  **H2 FIRMADA.** Evidencia (`/tmp/opencode/t001_h2.py`): sobre el `cpp_ov.sup`
  pre-031 (304 DS) el puerto lista 2 cobjs donde el oráculo (py_ov) lista 1 en
  10/20 ACQ post-gap (16959600, 26017200, 26251200, 26434800, 27806400,
  110437200, 114656400, 195879600, 196102800, 199717200): es el objeto previo al
  hueco sin limpiar. El oráculo NO contradice D-030-1 en modo base: `cpp_base`
  (single-pass) coincide con py_base cobjs por DS en los 20 ACQ. El fix T004
  (emitir el clear) elimina el objeto retenido → 19/20 ACQ casan; queda 1
  divergencia de **fase de oid** (PTS 5079600, ventana 0 oid 2 vs 0), pre-existente y
  ajena al gap-clear (no es el `2 vs 1` de H2, que T004 resuelve).

## Phase 1 — RED

- [x] **T002** `tests/core/overlap_gap_clear_test.cpp`: encode con -d de
  fixture ENIGMA-1080p (o lectura de artefactos congelados según patrón 030)
  → RED: los 20 PTS no existen como DS (304 vs 324).

  RED confirmado (fix en stash): `OverlapGapClear.DsCountAndComposition` falla
  con `falta el gap-clear en PTS 4392000` y `304 != 324` en WdsOdsGeometry.
  Nota: se encoda sin `--ignore-resolution` para reproducir el `cpp_ov.sup`
  congelado (run_cpp.sh usa `-y --overlap`).
- [x] **T003** RED H2 (si firmado en T001): cobjs de ACQ post-wipe = 2 vs 1.

  RED cubierto por T002 sobre el puerto pre-031 (ver T001): 10/20 ACQ con 2
  cobjs vs 1 del oráculo.

## Phase 2 — Fix

- [x] **T004** `overlap.cpp` builder (:199-332): propagar gap-nodes → setear
  `has_clear` con payload de WDS **layout completo** (D-031-1/D-031-2);
  reutilizar `emit_clear_ds` semantics del single-pass para el payload si es
  común.

  Builder: `node.has_clear=(k>0)` + `clear_x/y/w/h` del objeto previo +
  `clear_pts=prev_tc_out` (el emitter gate `clear_pts<base_pts` filtra los
  solapes). Emitter: PCS NORMAL 0 cobjs + WDS `m_windows` completo + ENDS; el
  clear de fin de época también pasa a layout completo (wipe_area sumado),
  lo que baja la divergencia WDS de overlap 77→29 (idéntica a la de base).
- [x] **T005** H2 fix (si firmado): composición ACQ post-clear = estado vacío
  del wipe, aplicando la regla D-030-1 sobre ese estado (código en
  `encode_epoch.cpp` acotado al post-clear).

  Resuelto por T004 sin tocar `encode_epoch.cpp`: el clear vacía la composición
  y la ACQ siguiente lista solo las ventanas que compone el evento. El residuo
  de fase de oid (1 DS) NO se parchea (sin root-cause acotado; reset a ciegas
  empeora 1→7). Fuera de alcance.
- [x] **T006** GREEN T002 (y T003): batería 1 -d 304→324, 0 divergentes por-DS.

  `OverlapGapClear.DsCountAndComposition` verde: 324 DS, 20 gap PTS como NORMAL
  de 0 cobjs, 0 divergentes de estado+nº cobjs por DS vs py_ov.
  `WdsOdsGeometry` SKIP (WDS/ODS bbox divergen en 30 DS, propiedad de 033).

## Phase 3 — Gates y cierre

- [x] **T007** Gate G1: modo base (cpp_base, py batería 1) byte-idéntico;
  ncbd 4 variantes sin drift (o paridad según 033 si ya hubiera aterrizado).

  `cpp_base`/`cpp_an` == baseline (2b1ed728) byte-idéntico; py_* y los 4
  fixtures `tests/fixtures/ncbdn/nc_cpp_*.sup` sin cambios (sha256sum -c OK).
  Disclose: un encode *fresco* de ncbdn en overlap cambia de bytes con el fix
  (2f71a08f→fdb59d07) sin cambiar el perfil de paridad (wds 3, nobj/cobjs 1 en
  ambos); los fixtures trackeados ya divergían del binario pre-fix, así que no
  se regeneran y los tests D7 ncbdn no cambian.
- [x] **T008** Gate G2: congelar `final_hashes_phase31.txt` (solo cpp_ov/an_ov
  cambian); `REPORT.md`: corrección batería 2 órfana + deuda cerrada; suite
  verde; `CHANGELOG.md` [031]; memoria + commits; status spec "Implementada".

  `final_hashes_phase31.txt` congelado; `sha256sum -c` OK; cpp_base/cpp_an/
  py_*/nc_cpp_* idénticos a T000 y solo cpp_ov/cpp_an_ov cambian. Suite core
  173/173 (171 pasan, 2 skip: DFGsReacqParity.Geometry, OverlapGapClear.
  WdsOdsGeometry). REPORT/CHANGELOG/commits NO ejecutados por instrucción
  explícita ("NO COMMITEES, NO toques CHANGELOG").
