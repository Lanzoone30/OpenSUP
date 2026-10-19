# Implementation Plan: 031 - Gap-clears del oráculo en modo overlap

**Branch**: `031-overlap-gap-clears`

**Feature spec**: `specs/031-overlap-gap-clears/spec.md`
**Research**: `specs/031-overlap-gap-clears/research.md`

**Style conformance**: metodología 025-030. **El modo base queda byte-idéntico**
(gate post-030): el cambio es confinado a la ruta overlap (`overlap.cpp` builder),
con posible fix secundario de ACQ post-wipe si la evidencia de D-031-3 lo pide.

## Summary

1. El builder overlap (`overlap.cpp:199-332`) debe recibir/pasar los
   **gap-nodes** del oráculo (huecos ≥1 frame) para que `has_clear` se setee y
   el bloque emisor (`overlap.cpp:376-405`) cobre vida con **WDS de layout
   completo** (`_get_undisplay`, render2.py:802-807).
2. CASO H2 (opcional, con firmeza): la ACQ post-clear lista cobjs contra un
   estado de composición vacío — alinear con el oráculo en los 10/20 PTS.

## Technical Context

**Stack**: C++17 (core). Sin dependencias nuevas. Go UI intacto.

**Patch boundary**:
- `src/opensup/core/render/overlap.cpp` — builder: propagación de gap-clears
  (`has_clear`, payload del clear con layout completo).
- `src/opensup/core/render/encode_epoch.cpp` — solo si la evidencia D-031-3
  requiere ajuste del estado post-clear (acotado al bloque de composición ACQ).
- `tests/core/overlap_gap_clear_test.cpp` (nuevo, patrón tests/core +
  ds_parity.py como referencia del harness).
- `temp/params-audit/REPORT.md` + `CHANGELOG.md` — corrección documental.
- **NO tocar**: ruteo (encode_epoch.cpp:457-458), emit_clear_ds del single-pass
  (:516-556, ya paridad), parser BDN (033), writer MUI, Go UI.

## Decisiones de diseño

- **D-031-1** ver spec. **D-031-2** ver spec (builder, no emisor). **D-031-3**:
  H2 con evidencia byte-level ANTES del fix; si el post-clear contradice la
  regla D-030-1 en modo base, STOP: el defecto sería del ruteo/composición y
  abre decisión nueva (no parche).
- **D-031-4**: el DTS noise (~2-30 ticks, preexistente) queda documentado como
  divergencia aceptada y fuera del gate de éxito (comparación por presencia de
  DS + composición, no por DTS exacto en clears comunes).

## Risk assessment

| Riesgo | Impacto | Mitigación |
|---|---|---|
| El clear con 1 sola window_definition diverge del layout completo | medio | gate G2: comparación por-DS contra py_ov (con WDS geometry check) |
| H2 no aplica en modo base (STOP-the-line) | medio | evidencia antes del fix; caida segura a US1-only |
| tocar el builder activa el estado de 2 windows no-gauche | bajo | tests ncbd -d/-n -d como guardia |
| diff DTS preexistente gemela el gate | bajo | D-031-4 define el gate por presencia+composición |

## Gates resumidos
- **G0** — baseline congelado (`baseline_hashes_phase31.txt` sobre batería 1 base
  + -d + ncbd 4 variantes, estado pre-spec); suite 171/171.
- **G1** — modo base y ncbd byte-idénticos (no-regresión post-030/033-*).
- **G2** — batería 1 -d: 304→324 DS, ds_parity 0 divergentes (o solo DTS noise
  documentado); test geométrico ncbd verde; suite verde; CHANGELOG + REPORT +
  commits + memoria.
