# Research: 034 — Modos inspector del CLI (validate/inspect/stats)

Fuente primaria: lane exp-5 (2026-10-18) + probe empírico de CLI11 v2.6.2
(`/tmp/opencode/spec034-research.md` — resumen consolidado aquí). Precedente de
valor: funciones del oráculo nunca expuestas (SUPFile, is_compliant,
check_pts_dts_sanity, test_diplayset, test_rx_bitrate, debug_stats, decode_rle).

## 1. Capital C++ actual (síntesis para la spec)

- El gate "post-encode" de `flags_gate_test.cpp` usa `bdn_render_c::execute()`
  (`interface.cpp:339-353`): hoy solo `test_rx_bitrate` (con `-m`) y el hook de
  tests (`interface.h:68-70`); su único efecto es gatear PES+MUI
  (`interface.cpp:379-387`). **Cero reglas estructurales** del oráculo porteadas.
- Reglas de paleta: `pds_c::validate_palette` existe (`segments.h:229`) — 4 reglas
  ya portadas (is_compliance familiares).
- **Falta `decode_rle`**: solo existe `encode_rle` (`media/pgraphics.h:22`) —
  prerrequisito para validar ODS y conteo de objetos.
- Estructuras del stream disponibles: familia PG (PCS/WDS/PDS/ODS/ENDS,
  `segments.{h,cpp}`) ya modelo del decoder (usadas al emitir y en tests de
  estructura).

## 2. Compliance — qué portear (oráculo `pgstream.py`)

- `is_compliant` (:184): 6 familias de reglas — las de paleta ya están; faltan
  reglas estructurales del flujo (PTS/DTS sanity :403, test del DS :147, bitrate
  LeakyBuffer :115, conteo/suporte de composición, de-storage).
- `check_pts_dts_sanity` (:403) y `test_diplayset` (:147) → núcleo del modo
  `validate`.
- `test_rx_bitrate` ya portado (usado con -m); exponerlo standalone en validate.

## 3. Stats — gap NDJSON vs `debug_stats` (pgstream.py:380-401)

`done` event actual = `success/events/epochs/segments/duration_ms`
(`json_emitter.cpp:75-86`, espejo `protocol.go:57-67`). Faltan contadores
ES/ACQ/NC/ODS/PU/PU-buffered. Dato útil: `reuse_candidates` ya se computa y
nunca se emite — gratis.

## 4. Superficie CLI — verificado con probe CLI11 v2.6.2

- `add_subcommand` NO rompe la invocación flags-plana de la GUI
  (`--json -i in.xml out.sup`) — verificado empíricamente.
- PERO los `->required()` actuales (`cli_parser.cpp:26-30`) **matan** cualquier
  subcomando (regla `App_inl.hpp:1999-2002` + `:1378`): hay que quitar `required`,
  validar a mano, y usar `sc->fallthrough()` (probe OK).
- Cambio mínimo propuesto: subcomandos detectables solo si `argv[1]` es
  literalmente `validate|inspect|stats`; excepcionalmente, pre-scan de `argv[1]`.
  **Edge case confirmado**: output file llamado literalmente "validate" — el
  pre-scan lo resuelve.
- Sites: `cli_parser.cpp/h`, `main.cpp:50-57`, `core/CMakeLists.txt:1-11`.

## 5. Tests

Patrones a copiar: `cli_parser_test.cpp:61-72` (EXPECT_EXIT),
`flags_gate_test.cpp:34-50` + `ds_structure_test.cpp:19-53` (fixtures SUP).
Avisos de exp-5: `json_emitter.cpp` no está en el target de tests hoy; y no hay
fixture `.sup` en repo — el modo stats necesita uno pequeño versionado (o
generarlo desde la batería vía helper, mismo criterio de fixtures que 030-T002).
