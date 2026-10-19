# Tasks: 032 - Paridad exacta de superficie CLI/GUI

**Input**: `spec.md` + `plan.md`. Convención: TDD, fases acotadas, gates.
Stop-the-line: si el consumo de prefer (Phase 2) toca el stream default,
revertir scope según D-032-5.

## Phase 0 — Baseline (no tocar código)

- [x] **T000** Congelar `baseline_hashes_phase32.txt`: sha256 de artefactos base
  batería 1 (py_*/cpp_*) + ncbd (nc_*) — estado pre-spec.
- [x] **T001** Suite ctest base verde documentada (estado pre-spec, 169/169).

## Phase 1 — Shorts exactos + deprecados (US2)

- [x] **T002** `cli_parser.cpp`: remap shorts (-d/--ahead, -t/--threads,
  -n/--allow-normal, --ssim-tol sin corto, --debug long-only; aliases
  --overlap/-j conservados y marcados deprecated).
- [x] **T003** Warning deprecación: uso de `-j`/`--overlap` imprime warn una vez
  ("deprecated alias, use -t/--threads / -d/--ahead").
- [x] **T004** `runner.go:buildArgs`: shorts nuevos ("-t", "-d/--ahead"), y
  pass-through inalterado para tests `runner_test.go` (actualizar expectativas).
- [x] **T005** `cli_parser_test.cpp`: casos nuevos (shorts, alias-warn) + fixes a
  afectados. Suite verde.

## Phase 2 — Quantizer IDs SUPer (US1)

- [x] **T006** `cli_parser.cpp`: rango 0..4 + fuera→3+warn; default 3; help text
 numeración SUPer; fail-fast explícito para 0/1/4 con mensaje
 "backend not available in this build (available: 2, 3)".
- [x] **T007** `optimizer.cpp`: mapping de IDs por backend (2→hextree, 3→LIQ) y
  registro de claves fail-fast coherente con cli_parser (único sitio de verdad).
- [x] **T008** Tests `cli_parser_test.cpp` (defaults, out-range→3+warn,
  fail-fast 0/1/4) + deepen `runner_test.go` (GUI default 3).
- [x] **T009** markup `combo_quantizer`: ids 2/3, textos "HexTree (good, very
  fast)"/"PILIQ (best, fast)" vía i18n, select default 3.

## Phase 3 — prefer-normal (US3)

- [x] **T010** CLI `-k/--prefer-normal` + regla de fuerza allow
  (`max(prefer, allow)` como supercli.py:148-150) + `encode_config_t` inalterado.
- [x] **T011** Consumo fiel en core: `encode_epoch.cpp`/`overlap.cpp` decision
  points de normal-case (:870-874; :301) *honor prefer*. Gate: default hashes
  intactos (comparar contra T000 tras cada paso).
- [x] **T012** Paridad con -k: batería sobre ncbd/ENIGMA con -k, comparación de
  sets+geometría vs oráculo con -k (spawn py). RED→GREEN documentado.
- [x] **T013** Tests de urgencia: "-k sin allow — allow forzado" (supercli rule).

## Phase 4 — log-to-file (US4)

- [x] **T014** CLI `-l/--log-to-file` int {0,10,20,30} + `-l -10` warn Succint
  divergente (D-032-2); escritura de archivo `<outfile>_build.log` (o nombre SUPer
  `<outfile>.log`) vía `set_file_output` (`logger.cpp:81-84`).
- [x] **T015** Test: nivel escogido y archivo se crea con contenido del logger.

## Phase 5 — GUI + i18n (US5)

- [x] **T016** Labels exactas SUPer (EN+ES) según audit (research §1) para las
  claves afectadas + tooltips. Audit completo de las no revisadas.
- [x] **T017** Fix label/tooltip invertidos de alternateOids → "Force legacy
  global object ids (0/1)." (EN+ES) y coherencia con help del CLI.
- [x] **T018** Nuevo checkbox prefer (markup left-control-panel + i18n) y combo
  log (Disabled/Debug/Standard/Warnings/errors — subset del oráculo sin opciones
  inexistentes) + `runner.go` pass-through de ambos.
- [x] **T019** i18n espejo `shared/i18n.js` + strings_test verde.

## Phase 6 — Gates y cierre

- [x] **G0/G1/G2** Según plan.md: hashes default == baseline_phase32
  (byte-identity); suite completa verde; parafarmacia `-k` documentada;
  CHANGELOG (breaking -q + resumen); commits; memoria engram; status "Implementada".

## Firmas y desviaciones (2026-10-19)

- **T000** `baseline_hashes_phase32.txt` = `final_hashes_phase33.txt` (estado
  current-truth, 12 artefactos). `sha256sum -c` OK 12/12 pre y post cambios.
- **T001** La suite base real era **173/173** (1 skip: `OverlapGapClear.WdsOdsGeometry`),
  no 169/169 como estimó el plan (033 añadió tests tras el conteo de la spec).
- **T002–T005** Shorts remapeados: `-d/--ahead`, `-t/--threads`,
  `-n/--allow-normal`, `--ssim-tol` y `--debug` long-only; `--overlap` y `-j`
  conservados como alias con warning de un disparo. `runner.go` emite los
  shorts nuevos. 29 casos `CliParser.*` verdes.
- **T006–T009** IDs SUPer: rango 0..4, fuera→3+warn, default 3, fail-fast 0/1/4
  con `backend not available in this build (available: 2, 3)`. `optimiser_c::index_for`
  es el único sitio de mapeo (2→hextree, 3→LIQ). Combo GUI con ids 2/3 y textos
  SUPer vía i18n (`quantHexTree`/`quantPiliq`), default 3.
- **T010** `-k/--prefer-normal` + regla de fuerza `max(prefer, allow)`
  (supercli.py:148-150) verificado en `PreferNormalForcesAllowNormal`.
- **T011 — D-032-5 STOP aplicado.** La regla fiel del oráculo
  (`render2.py:544`, `j_nc > j or prefer_normal_case`) necesita la cadena de
  nodos de decodificación (buffer `j`/`j_nc`), que este encoder no modela: en el
  modelo actual `allow` solo ya se comporta como prefer-always, así que cablear
  `prefer` exigiría una spec de comportamiento 030-style y tocaría el stream.
  Se revierte el scope a **flag + storage** (`-k` fuerza `allow`); el campo
  queda `[[maybe_unused]]` con comentario de decisión en `epoch_encoder.h` e
  `interface.h`. Divergencia aceptada y documentada, no un fallo.
- **T012 — diferido por T011.** La paridad `-k` vs oráculo depende de la misma
  regla no modelada; no se hace spawn del oráculo.
- **T013** Cubierto por `PreferNormalForcesAllowNormal` + `NewShortFlagsMapToSuPerSurface`.
- **T014–T015** `-l/--log-to-file` int, `{0,10,20,30}`; `-10` (Succint, D-032-2)
  avisa y cae a estándar; `>40` desactiva con warn; escribe
  `<outfile>_build.log` vía `set_file_output`. Verificado por tests unitarios +
  run real (260 B de log).
- **T016–T019** Labels EN alineadas al audit SUPer (con `: ` literal), ES
  corregidas; `alternateOids` des-invertido (label + tooltip) a
  "Force legacy global object ids (0/1)."; checkbox `chk_prefer_normal` y combo
  `combo_log` añadidos y cableados en `btns-actions.js`; espejo `shared/i18n.js`
  = 74 claves, `strings_test.go` verde.
- **G0/G1/G2** G0: baseline congelado. G1: help exacto (numeración SUPer,
  deprecados visibles, fail-fast 0/1/4 verificado end-to-end). G2:
  `sha256sum -c baseline_hashes_phase32.txt` = **12/12 OK** (stream default
  byte-idéntico). Suite **183/183** (1 skip: `OverlapGapClear.WdsOdsGeometry`),
  Go `go test ./...` verde.
- **Extra (031)** Re-evaluación post-033 de `OverlapGapClear.WdsOdsGeometry`:
  sigue divergiendo (30 DS) — 033 no cubre este fixture (ENIGMA-1080p es
  single-graphic). Se conserva el skip con mensaje actualizado a
  "re-evaluado post-033: sin resolver".
- **No ejecutado por instrucción**: `CHANGELOG.md` (prohibido), commits, y
  cambio de status en `spec.md` (sigue "Planificada"). Cierre registrado en
  `closure.md`.
