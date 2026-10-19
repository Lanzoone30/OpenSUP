# Feature Specification: 031 - Gap-clears del oráculo en modo overlap

**Feature Branch**: `031-overlap-gap-clears`

**Created**: 2026-10-19

**Status**: Implementada (2026-10-19)

**Input**: Divergencia overlap batería 1 detectada en el cierre de 029 y
re-verificada post-030: el port omite **20 DS de gap-clear** (NORMAL, 0 cobjs,
PCS+WDS+ENDS) que el oráculo emite a los OutTC de huecos ≥1 frame entre
eventos. Root-cause cerrado por exp-6: el builder overlap nunca setea
`has_clear` ⇒ bloque emisor muerto (`overlap.cpp:376-405`).

## No-goals
- **Re-baseline de batería 2 (o2*)**: imposible (sin oráculo, VideoFormat
  1350x1080 no parseable por supercli, `filestreams.py:518`) — solo corrección
  documental.
- **DTS noise preexistente** (~2-30 ticks en clears comunes): documentar, no tocar.
- **Modo base**: byte-idéntico por contrato post-030 (no tocar el ruteo
  `encode_epoch.cpp:457-458`).
- **Parser multi-Graphic / superficie CLI**: 032/033.

## User Scenarios & Testing (priorizados)

### US1 (P1) — Gap-clears emitidos en modo overlap, byte-parity
```
Given batería 1 (ENIGMA-1080p) con -d/--overlap
When el port encodea (cpp_ov) y se compara vs py_ov por ds_parity
Then los 20 PTS gap-clear existen como DS completas (PCS NORMAL 0 cobjs +
  WDS con el LAYOUT COMPLETO de ventanas + ENDS) — el port pasa de 304 a 324 DS
And 0 divergentes (el Diff por-DS contra el oráculo es vacío
  — salvo el DTS noise documentado)
And el modo base (cpp_base vs py_base) queda byte-idéntico (gate post-030)
```

### US2 (P2) — Composición de la ACQ post-wipe (defecto secundario H2)
```
Given los 10/20 PTS donde el oráculo lista 1 cobj en la ACQ siguiente al clear
  y el port lista 2
When se aplica la regla D-030-1 (solo ventanas re-adquiridas) sobre el estado
  post-clear (composición vacía)
Then el port lista los mismis cobjs que el oráculo en esos 10 PCS de adquisición
And se firma D-031-3 con evidencia byte-level ANTES del fix (stop-the-line si
  contradice D-030-1 en modo base)
```

### US3 (P3) — Corrección documental de la batería 2
```
Given REPORT.md comparaba o2cpp_* (BDN 1350x1080) contra py_* (ENIGMA-1080p)
When se actualiza el documento
Then nota explícita: batería 2 órfana (TypeError filestreams.py:518), solo
  logs; ninguna conclusión de paridad se extrae de esa comparación cruzada
```

## Decisiones vinculantes
- **D-031-1**: el gap-clear en modo overlap emite **WDS con el layout completo**
  (fiel a `_get_undisplay`, no 1 sola window como el bloque muerto actual).
- **D-031-2**: el fix vive en el builder (`overlap.cpp:199-332`: propagar
  gap-nodes/`has_clear`), no en el bloque emisor (código reusado tal cual).
- **D-031-3**: H2 (ACQ post-wipe) se cierra con evidencia propio en Phase 1;
  STOP-the-line si contradice D-030-1 en cualquier modo.
