# Tasks: 030 - Paridad del PCS de adquisición (solo ventanas re-adquiridas)

**Input**: `spec.md` + `plan.md` (mismo directorio). Convención: TDD (RED antes
que código), fases acotadas, gates con hashes como en 025-029.

**Reglas de ejecución**:
- Cada fase termina con la suite ctest verde (o skip explícito documentado) y el
  gate documentado en esta lista.
- Ningún archivo fuera del patch boundary del plan (encode_epoch.cpp cobjs-list,
  overlap.cpp cobjs-list, tests de D7, higiene).
- Al primer falso-negativo de la regla D-030-1: STOP y reabrir la decisión
  (plan.md, D-030-3) — no se parchea con excepciones.

---

## Phase 0 — Higiene de 029 (sin tocar el encoder)

- [ ] **T000** Commits atómicos del fix 029 pendiente en `dev`: (a) fix D6
  (filestreams.cpp MUI writer), (b) fix D7 (encode_epoch.cpp + overlap.cpp),
  (c) tests D6/D7 + CrossWindow actualizado, (d) specs/029 completo. Mensajes
  estilo repo; working tree limpio al terminar.
- [ ] **T001** CHANGELOG.md: entrada `[029]` con resumen D6/D7 + decisión
  D-029-2 (numeración por ventana) y deuda 11 DS → referenciada a spec 030.
- [ ] **T002** Versionar fixtures ncbd: mover/copiar a `tests/fixtures/ncbdn/`
  (o, si el tamaño excede el criterio del repo, dejar en temp/ + runner
  documentado `tests/scripts/fetch_fixtures.sh` con SKIP explicito).
  Actualizar rutas hardcoded en tests D6/D7 y SinglePassDsParity.
- [ ] **T003** CMake: `SKIP_RETURN_CODE` (ctest properties) para que los
  GTEST_SKIP cuenten como Skipped y no Passed; verificar en clone limpia sin
  temp/ (`ctest` reporta Skipped=X). Gate G0-d: higiene verificada.

## Phase 1 — Evidencia de la regla del oráculo (no tocar código)

- [x] **T004** Cierre de D-030-1 con evidencia byte-level del oráculo sobre
  batería 1: para los 11 DS divergentes (DS30, 34, 50, 52, 54, 65…), listar del
  `py_base.sup` qué ventanas el evento pinta (via `<Graphic>` de cada evento y
  composition del DS) y evidenciar que la ventana NO listada está retenida.
  Escribir la regla final en plan.md (actualizando/firmando D-030-1) y en
  esta tasks como nota de cierre.
  → Cerrada 2026-10-18: 11 DS (1-based 30,34,50,52,54,65,169,187,282,284,302),
  solo sobre-emisión (0 under-listing, port 231 vs oráculo 220 cobjs). Firma en
  plan.md §D-030-1.

## Phase 2 — RED (tests antes del fix)

- [x] **T005** Test RED D-A (batería 1, modo base): test C++ que encodea con
  exec de batería (como params-audit) o lee los artefactos congelados de
  `temp/params-audit/` y compara por DS el set de cobjs del port (`cpp_base.sup`)
  contra `py_base.sup`: exactamente los 11 DS divergentes → RED; el resto GREEN.
  Si depende de artefactos en temp/ (gitignored), mismo criterio de fixtures
  que T002 (fetch-runner + skip explícito).
- [x] **T006** Test geométrico (D-D): extender `D7CobjsReacqParity` a exigir
  bounding boxes del WDS + crop/offset/tamaño de cada ODS por DS, contra el
  oráculo par (`nc_py_*`):
  - Caso ncbd: RED documentado como divergencia conocida multi-Graphic
    (skip explicito, owner spec 033) — NO false-silence.
  - Meta general: cuando 033 aterrice, ese skip debe desaparecer.
- [x] **T007** Baseline de hashes `baseline_hashes_phase30.txt`: metodología 029
  (`sha256sum` de los artefactos base de batería 1 y ncbd — estado ANTES del fix).

## Phase 3 — Fix (regla D-030-1)

- [x] **T008** `encode_epoch.cpp`: restringir el listado de cobjs del PCS de
  adquisición a las ventanas que el evento composita (sus `<Graphic>` mapean a
  la ventana); ventanas activas no pintadas → retención, sin cobj en este PCS.
- [x] **T009** `overlap.cpp` (`emit_epoch_from_nodes`): el mismo criterio en la
  ruta `--overlap` particionada por ventana (paridad de criterio con 029).
- [x] **T010**   GREEN T005: los 11 DS divergentes → 0; batería 1 modo base limpia
  (sin tocar overlap de calidad — spec 031).

## Phase 4 — Gates y cierre

- [x] **T011** Gate G1: ctest suite verde (pase esperado 169/169 con fixtures,
  Skipped if fixture-fetcher); D7CobjsReacqParity 3/3 GREEN (US2); nc_* hashes
  sin cambios respecto `final_hashes_phase29.txt` (US2 byte-idéntico).
- [x] **T012** Gate G2 (re-baseline): congelar `final_hashes_phase30.txt` sobre
  todos los artefactos base; diff vs baseline_phase30 = solo los 11 DS
  afectados (y sus renumeraciones colaterales); diff vs
  `baseline_hashes_phase29.txt` = 39 (029) + cambios exclusivos del fix;
  oráculos py_*/nc_py_* intactos.
- [x] **T013** CHANGELOG: entrada `[030]`; REPORT.md §"Próximos pasos" actualizado
  (dede 029 → cerrada; overlap 20 DS → spec 031; multi-Graphic → spec 033).
- [x] **T014** Memoria (engram): obs con cierre de 030, hashes, decisiones.
  Commits de la spec 030 (+spec/030 completo). Status del spec.md a "Implementada".

---

**Gates resumidos**:
- **G0** — Higiene: working tree limpio, CHANGELOG 029, fixtures versionadas, ctest de conteo skip honesto.
- **G1** — ncbd invariante: D7 3/3 GREEN + hashes nc_* estables vs phase29.
- **G2** — batería 1 modo base: 11 → 0 DS divergentes; re-baseline congelado.

## Nota de cierre (2026-10-18)
- T005: → RED confirmado: 11 DS (DS30,34,50,52,54,65,169,187,282,284,302), port 2 vs oráculo 1, extra = ventana retenida.
- T006: → TEST(DFGsReacqParity, Geometry) implementado; skip explicito documentado (owner spec 033).
- T007: → baseline_hashes_phase30.txt creado y verificado con sha256sum -c (batería + ncbd).
- T008: → regla D-030-1 aplicada: bloque reemit_retained eliminado (sin código muerto).
- T009: → mismo criterio en overlap.cpp (em_prev_all), paridad con la ruta --overlap.
- T010: → GREEN: Bateria1CobjsParity 0 divergentes byte-level; suite 171/171 (1 skip por diseño); D7 3/3.
- T011: → G1 ok: D7 3/3 + nc_py_* hashes idénticos a phase29.
- T012: → G2 ok: solo cpp_base/cpp_an/cpp_an_ov cambian (1f4cc030→2b1ed728 ×2, c5754228→997cc0bb); oráculos intactos; final_hashes_phase30.txt congelado.
- T013: → CHANGELOG [030] + REPORT.md §Próximos pasos actualizados.
- T014: → commits spec 030 + memoria engram + status Implementada.
