# Research: 029 - Paridad MUI PTS y re-adquisición con reutilización de gráficos

Fuente primaria: auditoría de paridad de los 5 parámetros booleanos cerrada el
2026-10-18. Evidencia completa en `temp/params-audit/REPORT.md` (baterías 1 y 2) y
fixture `temp/params-audit/ncbdn/`. Obs #341 de memoria (veredicto consolidado).

## D6 — Writer MUI del port emite PTS ilegibles

### Evidencia (batería 1, BDN 1080p, 168 eventos)
| Señal | Oráculo Python | Port C++ |
|---|---|---|
| Records PTS decodificables por `TSPair.from_mui` | 1360/1360 | **0/1412** |
| Records DTS decodificables | 1360/1360 | 1412/1412 (exactos, == SUP pts) |
| Tipos MUI vs types del SUP | 100% iguales | 100% iguales |
| `check_integrity()` del oráculo | pasa | pasa (solo estructura) |
| Errores a mitad de stream | n/a | offsets no constantes (~−54e6 al inicio, checksums relativos rotos luego) |

Patrones observados al decodificar el MUI del port:
- El DTS de cada record es EXACTO al pts del segmento SUP correspondiente → el
  layout del record y el orden se mantienen; el defecto está en los 4 bytes del
  campo PTS (o en su posición dentro del record).
- El offset corrupto no es una constante del archivo: cambia al avanzar, típico de
  un des-sync stepwise en el parseo de la segunda mitad del record (campo extra u
  omisión de tamaño fijo por record).
- Los tipos MUI y la cantidad de records coinciden 100% con el SUP del port.

### Root-cause (candidatos)
`src/opensup/core/filestreams.cpp:289-350` — el writer MUI escribe DTS correcto y
PTS corrupto. Phase 0 del código: leer el writer lado a lado con
`scenaristream.TSPair.from_mui` (temp/SUPer-main/scenaristream.py) como contract,
y emular byte a byte el layout correcto. Probablemente el writer emite (PTS, DTS)
intercambiados en el segundo sub-campo o empaca los 33 bits de PTS de forma
distinta al lector del oráculo. El des-sync stepwise sugiere además que la
cabecera por record del oráculo lleva un campo bit que el port omite o emite con
otro tamaño.

## D7 — PCS de re-adquisición omite cobjs con reutilización de gráficos

### Evidencia (BDN sintético `temp/params-audit/ncbdn/`)
- Contenido: E1 @10-20s con 2 `<Graphic>` (el oráculo los numera id 0→win0,
  1→win1); E2 @20-30s con 2 `<Graphic>`, uno nuevo y uno que reutiliza 0002.png
  (ya emitido en E1) → el oráculo lo numera id 2 y 3 en la re-adquisición.
- Oráculo (`nc_py_base.sup`): DS2 (idx1, state 0x40) = PCS con **cobjs [2, 3]**
  (win0 y win1) y ODS por objeto.
- Port (`nc_cpp_base.sup`): DS2 = PCS con **cobjs [2] solo** (win0) → el subtítulo
  de win1 desaparece visualmente de 20-30s.
- Port con `--overlap` (`nc_cpp_an_ov.sup`): DS1 ya emite cobjs `[0]` solo → win1
  no aparece en toda la época.
- Evidencia estructural: extracción de cobjs/WDS por display set con
  `temp/params-audit/render_pgs.py`. Basta el conteo: el oráculo nunca emite un
  acquisition con objetos faltantes en la composición.

### Relación con 027/028
- 027 decidió ODS per-ventana y dejó la suite en verde (161 tests al cierre).
- 028 decidió "oráculo decide" para display sets (D-028-1, doctrina 024) y su
  implementación normalizó 1 display set por evento según el oráculo. Pero 028
  usó fixtures del BDN real (temp/ENIGMA) — SIN reutilización de gráfico
  cruzada entre ventanas en una re-adquisición. Por eso este caso coexiste con
  la suite 165/165 verde actual.
- La regla faltante: en re-adquisición, el oráculo lista TODOS los objetos
  activos de cada ventana en el PCS y re-emite el ODS del objeto reutilizado; el
  port construye el PCS solo con objetos bitmap-nuevos (lógica de dedupe del
  cache de ODS aplicada también a la composición, donde el oráculo no la aplica).

### Root-cause (candidatos)
`src/opensup/core/render/encode_epoch.cpp` (emisión por ventana post-028) y
`src/opensup/core/render/overlap.cpp`: el dedupe de objetos ya emitidos debe
aplicarse SOLO a la decisión de emitir ODS (el oráculo re-emite el ODS del
bitmap reutilizado en la nueva época) y nunca al listado de cobjs del PCS de
adquisición. Phase 0 del código: confirmar exactamente qué re-emite el oráculo
(un ODS por objeto, nodo de `render2.py` en la re-adquisición) y dónde el port
filtra.

## Datos para el spec
- Suite ctest actual: 165 tests.
- Repros exactos (fixtures + comandos): REPORT.md §Métodos; scripts de la
  auditoría en `temp/params-audit/`.
