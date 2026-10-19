# Feature Specification: 034 - Modos inspector del CLI (validate / inspect / stats)

**Feature Branch**: `034-sup-inspector-modes`

**Created**: 2026-10-18

**Status**: Implementada (2026-10-19, vía research §4/D-034-1: pre-scan argv[1] + app inspector separada; T003 de tasks acotado por D-034-A)

**Input**: Valor QA propio inspirado en funciones invocables del oráculo SUPer
cidas sin superficie (`SUPFile`, `is_compliant`, `check_pts_dts_sanity`,
`test_diplayset`, `test_rx_bitrate`, `debug_stats`, `decode_rle`). Objetivo:
dar al CLI la capacidad de leer/inspeccionar/validar un `.sup` existente —
también verificado contra el producto que produce el propio encoder.

## No-goals
- **Paridad algorítmica del encoder**: cero cambios de emisión de stream (los
  gates de esta spec son de lectura, no de escritura).
- **SUP→PES conversion / re-time**: no en scope.
- **GUI**: superficie GUI de inspector se considera en spec aparte si el uso lo
  demanda (desing exp-5 §6); aquí solo CLI.
- **`check_pts_dts_sanity` completo sobre MUI/PES**: solo familia de reglas del
  `.sup` — MUI reader es de 029 (ya existe en tests/scripts, no en el binario).

## User Scenarios & Testing (priorizados)

### US1 (P1) — `validate <file.sup>`: compliance estructural
```
Given un .sup existente (producción propia u oráculo)
When se ejecuta opensup_cli validate file.sup
Then se corren las reglas porteadas del oráculo:
  - is_compliant-structural (familia port: conteo DS/PCS/ODS coherente por epoch,
    paleta < 256, ODS referenciados presentes)
  - check_pts_dts_sanity (DTS monotónico, pts-dts ≥ 0,PTS no en futuro lejano)
  - test_diplayset por DS (todas las invariates estructurales del extractor)
  - test_rx_bitrate (opcional con --max-kbps N, LeakyBuffer 1MiB port)
And exit 0 si conforme, exit 1 con lista de violaciones (human-readable)
And --json emite objeto de reporte NDJSON coherente con el emisor actual
```

### US2 (P2) — `inspect <file.sup>`: dump estructural legible
```
Given un .sup
When opensup_cli inspect file.sup [--ds N]
Then resume: epochs, DS, segmentos por DS (tipos/orden), ventanas (WDS),
  paletas (PDS entradas usadas), objetos (ODS id/size/RLE len), PTS/DTS por DS
And --ds N limita el detalle a un DS (--json migrable)
```

### US3 (P3) — `stats`: contadores del oráculo (incl. en encode)
```
Given encode normal
When --json done se emite
Then añade contadores ES/ACQ/NC/ODS/PU/PU-buffered y reuse_candidates
  (ya computado y nunca emitido — pgstream.py:380-401 gap)
Given un .sup existente
When opensup_cli stats file.sup
Then imprime los mismos contadores de lectura
```

### US4 (P4) — `decode_rle` port (prerrequisito técnico)
```
Given ODS serializado
Then decode_rle fiel al modelo del decoder (pgraphics.py:44-132) disponible
  para validate (conteo de pixels/color no-empty) y para debug futuro
```

## Decisiones vinculantes
- **D-034-1**: subcomando solo si `argv[1]` es literal `validate|inspect|stats`
  (pre-scan); el modo encode conserva la invocación flags-plana de la GUI
  (probe CLI11 OK; `->required()` de encode-io se reemplaza por validación manual
  exactamente como indica el probe, para no matar subcomandos).
- **D-034-2**: reglas de compliance por fases — US1 empieza con reglas
  estructurales portadas + PTS/DTS sanity; la familia completa
  `is_compliant` se consolida incrementalmente sin bloquear.
- **D-034-3**: ningún gate de hash de stream: los artefactos existentes no cambian.
