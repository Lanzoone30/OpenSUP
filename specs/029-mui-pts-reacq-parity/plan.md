# Implementation Plan: 029 - Paridad MUI PTS y re-adquisición con reutilización de gráficos

**Branch**: `029-mui-pts-reacq-parity`

**Feature spec**: `specs/029-mui-pts-reacq-parity/spec.md`
**Research**: `specs/029-mui-pts-reacq-parity/research.md`

**Style conformance**: single codebase (C++ core + Go UI intacto); repositorio en
GitHub; suite ctest como verificación. Equivalente a las features 025-028 (TDD,
fases acotadas, gates con hashes).

## Summary

Dos defectos independientes, verificados con fixtures y repros de la auditoría
2026-10-18 (`temp/params-audit/`):

1. **D6 — Writer MUI**: el 100% de los PTS en el MUI del port son ilegibles por el
   lector del oráculo (`TSPair.from_mui`), con DTS exactos y tipos/conteo 100%
   correctos. Defecto confinado al writer `src/opensup/core/filestreams.cpp:289-350`.
2. **D7 — Re-adquisición con reutilización de gráficos**: el PCS de adquisición del
   port lista solo los objetos bitmap-nuevos y omite el objeto reutilizado de la otra
   ventana; el oráculo siempre lista todos los cobjs activos de cada ventana y re-emite
   el ODS del bitmap reutilizado. Defecto en `encode_epoch.cpp` y su variante overlap
   en `overlap.cpp` al aplicar el dedupe de ODS a la composición.

## Technical Context

**Stack / lenguaje**: C++17 (core encoder), Go+Wails (UI, intacto), ctest + fixtures
en `tests/media/`. Sin dependencias nuevas.

**Archivos cambiados (código)**:
- `src/opensup/core/filestreams.cpp` — solo el writer MUI (:289-350 aprox).
- `src/opensup/core/render/encode_epoch.cpp` — exclusivamente el listado de cobjs
  (composición) en la emisión multi-ventana single-pass, según el contrato del
  oráculo.
- `src/opensup/core/render/overlap.cpp` — el mismo listado de cobjs en el display
  set de `emit_epoch_from_nodes` (es la ruta donde `--overlap` pierde win1).
- `tests/` — nuevos tests RED para D6 y D7 (fixtures y repros de la auditoría).

**Archivos NO cambiados**: `ds_timings.cpp`, `segments.cpp`,
`sequence_quality_policy.cpp`, CLI, UI Go/TS, y las decisiones de emisión de
027/028 (per-window ODS/WDS/PDS se conserva).

## Design

**Doctrina**: "oráculo decide" (024): los .sup/.pes/.mui del oráculo mandan. Sin
esquemas nuevos; solo paridad del writer contra el lector del oráculo.

**Decisión D-029-1 (cerrada en Phase 1 con código)**: layout del record MUI
verificado byte a byte contra `TSPair.from_mui` (scenaristream 0.0.5). Record =
`type(1) + block_len(4, BE) + ts(9)` (14 B), tail `0xFF + 13×00`. El `ts` de 9 bytes
se compone así:
- bytes 0..3 = DTS[32:1], BE 32-bit (`(dts>>1)`); byte 4 bit 7 = DTS[0].
- byte 4 bits[6:0] = PTS_rel[38:32]; bytes 5..8 = PTS_rel[31:0], BE 32-bit.
- con `PTS_rel = ((raw_pts + 54e6) << 6) & ((1<<39)-1)`.

`from_mui` reconstruye 39 bits (`(b4&0x7F)<<32 | BE32(b5..8)`), hace `>>6` y resta
54e6; el writer debe emitir el PTS ya desplazado `<<6` para que `(PTS_rel>>6)-54e6 ==
raw_pts`. Root-cause confirmado: el port escribía `raw_pts+54e6` sin el `<<6` →
`(b4&0x7F)<<32 | BE32` reconstruía un PTS 64× menor → ilegible. DTS ya era exacto
(`>>1` en 4 B + LSB en bit 7). No se cambia el orden ni el tamaño del record: tipos,
conteo, cabecera y DTS ya eran 100% correctos; el defecto está confinado al campo PTS.
La verdad del fix es la ida y vuelta (SUP → MUI → from_mui).

**Decisión D-029-2 (CERRADA en Phase 2, evidencia byte-level del oráculo)**: regla
confirmada de la re-adquisición con reutilización (fixture `ncbdn/`,
`BDN_Index.xml`: ev0 = 0001.png w0 + 0002.png w1; ev1 = 0003.png w0 + 0002.png
reutilizado w1):
- El PCS de adquisición lista el cobj de **cada ventana que el evento re-adquiere**
  (las dos en ncbdn), incluidas las de bitmap reutilizado.
- El oráculo **re-emite el ODS del bitmap reutilizado con un id NUEVO**
  (renumbering por ventana): `nc_py_base.sup` pasa el cobj de w1 de `o1` (DS0) a
  `o3` (DS1), con ODS(o3) y PDS nuevos.
- Cadencia de ids (per-ventana double buffer): slot inicial = nº ventanas; cada
  adquisición avanza el slot de **todas** las ventanas re-adquiridas →
  `oid = wi + buff[wi]` (w0: 0,2,…; w1: 1,3,…). Evidencia: oráculo DS0
  `(o0,w0)(o1,w1)`, DS1 `(o2,w0)(o3,w1)`.
El port replica esa regla con doble buffer por ventana (027); el detalle de
implementación vive en `encode_epoch.cpp` (single-pass) y `overlap.cpp`
(`--overlap`).

## Phases

**Phase 0 — Baseline y RED (solo tests, código intacto)**
- T000 Congelar baseline de hashes de los fixtures de la auditoría (BDN 1080p
  batería 1 y `ncbdn/`) en `baseline_hashes_phase29.txt` (metodología 027, estado
  ANTES del fix).
- T001 Test RED D7: para cada display set de `ncbdn/nc_cpp_base.sup` y
  `nc_cpp_an_ov.sup` (y su par del oráculo `nc_py_*.sup`), asserts de que el set de
  cobjs por DS es igual al del oráculo. Debe FALLAR hoy (2 cobjs en DS2 del oráculo
  vs 1 del port; con overlap, 2 vs 1 en DS1).
- T002 Test RED D6: encode con --withsup y loop de ida y vuelta: decodificar el MUI
  con el lector del oráculo (subprocess de un mini-script Python en `tests/` que
  usa `scenaristream.TSPair.from_mui` y emite PTS/DTS por record como TSV) y
  comparar contra el `.sup` gemelo. Debe FALLAR hoy (0 PTS decodificables).
- Gate G0: suite ctest verde + los 2 tests nuevos en RED.

**Phase 1 — Fix D6 (writer MUI)**
- Comparación byte a byte del layout de record del writer vs el lector del oráculo;
  fix puntual en filestreams.cpp (PTS y/o campo de cabecera por record).
- Test D6 GREEN: PTS/DTS 100% decodificables y exactos; `check_integrity()` ok.
- Gate: los hashes del SUP y del PES deben permanecer IGUALES al baseline (solo el
  MUI cambia); suite ctest verde.

**Phase 2 — Fix D7 (cobjs de re-adquisición)**
- Cerrar D-029-2 con el fixture mínimo del oráculo; aplicar la regla confirmada a
  encode_epoch.cpp (single-pass) y overlap.cpp (`--overlap`): el PCS de adquisición
  lista todos los cobjs activos per-ventana y el ODS del bitmap reutilizado se
  re-emite según lo que haga el oráculo.
- Test D7 GREEN sobre `ncbdn/` (base, allow-normal y overlap) — `D7CobjsReacqParity`
  3/3 OK.
- Gate: nuevos hashes `ncbdn` congelados en `final_hashes_phase29.txt`; suite
  ctest verde completa.
  **Hallazgo de gate (Phase 2)**: la corrección del renumbering por ventana
  (avanzar el slot de **todas** las ventanas re-adquiridas, no solo la del evento,
  ver D-029-2) también afecta a batería 1: los eventos que cruzan la frontera de
  ventana pasaban de `(o0,w0)(o3,w1)` a `(o0,w0)(o1,w1)` (el slot de la 2ª
  ventana no se inicializaba a su primer valor). Por eso batería 1 **cambia** de
  hash y el gate "batería 1 IGUALES al baseline" no puede sostenerse tal cual: los
  89 artefactos se re-congelan en `final_hashes_phase29.txt` (31 de batería 1 +
  8 de ncbdn cambian; los `py_*` del oráculo NO cambian). Es una corrección de la
  numeración per-ventana de 028, no una regresión funcional.

**Phase 3 — Checklist y cierre**
- REPORT.md actualizado con el veredicto por fix (D6/D7 cerrados; D2'/D4
  documentados como no-goals).
- Status de spec.md a "Implementada"; obs de memoria con los hashes finales.

## Riesgos
- D-029-2 puede revelar que el oráculo re-emite el ODS reutilizado con id nuevo
  (renumbering parcial de ventana); el port tiene que replicar ese renumbering y no
  solo el listado de cobjs — se decide ANTES de tocar el port.
- El fix D6 no debe alterar los DTS (ya exactos) ni el orden de records; el menor
  riesgo es tocar por descuido el PES (el fix se limita al MUI).
