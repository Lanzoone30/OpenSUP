# Feature Specification: 029 - Paridad MUI PTS y re-adquisición con reutilización de gráficos

**Feature Branch**: `029-mui-pts-reacq-parity`

**Created**: 2026-10-18

**Status**: Implementada (2026-10-18, suite 169/169, G0-G2 documentados en plan.md/tasks.md)

**Input**: Cierre de la auditoría de paridad de los 5 parámetros booleanos (REPORT.md
en `temp/params-audit/`; obs #341 de memoria): 4 de 5 parámetros en paridad
(allow-normal, alternate-oids, palette, with-sup). Quedan 2 divergencias nuevas
confirmadas que no cubren las specs 026-028: **D6** (PTS corruptos en el MUI del port)
y **D7** (cobjs omitidos en PCS de re-adquisición cuando hay reutilización de gráficos
entre ventanas).

## No-goals (descartados por ya decididos)
- **D2'** (PDS por ventana en DS de 2 objetos, +52 PDS): es la emisión per-ventana
  decidida e implementada en 027/028 (doctrina "oráculo decide", D-028-1). No se
  re-litiga; REPORT.md lo documenta como diferencial consistente, no como bug.
- **D4** (flag forced 0x40 del port vs 0x00 del oráculo): decisión doctrinal menor;
  se documenta como divergencia aceptada (US3), sin cambio de código salvo que la
  implementación demuestre un riesgo concreto.

## User Scenarios & Testing (priorizados)

### US1 (P1) — MUI legible por el lector del oráculo: PTS exactos
Como herramienta post-proceso (la GUI del SUPer lee el MUI del port vía
`scenaristream.TSPair.from_mui`), necesito que los PES/MUI producidos por el port
tengan PTS/DTS decodificables y exactos respecto al `.sup` gemelo.

**Contrato**:
```
Given un encode con --withsup (SUP + PES + MUI) en cualquier contenido
When el oráculo decodifica el MUI con TSPair.from_mui()
Then cada record tiene PTS == PTS del segmento correspondiente del .sup
And cada record tiene DTS == DTS del segmento correspondiente del .sup
And TSPair.from_mui() no lanza excepción y no cae en checksum-relativo roto
```

**Estado actual (RED)**: con el port actual el 100% de las fechas PTS son ilegibles
(0/1412 decodificables en la batería 1; offset no constante ~−54e6 al inicio y
checksums relativos rotos a mitad de archivo). Los DTS decodifican exactos; el
defecto es del writer MUI del port (`src/opensup/core/filestreams.cpp:289-350`).

**Escenario de fallo actual**: decodificar records del MUI del port produce fechas
PTS imposibles (negativas o con salto); la verificación provee la prueba de ida y
vuelta (SUP → MUI → lector del oráculo) incumplida.

### US2 (P1) — Re-adquisición no pierde objetos cuando hay reutilización de gráficos
Como usuario, al ver un stream con 2 ventanas donde una época reutiliza un bitmap ya
emitido y la otra introduce uno nuevo, necesito que el subtítulo de la ventana
reutilizada siga presente: el PCS de adquisición debe referenciar ambos objetos, no
solo el de la ventana con bitmap nuevo.

**Estado actual (RED)**: con el BDN sintético `temp/params-audit/ncbdn/` (E1: 2
ventanas, ids 0/1 @10-20s; E2 reutiliza 0003.png y añade 0002.png @20-30s):
- Port base: el PCS de la 2ª DS solo referencia id2 (win0); win1 desaparece de la
  composición 20-30s.
- Port `--overlap`: peor — DS1 ya emite solo id0; el sub de win1 no aparece en toda
  la época.
- Oráculo: el PCS de adquisición re-emite ambos cobjs (ids 2 y 3, un ODS por objeto,
  como siempre hace, conforme a la emisión per-ventana de 027).

**Contrato**: para cualquier época, el PCS de adquisición del port incluye los mismos
cobjs que el oráculo emite en esa misma época (misma emisión per-window ODS de 027);
ningún display set con contenido emitido deja objetos fuera del PCS.

### US3 (P3) — Documentar flag forced como divergencia doctrinal (sin código)
El bit forced no se cambia en esta spec: el port setea 0x40 en los cobjs del
epoch-start; el oráculo nunca. Riesgo bajo (solo afecta players que repliquen
forced-only). Se deja documentado aquí y en REPORT.md.

## Requisitos
- R1 (US1): el writer MUI del port produce PES/MUI con PTS decodificable por
  `TSPair.from_mui` del oráculo, PTS == SUP, DTS == SUP, manteniendo
  `check_integrity()` del archivo en verde.
- R2 (US2): la emisión de adquisición del port replica al oráculo en cobjs, en modo
  default y con `--overlap` (y sanity con `--alternate-oids`), sobre el fixture
  `ncbdn/` de la auditoría.
- R3 (US3): documentación en esta spec (no código).

## Criterios de éxito
- Gate RED→GREEN con los repros exactos de REPORT.md: D7 (cobjs por DS == oráculo en
  todos los DS del fixture ncbdn, base y overlap) y D6 (PTS del MUI 100%
  decodificables y exactos vía `TSPair.from_mui`).
- Suite ctest verde y ampliada con los tests RED de US1/US2.
- Baseline de hashes congelado en `baseline_hashes_phase29.txt` antes del primer fix
  (metodología 027).
