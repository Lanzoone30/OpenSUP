# Research: 030 — PCS de adquisición: solo ventanas re-adquiridas por el evento

Fuente primaria: auditoría post-cierre de spec 029 (2026-10-18, lane exp-3) sobre
el estado ya implementado de D6/D7. Evidencia en `temp/params-audit/REPORT.md:28`
y repro computado sobre batería 1 (`temp/ENIGMA-1080p/BDN_Index_1920.xml`).

## D-A — Sobre-emisión de ventanas retenidas en el PCS de adquisición (batería 1)

### Evidencia
- Repro post-029: `cpp_base vs py_base` = **11 DS divergentes** (DS30, 34, 50, 52,
  54, 65…). Patrón uniforme: port lista `[(2,0),(3,1)]` (dos ventanas) donde el
  oráculo lista `[(2,0)]` (una).
- ENIGMA-1080p es 100% single-`<Graphic>`: **168/168 eventos tienen exactamente 1
  `<Graphic>`**. ⇒ La atribución del cierre de 029 ("root probable: parser BDN
  solo lee el primer `<Graphic>`") es **incorrecta para esta batería**; el defecto
  de multi-`<Graphic>` es otro defecto, aparte (spec 033 candidata).
- La regla de renumeración cerrada en D-029-2 sigue siendo cierta: los ids se
  asignan por ventana (`oid = wi + buff[wi]`). Lo que sobra NO es la regla de
  numeración: es el **universo de ventanas** que el PCS de adquisición lista.

### Regla del oráculo (hipótesis a cerrar en Phase 1 — T004)
El PCS de adquisición de un evento lista **solo las ventanas que el evento
re-adquiere** (las mapeadas a sus propios gráficos en esta composición del
epoch). Una ventana que el evento NO toca queda **retenida** (continue del
contenido anterior) y NO recibe cobj en este PCS — no hay que re-declararla aquí.

Versión 029 aplicada durante el fix D7: "todas las ventanas activas de la época".
Esa sobre-generalización corrigió el caso multi-ventana de `ncbdn` (donde todos
los cobjs SÍ re-adquieren) pero rompió el caso single-Graphic con ventana
retenida (ENIGMA). Regla correcta propuesta:

> cobjs del PCS de adquisición de una ventana-w en DS-e = 1 iff el evento en
> DS-e está asociado/compositado a la ventana-w; si el evento no pinta w, w no
> aparece en el PCS de E (su contenido persiste por retención del decoder).

### Sitio del defecto (candidatos)
- `src/opensup/core/render/encode_epoch.cpp` — emisión del PCS de adquisición
  (single-pass multi-ventana, post-028): subconjunto de ventanas listado.
- `src/opensup/core/render/overlap.cpp` (`emit_epoch_from_nodes`): la ruta
  `--overlap` particionada por ventana (post-029), mismo listado.

## D-D — Paridad de composición no-numericológica en el test D7
`tests/core/d7_cobjs_reacq_parity_test.cpp:40-99,104-119` solo compara sets
`(o_id, window_id)` por DS — pasó GREEN durante 029 **mientras** `ncbdn` divergía
en geometría completa (0002.png nunca cargada: port emitía w1 = rebanada del
gráfico izquierdo; `tests/media/epoch_flags_test.cpp` no compara geom tampoco).
Familia necesaria: paridad per-DS de **WDS bounding boxes**, **tamaños/crop/offset
de ODS** de cada DS contra el `.sup` del oráculo par. Criterio técnico: el
harness `tests/ds_parity.py` ya compara composition_state por PTS; ampliar al
punto de exigir, además del set de ids, la geometría (WDS bounding boxes +
crop/offset de ODS). **Este test es prerrequisito para confiar en el
re-baseline de esta spec** (si el fix 030 cambia algo sensible de geometría,
debe fallar bien).

## Higiene (bloquea confianza en el baseline)
- **Fix 029 sin commitear** en `dev` (6 modified + 3 untracked; `git status`
  verificado 2026-10-18): filestreams.cpp / encode_epoch.cpp / overlap.cpp / 3
  tests. CHANGELOG sin entrada de 029 (el commit de 028 sí la tuvo).
- **Fixtures de ncbd**, `ncbdn/` gitignored (`.gitignore:57`) → D7/D6 testea con
  `GTEST_SKIP` sin `SKIP_RETURN_CODE` en CMake → `ctest` cuenta skips como
  "Passed" ⇒ suite 169/169 frágil en clone limpia.
- REPORT.md §"Próximos pasos" (líneas 54-58) stale (índices 1-2 ya resueltos).

## Datos para el spec
- Suite ctest en el estado actual: 169/169 (verificado a 2026-10-18).
- Baseline congelado vigente: `specs/029-mui-pts-reacq-parity/final_hashes_phase29.txt`
  (39 differ respecto `baseline_hashes_phase29.txt`, efecto D7 esperado; oráculos
  `py_*`/`nc_py_*` intactos).
- Batería 1 divergencia post-029: 55→11 DS (modo base). Overlap: pendiente spec 031
  (20 DS NORMAL ausentes — NO en scope aquí).
- Riesgo de geometría: el oráculo recorta 13 filas en ncbd (53 vs 66 px) — cota
  alta al tocar el crop: NO re-implementar recorte, solo reutilizar la estructura
  del port; si el fix llegara a tocar geometría, el test de D-D lo detecta.
