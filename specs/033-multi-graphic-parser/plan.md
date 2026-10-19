# Implementation Plan: 033 - Parser multi-`<Graphic>` (canvas unión por evento)

**Branch**: `033-multi-graphic-parser`

**Feature spec**: `specs/033-multi-graphic-parser/spec.md`
**Research**: `specs/033-multi-graphic-parser/research.md`

**Style conformance**: metodología 025-030 (TDD, fases acotadas, gates con
hashes). La fuente de verdad de geometría es el fixture versionado
`tests/fixtures/ncbdn/`.

## Summary

El render multi-ventana por evento ya existe (doctrina 027,
`encode_epoch.cpp:1057-1140`); el agujero es solo el parser+modelo de evento:
1. Iterar **todos** los `<Graphic>` del evento → box unión y `load_image()`
   componiendo el canvas (paste con offset relativo), como
   `filestreams.py:190-246,306-326,427-467`.
2. `filestreams.h:25-58`: extender el modelo 1-imagen → lista/canvas.
3. `interface.cpp:117-143` `determine_windows` sin cambios (entra canvas igual).
4. Quitar el SKIP del test geométrico D7 (`d7_cobjs_reacq_parity_test.cpp:248`).

## Technical Context

**Stack**: C++17 (core). Sin dependencias nuevas. Go UI intacto.

**Patch boundary**:
- `src/opensup/core/filestreams.{h,cpp}` — parser y modelo del evento (lista de
  Graphics, canvas unión, validación no-solape + declared==real).
- `tests/core/d7_cobjs_reacq_parity_test.cpp` — quitar skip geométrico (:248).
- `specs/033-…/baseline_hashes_phase33.txt` + `final_hashes_phase33.txt` —
  congelar sobre ncbd y batería 1.
- **NO tocar**: `interface.cpp` (determine_windows), `encode_epoch.cpp`,
  `overlap.cpp`, Go UI, supercily writer MUI.

## Decisiones de diseño

- **D-033-1** (verifica exp-7, **FIRMADA**): "1 Event = 1 bitmap unión;
  ventanas/crop del canvas combinado; objetos = ventanas pintadas, no Graphics".
  Evidencia del oráculo:
  - `temp/SUPer-main/SUPer/filestreams.py:190-246` — `BDNXMLEvent`: docstring
    "a BDNXML event can have numerous child elements ... and >1 graphics
    (files) shown at once"; box = `Box.union` de todos los `<Graphic>`
    (224-229); `self._width/_height = box.dx/dy` (238-239).
  - `:306-326` — `load()`: N>1 → `Image.new('RGBA',(self._width,self._height))`
    + `paste(gfxp, (box.x-self.x, box.y-self.y))` con dims reales del PNG;
    `assert Box.intersect(*boxes).area == 0`; `assert img.width==self._width`.
  - `:427-467` — `_parse_events`: agrupa TODOS los `<Graphic>` del `<Event>` en
    `gevents` ("2+ images are merged to have one image").
  - `temp/SUPer-main/SUPer/render2.py:157-201` — `mask_event`: el crop de filas
    vive en el window-split, no en el Event; `:1304-1383` — `WindowAnalyzer`:
    los objetos salen por VENTANA.
  - `temp/SUPer-main/SUPer/interface.py:73-172,225-227,616-620` —
    `determine_windows` consume el canvas combinado vía alpha.
- **D-033-2** gate de calidad = geometría+cobjs+conteo ODS — la paridad
  byte-total del `.sup` NO es meta de esta spec (riesgo de fusión SSIM
  por-evento vs por-ventana); si el gate GM revela divergencia estructural que
  afecta el subtítulo, STOP y abrir spec de análisis aparte.
- **D-033-3** determine_windows no cambia.
- **D-033-4** el gate de "no-regresión ENIGMA" es obligatorio: los 168/168
  eventos single-Graphic de batería 1 deben quedar byte-idénticos (la ruta
  N=1 es un caso particular del canvas unión, no un branch nuevo).

## Risk assessment

| Riesgo | Impacto | Mitigación |
|---|---|---|
| Fusión SSIM por-evento altera paleta/shape de objetos existentes (single-Graphic) | alto | G1 hash batería 1 byte-idéntico + suite |
| determine_windows receptor de canvas distinto (gap interno) | medio | ncbd geometría + caso del gap x=751-766 en test |
| Validación new-rules rompe BDNs locales generosos | bajo | solo fail explícito en solape/declared!=real (igual que oráculo) |

## Gates resumidos
- **G0** — baseline congelado (estado pre-spec); suite 171/171.
- **G1** — ENIGMA batería 1: hashes byte-idénticos (no-regresión).
- **G2** — ncbd: WDS/ODS/cobjs geometry == oráculo (4 variantes), test
  geométrico fuera de skip, hashes nc_cpp_* re-baseline a phase33; suite verde;
  CHANGELOG + memoria + commits.
