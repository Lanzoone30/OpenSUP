# Implementation Plan: 034 - Modos inspector del CLI

**Branch**: `034-sup-inspector-modes`

**Feature spec**: `specs/034-sup-inspector-modes/spec.md`
**Research**: `specs/034-sup-inspector-modes/research.md`

## Summary

Cuatro frentes en orden de dependencia:
1. **Parser de lectura de .SUP** (nuevo; primero).
2. **`decode_rle`** (US4 — prerrequisito de validate).
3. **`validate`** (US1) y **`inspect`** (US2) sobre el parser.
4. **`stats`** (US3) — nuevo modo + extensión del `done` NDJSON del encode.

## Technical Context

**Stack**: C++17. Sin dependencias nuevas. Desktop CLI11 v2.6.2 (ya en extern).

**Patch boundary**:
- Nuevo: `src/opensup/read/` (reader .SUP: segmentos→structs existentes),
  `src/opensup/media/rle_decode.{h,cpp}` (port pgraphics.py:44-132),
  `src/opensup/cli/inspector.{h,cpp}` (modos). O1 dir `src/opensup/inspector/`.
- Tocado: `cli_parser.{h,cpp}` (subcomandos + quitar `->required()` de encode-io
  → validación manual exacta como probe; pre-scan argv[1] D-034-1),
  `main.cpp:50-57` (dispatch), `cli/json_emitter.cpp` (contadores en `done`),
  `core/CMakeLists.txt` + `tests/CMakeLists.txt` (targets).
- **NO tocar**: encoder, writer, overlap, Go UI (salvo buildArgs si `--json`
  primer arg sigue literal — no cambian).

**Formatos salida**: reporte humano por defecto; `--json` → NDJSON coherente con
`json_emitter` actual (`emit_log/progress/done` + objeto de reporte). Precedente
TSV: `tests/scripts/mui_pts_tsv.py` como referencial para tablas comparables.

## Decisiones de diseño

- **D-034-1** ver probe exp-5: quitar `->required()` (App_inl.hpp:1999-2002/:1378),
  validar input/output a mano, `sc->fallthrough()`; pre-scan `argv[1]` literal
  para edge case output llamado "validate".
- **D-034-2** reglas de compliance incremental (no big-bang): base = structural
  + PTS/DTS sanity; test_rx_bitrate con LeakyBuffer port (1MiB).
- **D-034-4**: fixture `.sup` versionado pequeño (candidato: salida más pequeña
  de la batería o generada por tests y congelada) — reutilizando el criterio de
  fixtures de 030-T002.

## Risk assessment

| Riesgo | Impacto | Mitigación |
|---|---|---|
| `->required()` cambio rompe GUI (invocación flags-plana) | alto | probe OK pero test de regression runner_test + smoke manual |
| validador marca falsos positivos en .sup del oráculo | medio | fixture oráculo (py_base) GREEN obligatorio antes de definir exit-code |
| edge case "validate" como output | bajo | pre-scan argv[1] (D-034-1) + test |
| decode_rle diverge del modelo del decoder | bajo | roundtrip test vs encode_rle existente |

## Gates resumidos
- **G0** — suite verde pre-changes + fixture .sup versionado disponible.
- **G1** — validate GREEN sobre artefactos del oráculo (py_*) y sobre cpp_base;
  violación detectada y señalada en fixture envenenado (test negativo).
- **G2** — stats/stat counters emiten en `done` NDJSON; quitar `required()` no
  rompe la invocación flags-plana de runner.go (smoke test); suite verde;
  CHANGELOG + memoria.
