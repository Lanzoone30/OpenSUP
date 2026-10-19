# Research: 031 — Gap-clears faltantes en el modo overlap

Fuente primaria: lane exp-6 (2026-10-18/19) sobre los artefactos congelados
(`temp/params-audit/py_ov.sup` 324 DS vs `cpp_ov.sup` 304 DS; batería 1,
ENIGMA-1080p). Análisis estruct-realiko con struct-parse (`/tmp/opencode/parse_sup.py`).

## 1. Qué son los 20 DS ausentes (mecanismo del oráculo — cerrado)
- Son **gap-clears**: el oráculo inserta un gap-node por todo hueco ≥1 frame
  entre eventos (`render2.py:1147-1158`) y `_convert` (:862-877) emite
  `_get_undisplay` (:802-807): **PCS NORMAL con 0 cobjs + WDS con el layout
  completo + ENDS** al `tc_out` del evento previo.
- Verificado en el binario: cada DS ausente es `NORMAL nobj=0 (PCS+WDS+ENDS)` a
  exactamente el OutTC de un evento BDN, con la ACQ siguiente a +3600 ticks
  (1 frame @25fps). NO son hold/refresh ni leaky-buffer.
- Lista PTS (verificado por orquestador con ds_parity): 4392000, 5076000,
  16959600, 25246800, 26017200, 26251200, 26431200, 27097200, 27273600,
  27457200, 27802800, 46368000, 110433600, 114652800, 195876000, 196099200,
  197006400, 197326800, 199713600, 209145600.

## 2. Dónde se pierde el port (root-cause)
- `encode_epoch.cpp:457-458`: épocas de 2 ventanas → ruta `encode_epoch_overlap`.
- `overlap.cpp:199-332` (builder): **nunca setea `has_clear`** (defaults
  `epoch_encoder.h:74-77`) ⇒ el bloque emisor de clears (`overlap.cpp:376-405`)
  es código muerto en modo overlap.
- El single-pass (1 ventana) SÍ emite gap-clears: `encode_epoch.cpp:1040-1042`
  + `emit_clear_ds` (:516-556). Por eso el modo base quedó en paridad.
- ⚠️ Detalle crítico: el bloque muerto emitiría el clear con **1 sola
  window_definition**; el oráculo usa el layout completo — alinearlo o el DS
  aparecerá pero divergiendo.

## 3. Defecto secundario (H2, Media)
En 10/20 casos, la ACQ (azing +3600 ticks) del oráculo lista **1 cobj** vs
**2 cobjs del port**: tras el clear/vipe la composición arranca vacía y el
oráculo considera una ventana sin contenido nuevo (regla D-030-1 aplicada al
estado post-wipe). Requiere evidencia propia antes del fix (D-031-3).

## 4. Corrección documental obligada
La batería 2 (o2*) es **órfana**: `temp/ENIGMA/BDN_Index.xml` tiene
VideoFormat `1350x1080` que el oráculo rechaza (`filestreams.py:518` TypeError);
existe solo `o2py_*.log`, ningún `.sup`. Las comparaciones o2cpp-vs-py del
REPORT.md eran cruzadas (BDs distintos) — hay que re-documentarlas.

## 5. Riesgos / notas
- DTS de clears ya divergen ~2-30 ticks py-vs-port en clears comunes (ruido
  preexistente, no causa del defecto) — documentar, no tocar.
- Gates post-030 igual obligan: modo base byte-idéntico (sin tocar ruteo),
  ncbd (4 variantes) estable/par.
- No se re-baseline o2* en esta spec (sin oráculo posible).
