# Feature Specification: 030 - Paridad del PCS de adquisición (solo ventanas re-adquiridas)

**Feature Branch**: `030-acquisition-cobjs-parity`

**Created**: 2026-10-18

**Status**: Implementada (2026-10-18)

**Input**: Deuda explícita del cierre de spec 029 ("11 DS de batería 1 con
sobre-emisión de ventanas retenidas + root atribuido al parser BDN") revisada
post-cierre: la atribución es incorrecta (ENIGMA-1080p es 100% single-`<Graphic>`,
168/168 eventos). Se abren 2 frentes consolidados: **D-A** (regla de listado de
cobjs del PCS de adquisición) y **D-D** (test de paridad D7 solo numerológico).
Incluye higiene de 029 como Phase 0 (commits pendientes, CHANGELOG, fixtures,
SKIP_RETURN_CODE) porque es prerrequisito de cualquier re-baseline.

## No-goals (fuera de scope)
- **Multi-`<Graphic>` por evento** (defecto aparte, `filestreams.cpp:158-161`):
  enruta a spec 033. Aquí NO se toca el parser BDN.
- **Overlap batería 1 (20 DS NORMAL ausentes)**: enruta a spec 031. Los tests de
  esta spec se limitan al modo base (sin `-d`) para no mezclar señales.
- **Backends de cuantizador / superficie CLI/GUI**: spec 032.
- **D-029-2 (renumeración por ventana)**: sigue siendo vinculante; no se re-litiga.

## User Scenarios & Testing (priorizados)

### US1 (P1) — PCS de adquisición conforme al oráculo: solo ventanas re-adquiridas
Como hipótesis de paridad byte-level, necesito que el PCS de adquisición de cada
DS del port liste exactamente las ventanas que el evento re-adquiere en esa
composición, y NO las ventanas retenidas que el evento no toca.

**Contrato**:
```
Given la batería 1 (temp/ENIGMA-1080p/BDN_Index_1920.xml) en modo base
When el port encodea y el harness compara DS por DS contra el oráculo py_base
Then los 55 DS originales divergentes de la auditoría → 0 divergentes
  (los 11 residuales post-029 desaparecen: DS30, 34, 50, 52, 54, 65…)
And para cada DS del port, set de cobjs == set de cobjs del oráculo
  (ids incluidos, conforme D-029-2: oid = wi + buff[wi])
And ningún cobj extra apunta a una ventana que el evento no pinta
```

### US2 (P2) — Regla ncbd intacta (multi-ventana re-adquirida)
Como caso multi-ventana (fixture `ncbdn/`), necesito que el fix NO retroceda:
los eventos que SÍ re-adquieren todas sus ventanas siguen listando todas
(ganancia de 029 se conserva byte-idéntica).

**Contrato**:
```
Given el fixture ncbd (nc_cpp_* vs nc_py_*)
When el port encodea con las mismas flags de 029
Then nc_cpp_* hashes == final_hashes_phase29.txt (sin cambios)
And D7CobjsReacqParity GREEN (3/3: Base, AnOverlap, AnNormal)
```

### US3 (P3) — Paridad de composición no-numerológica (test geométrico)
Como mantenedor, necesito que el test de re-adquisición valide GEOMETRÍA, no
solo ids, para que una divergencia como la de ncbd (WDS distinta, ODS nunca
cargado) nunca vuelve a pasar DESAPERCIBIDA en un test que da GREEN.

**Contrato**:
```
Given el test D7CobjsReacqParity extendido
When compara el port contra el oráculo por DS
Then además del set (o_id, window_id) exige:
  1. bounding boxes de WDS idénticas (x,y,w,h)
  2. crop/offset y tamaño de cada ODS idénticos
  3. conteo de objetos por DS idéntico
Y el test RED vs el port pre-fix-033 (multi-Graphic) queda documentado como
  divergencia conocida en-route a 033 (puede quedar skip explicito con SKIP_RETURN_CODE)
```

### US4 (P4) — Higiene de 029 y confianza de suite
Como desarrollador que implementará 031/033, necesito un estado commiteado y
reproducible: fix 029 en commits atómicos, CHANGELOG con entrada 029, fixtures
ncbdn versionados en repo, y GTEST_SKIP con SKIP_RETURN_CODE (skip ≠ passed).

**Contrato**:
```
Given clone limpia SIN temp/ ni fixtures locales
When ctest corre toda la suite
Then tests con fixtures ausentes reportan SKIP (SKIP_RETURN_CODE configurado)
And ctest reporta Passed y Skipped por separado (169 passed solo con fixtures)
And CHANGELOG.md tiene entrada [029] con resumen D6/D7
And el working tree queda limpio al cerrar la spec 030
```

## Decisiones previas vinculantes
- **D-030-1** (hipótesis a evidenciar en Phase 1): cobjs del PCS de adquisición =
  ventanas que el evento re-adquiere; ventanas retenidas NO (regla del oráculo).
- **D-030-2**: la verificación es dois-track — batería 1 (estructura + ids) y
  ncbd (geometría extendida). Ambas con hashes congelados baseline/final.
- **D-030-3**: el cambio es LOCAL al listado del PCS de adquisición. Si durante
  implementación se descubre que tocarlo altera renumeración y emit-OODS de
  re-utilización, STOP y reabrir decisión D-030-1 con evidencia del oráculo.
