# Research: 033 — Parser BDN multi-`<Graphic>` (canvas unión por evento)

Fuente primaria: lane exp-7 (2026-10-18). Corrección importante al contexto del
exp-3: los WDS del **oráculo** son `w0=(200,953,718x53)` y `w1=(1000,913,833x106)`;
el **port** emite `(200,953,551x43)+(767,953,151x53)` porque solo vio 0001.png y
su gap interno x=751-766 lo partió en dos pseudo-ventanas.

## 1. Algoritmo del oráculo (lo que hay que portear)
- N `<Graphic>` dentro de un `<Event>` = **UN evento con UN canvas-union**:
  box = unión de los boxes; cada graphic se pega con offset relativo
  (`filestreams.py:190-246,306-326,427-467`), con validación de
  no-solape y declared==real.
- El layout por época se decide sobre el **alpha del canvas unión**
  (`interface.py:73-172,225-227,616-620`); los modificadores N/G/S del selector
  de layouts actúan sobre el canvas combinado.
- Las 13 filas de crop del ejemplo son propiedad del **window-split**
  (bbox de contenido opaco por cluster, `render2.py:157-169,179-201`), no del
  Event — no hay crop por Graphic.
- Los **objetos son uno por VENTANA** (WindowAnalyzer + SSIM/CTU,
  `render2.py:1304-1383`), no por Graphic.

## 2. Fixtures disponibles
No existe fixture con N>2 Graphics en ningún lado. `tests/fixtures/ncbdn/`
(nd: N=2 por evento, 4 variantes de flags: base / -n / -n -d / -d) es la verdad
byte-level; `temp/params-audit/o_layout*` viene de un BDN fuera del repo (no
reproducible). Conclusión: **ncbd basta** como gate; no se necesita fixture nuevo
para US1 (opcional US2 si QA lo pide).

## 3. Sites del port
- Defecto único: `filestreams.cpp:152-184` — `child("Graphic")` toma solo el
  primero (comentario "deferred" :161). `filestreams.h:25-58` modela
  1-imagen-por-evento (extender a lista + box unión).
- `interface.cpp:117-143` `determine_windows` NO necesita cambios si el evento
  trae el canvas unión (recibe una image+box como hoy).
- Render multi-ventana por evento **ya existe y es la doctrina 027**
  (`encode_epoch.cpp:1057-1140`: ODS/oid por ventana).
- Test geométrico D7 en `d7_cobjs_reacq_parity_test.cpp:172-252` con SKIP
  "spec 033" (:248) — sale del skip con esta spec.

## 4. Impacto en la paridad lograda
- Intactos: D-028-1, D-029-2, D-030-1; batería 1 ENIGMA (168/168 single-Graphic);
  specs 020-026.
- A revisar con re-baseline: D7 cobjs 3/3 (ncbd), test geométrico (skip→verde
  duro), hashes `nc_cpp_*` de phase29/30 (los `nc_py_*`, `o_layout*` y batería 1
  ENIGMA no cambian — el oráculo no cambia).

## 5. Riesgo principal
Divergencia residual posible en **fusión por-evento vs por-ventana** del análisis
(SSIM/paleta compartida vs por-ventana): el gate 033 debe ser
**geometría + cobjs + conteo de ODS** (como D7 hoy), NO igualdad byte-total del
`.sup`. La igualdad byte-total solo dentro de los segmentos del subtítulo derecho
(ODS 833x106).

## 6. Encuadre spec 033
- US1: ncbd geometría byte-parity; D7 cobjs re-validado; skip fuera.
- D-033-1 (propuesta a firmar): "1 Event = 1 bitmap unión; ventanas/crop del
  canvas combinado; objetos = ventanas pintadas, no Graphics".
- Gates: re-baseline `nc_cpp_*` (phase33) + batería 1 sin cambios + suite verde.
- El "subtítulo ausente" se demuestra con el gate geométrico (833x106 presente);
  pixel-proof opcional decodificando ese ODS (no requiere pipeline del oráculo).
- Tamaño S; dificultad Media (por el riesgo del §5, no por el parser).
