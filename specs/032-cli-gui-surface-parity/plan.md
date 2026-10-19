# Implementation Plan: 032 - Paridad exacta de superficie CLI/GUI

**Branch**: `032-cli-gui-surface-parity`

**Feature spec**: `specs/032-cli-gui-surface-parity/spec.md`
**Research**: `specs/032-cli-gui-surface-parity/research.md`

**Style conformance**: metodología 025-030 (TDD, fases acotadas, gates). Esta spec
es de superficie: **el encode por defecto debe quedar byte-idéntico** — el gate
principal es hashes estables, no re-baseline.

## Summary

Cinco frentes independientes, sin dependencia entre sí (solo orden sugerido):

1. **CLI churn de flags** (US2): shorts exactos SUPer, aliases deprecados.
2. **Quantizer IDs** (US1): renumber + default 3 + fail-fast 0/1/4 + help text.
3. **prefer-normal** (US3): flag `-k` + consumo fiel en el core.
4. **log-to-file** (US4): `-l` con set_level + set_file_output.
5. **GUI/i18n** (US5): labels exactas, fix invertida alternateOids, nuevas
   checkboxes/combo, puente runner.go.

## Technical Context

**Stack**: C++17 (cli_parser, interface, encode_epoch/overlap, optimizer),
Go+Wails (runner.go, markup, i18n). Sin dependencias nuevas.

**Archivos cambiados (código)**:
- `src/opensup/cli/cli_parser.{h,cpp}` — flags (shorts, -k, -l, -q), validation,
  defaults, help texts.
- `src/opensup/cli/main.cpp` — wiring de -l (nivel+file output; :59-64 zona).
- `src/opensup/core/render/encode_epoch.cpp` + `overlap.cpp` — consumo de
  prefer_normal_case en los decision-points normal-case (:870-874; :301).
- `src/opensup/media/optimizer.cpp` — mapping de IDs/fail-fast de 0/1/4 (si el
  fail-fast vive mejor aquí que en cli_parser).
- `ui/engine/runner.go` — buildArgs (shorts nuevos, quantizer omit>=3 default,
  prefer/log pass-through).
- `ui/frontend/src/components/left-control-panel/*` + `btns-actions.js` —
  labels, nuevo checkbox prefer, combo log, fix alternateOids, combo quantizer.
- `ui/i18n/strings.go` + `ui/frontend/src/shared/i18n.js` — claves nuevas/corregidas.
- `CHANGELOG.md` — breaking note -q + summary.

**Tests**: `tests/core/cli_parser_test.cpp` (15 casos a actualizar + nuevos),
`ui/engine/runner_test.go` (buildArgs), `ui/i18n/strings_test.go` (completitud),
paridad por batería para -k (tests/core/ con fixture ncbd + batería 1 si aplica).

**Archivo sidebar**: baseline de hashes `baseline_hashes_phase32.txt` =
artefactos base de batería 1 + ncbd **sin -k** (estado pre-spec); deben permanecer
idénticos al final (gate). No se toca parser BDN, PES/MUI writer, ni ruta overlap
de calidad (031).

## Decisiones de diseño

- **D-032-1** hard cutover de IDs quantizer (sin alias posible por colisión):
  documentado en CHANGELOG como breaking change de superficie.
- **D-032-2** Succint fiel diferido a spec propia (canal de decisiones del core
  inexistente); `-l -10` avisa y cae a log estándar.
- **D-032-3** overwrite GUI sin cambios (paridad de comportamiento ya lograda).
- **D-032-4** backends no disponibles: sólo visibles en help como "reserved";
  abort fail-fast explícito al usarlos.
- **D-032-5** el consumo de `prefer_normal_case` replica exactamente la regla
  del oráculo (`render2.py` filter_events): decidir normal-case **preferente**
  cuando técnicamente permita, manteniendo la invariante "no usar normal-case
  si allow=0". Si al cablear sor plan de riesgo se detecta que el cambio toca
  el stream default, STOP — revertir el scope a flag+storage y abrir spec de
  comportamiento aparte (030-style) con evidencia del oráculo.

## Risk assessment

| Riesgo | Impacto | Mitigación |
|---|---|---|
| runtime break por -q viejo (0/1) | medio | fail-fast con mensaje de migración explícito; CHANGELOG |
| prefer cambia stream default | alto | gate hashes estables; STOP-the-line D-032-5 |
| shorts rompenscriptas que documentan --ssim-tol -t | medio | doble alias en una fase de transición documentada |
| labels cambian sin sincronizar i18n.js espejo | bajo | TestTableComplete + build UI |

## Gates resumidos
- **G0** — baseline congelado; suite ctest verde pre-changes.
- **G1** — CLI: help text exacto, aliases warn, fail-fast de -q 0/1/4; tests nuevos.
- **G2** — default byte-identical: hashes batería 1 + ncbd == baseline;
  paridad -k documentada; suite 100% verde; CHANGELOG + memoria.
