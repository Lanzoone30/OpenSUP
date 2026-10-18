# Implementation Plan: 030 - Paridad del PCS de adquisición (solo ventanas re-adquiridas)

**Branch**: `030-acquisition-cobjs-parity`

**Feature spec**: `specs/030-acquisition-cobjs-parity/spec.md`
**Research**: `specs/030-acquisition-cobjs-parity/research.md`

**Style conformance**: single codebase (C++ core + Go UI intacto); suite ctest como
verificación. Metodología idéntica a 025-029: TDD, fases acotadas, gates con
hashes congelados (baseline/final), decisiones D-NNN en plan.md.

## Summary

Tres frentes consolidados, en orden de dependencia:

0. **Higiene de 029** (prerrequisito): commits atómicos del fix 029 (working tree
   sucio: 6 modified + 3 untracked), CHANGELOG sin entrada 029, fixtures ncbd
   gitignored (skip silencioso ctest), ausencia de SKIP_RETURN_CODE.
1. **D-D — Test geométrico** (antes del fix): extender `D7CobjsReacqParity` a
   geometría (WDS bbox + crop/offset ODS) para que la paridad deje de ser
   numerológica. Red sobre el port actual solo para los casos donde la divergencia
   sea real (la geometría ncbd diverge — eso va a 033; aquí el test se documenta
   con skip explicito accionado por 033).
2. **D-A — Regla de listado**: PCS de adquisición = solo ventanas re-adquiridas
   por el evento (regla del oráculo, D-030-1). Efecto esperado: batería 1 modo
   base 11 divergentes → 0. ncbd intacto (US2).

## Technical Context

**Stack / lenguaje**: C++17 (core encoder), Go+Wails (UI, intacto), ctest +
fixtures. Sin dependencias nuevas.

**Archivos cambiados (código)**:
- `src/opensup/core/render/encode_epoch.cpp` — exclusivamente el listado de cobjs
  del PCS de adquisición (subset de ventanas por evento, no todas las de la época).
- `src/opensup/core/render/overlap.cpp` — el mismo listado en la ruta
  `--overlap` particionada por ventana ( ceremonial parity con 029).

**Archivos tocados (tests/infra)**:
- `tests/core/d7_cobjs_reacq_parity_test.cpp` — extensión geométrica (D-D).
- `tests/media/epoch_flags_test.cpp` — solo si el enumerador de cobjs lo exige.
- `CMakeLists.txt` (tests) — SKIP_RETURN_CODE para GTEST_SKIP.
- `.gitignore` / `tests/fixtures/` — versionar fixtures ncbd (si tamaño ok) o, si
  no caben en repo, un runner documentado en `tests/scripts/` con SKIP explicito.
- `CHANGELOG.md` — entrada [029] (higiene) y [030] (final).
- `temp/params-audit/REPORT.md` — actualizar §"Próximos pasos" stale.

**Fuera de patch boundary**: parser BDN (`filestreams.cpp:158-161`), ruta overlap
de calidad (031), superficie CLI/GUI (032), Go UI.

## Decisiones de diseño

### D-030-1 — Regla de listado del PCS de adquisición (deriva de D-029-2)
El PCS de adquisición de un DS-e lista un cobj por cada ventana que el evento
composita en DS-e (sus `<Graphic>` mapean a esa ventana). Ventana activa de la
época pero no pintada por el evento → NO aparece en el PCS de E (retención).
Evidencia: patrón byte-level batería 1 `[(2,0),(3,1)] vs [(2,0)]` exp-3.
Se cierra con evidencia del oráculo en Phase 1 (T004) y no por intuición.

**Cierre D-030-1 (T004, firmado 2026-10-18)** — evidencia byte-level sobre
batería 1 (`cpp_base.sup` vs `py_base.sup`, parser idéntico al de D7):
- Divergentes (1-based): DS30, 34, 50, 52, 54, 65, 169, 187, 282, 284, 302.
  Patrón uniforme: port lista el cobj extra de la ventana retenida donde el
  oráculo no lista nada más (p.ej. port `[(2,0),(3,1)]` vs oráculo `[(2,0)]`;
  la ventana extra alterna: DS30=(3,1), DS34=(2,0), DS52=(1,1), DS284=(0,0)).
- Solo sobre-emisión: 11 DS con cobj extra, **0 under-listing** (el oráculo
  nunca lista algo que el port emite de menos → no hay falso-negativo de
  retención, D-030-3 no se activa). Totales: port 231 vs oráculo 220 (+11).
- La ventana extra está activa en la época (WDS la declara) pero el evento no
  la composita (sus ODS en ese DS son solo los de la ventana re-adquirida): es
  retención, no re-adquisición. P.ej. DS30: port emite 2 ODS en el DS, oráculo 1.
  Regla cerrada: PCS de adquisición = cobjs de las ventanas que el evento
  composita en ese DS.

### D-030-2 — Orden de fases: higiene → test → fix
La higiene (P4) precede el fix porque el baseline de hashes y la suite ctest
honestos son el mandante de no-regresión (los 39 hashes differ de 029 ya
congelados como `final_hashes_phase29.txt`).

### D-030-3 — Stop-the-line
Si al aplicar la regla D-030-1 el port pierde el listado de un cobj que el
oráculo SÍ emite en algún DS (falso-negativo de retención), NO se parchea con
excepciones: se reabre la decisión con evidencia del oráculo (el=ismo paso que
corrigió la sobre-generalización de 029).

## Risk assessment

| Riesgo | Impacto | Mitigo |
|---|---|---|
| La regla D-030-1 altera renumeración en casos multi-ventana | medio | D7 3/3 + hashes nc_* estables gate; back out |
| Tocar `overlap.cpp` activa la divergencia oculta de 031 (20 DS) | medio | tests limitados a modo base; no tocar overlap de calidad |
| Test geométrico RED por defecto de multi-Graphic (033) | baja | skip explicito documentado, no falsa-silencio |
| Fixtures ncbd/pesados no caben en repo | baja | runner documentado + skip explicito |
| 20 DS de 031 contaminan re-baseline | baja | solo artefactos modo base en el gate |

## Resumen de tareas (ver tasks.md — fases detalladas)

- **Phase 0** — Higiene: commits 029 + CHANGELOG + fixtures + SKIP_RETURN_CODE + stale REPORT.
- **Phase 1** — Evidencia: T004 cierra la regla del oráculo con cita byte-level.
- **Phase 2** — RED: test batería-1 cobjs por DS (T005) + test geométrico extendido (T006-T007).
- **Phase 3** — Fix: regla en encode_epoch.cpp + overlap.cpp.
- **Phase 4** — GATES: batería 1 → 11→0; ncbd hashes estables; suite verde; re-baseline final_hashes_phase30.txt; CHANGELOG + UPDATE REPORT + memoria.
