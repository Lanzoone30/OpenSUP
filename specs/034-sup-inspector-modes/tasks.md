# Tasks: 034 - Modos inspector del CLI

**Input**: `spec.md` + `plan.md`. Convención: TDD, fases acotadas, gates.

## Phase 0 — Lectura de .SUP + fixture

- [x] **T000** Fixture `.sup` versionado pequeño (criterio 030-T002), desde
  batería o generación propia; listo en `tests/fixtures/`.
- [x] **T001** `src/opensup/read/`: parser de .SUP (segment header, mapeo a
  structs existentes PCS/WDS/PDS/ODS/ENDS/CObject) + tests de parse (fixture).
- [x] **T002** `decode_rle` (US4) con roundtrip contra `encode_rle`
  (`media/pgraphics.h:22`) y modelo del decoder (pgraphics.py:44-132).

## Phase 1 — Modos CLI

- [x] **T003** `cli_parser`: subcomandos `validate|inspect|stats` (solo literal
  en argv[1], D-034-1); quitar `->required()` de encode-io → validación manual +
  `sc->fallthrough()`; `main.cpp` dispatch.
- [x] **T004** **inspect** (US2): dump estructural humano + `--ds N` +
  `--json`. Tests sobre fixture.
- [x] **T005** **validate** (US1): reglas D-034-2 (structural + PTS/DTS sanity +
  test_diplayset + rx_bitrate opcional). Exit-codes precisos. Tests: GREEN en
  py_base y cpp_base; RED en fixture envenenado por test manipulado.
- [x] **T006** **stats** (US3): modo standalone (imprime contadores de lectura) +
  extensión del `done` NDJSON del encode (ES/ACQ/NC/ODS/PU/PU-buffered +
  `reuse_candidates` ya disponible). `json_emitter.cpp` + `protocol.go` espejo.

## Phase 2 — Gates y cierre

- [x] **G0** Suite ctest verde tras Phase 0 (parser nuevo no toca encoder).
  Evidencia: **203/203** (1 skip preexistente `OverlapGapClear.WdsOdsGeometry`,
  no tocado); `sha256sum -c` 032 = 12/12 OK.
- [x] **G1** validate/inspect/stats ejecutan sobre todos los artefactos de la
  batería congelados sin crash; validate GREEN sobre los oráculos.
  Evidencia: 267 `.sup` sin crash (rc ≤ 2); paridad **193/193** con
  `is_compliant`+`check_pts_dts_sanity`; **17/17** streams del oráculo GREEN.
- [x] **G2** Smoke GUI: runner.go lanza encode flags-plana sin cambios
  (regression tras quitar `required()`); suite completa verde.
  Evidencia: `go test ./...` verde (mirror `protocol.go` + test nuevo); el
  parser de encode quedó intacto (D-034-A), así que no hay regresión de
  `required()`. Pendiente por instrucción: CHANGELOG (prohibido), commits y
  status en `spec.md`.
