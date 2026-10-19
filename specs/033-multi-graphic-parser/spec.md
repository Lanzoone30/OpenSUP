# Feature Specification: 033 - Parser BDN multi-`<Graphic>` (canvas unión por evento)

**Feature Branch**: `033-multi-graphic-parser`

**Created**: 2026-10-18

**Status**: Implementada (2026-10-19)

**Input**: Defecto documentado desde siempre en el port
(`filestreams.cpp:158-161`, "multi-Graphic events deferred") y confirmado
byte-level en el fixture `ncbd/ncbdn` (2 Graphics por evento, subtítulo derecho
visualmente ausente). El oráculo unifica N Graphics en un canvas por evento.

## No-goals
- **Fusión SSIM/paleta multi-ventana refinada**: si el gate geométrico revela
  divergencia residual de análisis se abre spec de comportamiento aparte; aquí
  el contrato es geometría+cobjs, no byte-total.
- **Fixture N>2 nuevo** salvo US2 opcional.
- **Layout presets / backends**: 035/036 (diferidas).
- **Regla de cobjs/retención (D-030-1)**: intacta, solo revalidada.

## User Scenarios & Testing (priorizados)

### US1 (P1) — ncbd geometría byte-parity (sale el skip del test geométrico)
```
Given el fixture tests/fixtures/ncbdn (evento con 0001.png izq y 0002.png der)
When el port encodea como el oráculo (misma flags, 4 variantes: base/-n/-d/-n -d)
Then WDS del port == (200,953,718x53) y (1000,913,833x106)
And ODS por ventana: 718x53 (izq) y 833x106 (der) con crop del split del canvas
And el conjunto de cobjs por DS == oráculo (con ids por ventana D-029-2)
And el test geométrico D7 sale de SKIP y queda verde duro (sin fixture externo)
And el subtítulo derecho existe byte-level (ODS 833x106 presente)
```

### US2 (P2, opcional) — invariante de validación del canvas unión
```
Given un evento con Graphics que se SOLAPAN o un box declared != real
When el port parsea
Then falla con error explícito (como el oráculo filestreams.py:306-326)
```

### US3 (P3) — revalidación de la suite completa
```
Given la suite 171/171 post-030
When 033 aterriza
Then suite verde sin skips nuevos (solo los de fixtures por diseño)
And dnpy_y/nc_py_* y ENIGMA hashes intactos
```

## Decisiones vinculantes
- **D-033-1**: 1 Event = 1 bitmap-canvas unión; ventanas y crop del canvas
  combinado (no por Graphic); objetos = ventanas pintadas (no por Graphic) —
  fiel a filestreams.py/interface.py/render2.py del oráculo.
- **D-033-2**: gate de calidad = geometría+cobjs+conteo ODS (no byte-total del
  .sup), por el riesgo de fusión SSIM residencia (research §5).
- **D-033-3**: determine_windows NO cambia (input canvas unión ya lo satisface).
