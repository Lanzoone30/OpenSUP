# Tasks: 029 - Paridad MUI PTS y re-adquisición con reutilización de gráficos

**Input**: `spec.md` + `plan.md` (mismo directorio). Convención: TDD (RED antes que
código), fases acotadas, gates con hashes como en 025-028.

**Reglas de ejecución**:
- Cada fase termina con la suite ctest en verde y el gate documentado.
- Ningún archivo fuera del patch boundary del plan (filestreams.cpp MUI writer,
  encode_epoch.cpp cobjs, overlap.cpp cobjs + tests).
- Los tests de invariantes definen "hecho" (US1/US2).

**Cierre (2026-10-18)**:
- Gate G0: ctest 165/168 (3 RED esperados: D6+D7×2; `MultiwindowOds.CrossWindow` preexistente, familia D7).
- Gate G1: D6 GREEN 15/15 legibles y exactos; ctest 165/168 (solo familia D7 RED); prueba stash: el fix no altera SUP ni cobjs.
- Gate G2: D7 3/3 GREEN (Base, AnOverlap, AnNormal); `MultiwindowOds.*` GREEN (CrossWindow actualizado a doctrina 028/oráculo: el PCS del clear de fin de epoch va vacío); ctest **169/169**; `final_hashes_phase29.txt`: 39 differ **esperados** (efecto del fix D7 por numeración global `oid = wi + buff[wi]`; artefactos del oráculo `py_*`/`nc_py_*` intactos; batería 1: divergencia 55→11 DS). Los 11 DS restantes quedan fuera de alcance 029 — ver REPORT.md.

---

## Phase 0 — Baseline y RED (no tocar código)

- [x] **T000** Congelar `baseline_hashes_phase29.txt`: MD5/hashes de página fija
  (metodología 027) de TODOS los artefactos de los fixtures de la auditoría —
  `temp/params-audit/` batería 1 (py_*/cpp_* base/-n/-p/-d/-w) y `temp/params-audit/ncbdn/`
  (nc_py_*/nc_cpp_*). Estado ANTES del fix; es el mandante de no-regresión.
- [x] **T001** Test RED D7 (cobjs por display set): fixture de test C++ que, para cada
  DS de `ncbdn/nc_cpp_base.sup` y `ncbdn/nc_cpp_an_ov.sup`, extraiga el set de
  (o_id, window_id) del PCS y lo compare con el set del `.sup` del oráculo par
  (nc_py_base.sup / nc_py_an_ov.sup). Asserts: mismo cardinal y mismos ids/ventanas
  por DS. Debe FALLAR hoy (oráculo 2 cobjs vs port 1 en DS2 base; 2 vs 1 en DS1 overlap).
  Reuso permitido del criterio de parseo de `temp/params-audit/render_pgs.py`.
- [x] **T002** Test RED D6 (ida y vuelta MUI): mini-script Python bajo `tests/` que
  reciba un path de .sup y .pex/.mui, use `scenaristream.TSPair.from_mui` y emita
  PTS/DTS por record en TSV; test C++ que encodea `--withsup`, invoca el script
  (subprocess) y compara cada record con los PTS/DTS del `.sup` gemelo. Asserts:
  100% decodificables, PTS == SUP, DTS == SUP. Debe FALLAR hoy (0/1412 PTS legibles).
- [x] **Gate G0**: suite ctest verde (165 + RED en los 2 tests nuevos). Documentar
  el RED con los conteos exactos del repro.

## Phase 1 — Fix D6 (writer MUI)

- [x] **T003** Cerrar D-029-1: pareo byte a byte de un record MUI del port vs lo que
  `from_mui` consume (posiciones 0..N del payload, campos TIMING y cabeceras por
  record). Documentar en research.md el layout objetivo y el defecto concreto.
- [x] **T004** Fix puntual del writer MUI en `src/opensup/core/filestreams.cpp`:
  campo PTS (y si corresponde campo de cabecera por record) conforme al layout del
  lector del oráculo. Sin tocar DTS, orden de records, ni el PES.
- [x] **T005** Test D6 GREEN (ida y vuelta, 100% exacto, `check_integrity()` ok).
- [x] **Gate G1**: hashes SUP/PES sin cambios vs `baseline_hashes_phase29.txt`;
  hash del MUI cambia (esperado, es el defecto); suite ctest verde.

## Phase 2 — Fix D7 (cobjs de re-adquisición)

- [x] **T006** Cerrar D-029-2: con un fixture mínimo en el oráculo (2 ventanas,
  reutilización 0002.png en la 2ª época), confirmar: (a) ¿el PCS de la re-adquisición
  lista ambos objetos siempre? (b) ¿el ODS del bitmap reutilizado se re-emite y con
  qué id? Documentar la regla exacta con el `.sup` mínimo adjunto.
- [x] **T007** Aplicar la regla al port: `src/opensup/core/render/encode_epoch.cpp`
  (single-pass multi-ventana) y `src/opensup/core/render/overlap.cpp`
  (`emit_epoch_from_nodes`): el PCS de adquisición lista todos los cobjs activos
  per-ventana; el ODS del bitmap reutilizado se re-emite según el oráculo (T006).
- [x] **T008** Test D7 GREEN sobre `ncbdn/`: base y overlap con sets de cobjs ==
  oráculo en TODOS los DS; sanity `--alternate-oids` (ids distintos pero sets
  equivalentes).
- [x] **Gate G2**: no-regresión — hashes de batería 1 (BDN real, sin reutilización
  cruzada) IGUALES al baseline; nuevos hashes congelados en
  `final_hashes_phase29.txt`; suite ctest verde completa.

## Phase 3 — Checklist y cierre

- [x] **T009** Actualizar `temp/params-audit/REPORT.md` con el estado final (D6/D7
  cerrados, D2'/D4 no-goals documentados).
- [x] **T010** Marcar spec.md Status: Implementada (fecha) con el resumen de hashes
  y tests; guardar obs de memoria con los resultados.
- [x] **T011** Checklist de `checklist.md` del repo si aplica (ver convención de
  025-028).
