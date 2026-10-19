# Feature Specification: 032 - Paridad exacta de superficie CLI/GUI (nombrar como SUPer)

**Feature Branch**: `032-cli-gui-surface-parity`

**Created**: 2026-10-18

**Status**: Implementada (2026-10-19, con desviación autorizada D-032-5: -k solo flag+storage, T012 diferido)

**Input**: Consolidado de la auditoría de paridad de opciones (exp-1 SUPer,
exp-2 OpenSUP-go, 2026-10-18). El usuario fijó como meta la paridad "lo más
idéntica posible" con SUPer en el proyecto C++ y su GUI Go. Esta spec cubre SOLO
superficie (nombres, IDs, defaults, labels, opciones faltantes de bajo riesgo).
El stream del encode por defecto no cambia.

## No-goals
- **Backends QtzrUTC/Pillow/pngquant**: solo IDs reservados con fail-fast (spec
  futura 036); no se portea motor de cuantización en esta spec.
- **Informe Succint estructurado** (`-l -10` fiel): diferido (D-032-2). Aceptado
  como divergencia con warning explícito.
- **`--layout` presets (SAFE/NORMAL/GREEDY)**: spec diferida 035 (infraestructura).
- **Parser BDN / reglas del stream**: 030/031/033.
- **Overwrite GUI**: paridad ya lograda (D-032-3); sin cambios.

## User Scenarios & Testing (priorizados)

### US1 (P1) — IDs y defaults del quantizer exactos a SUPer
```
Given el CLI y la GUI
When el usuario usa -q
Then IDs exactos SUPer: 0 QtzrUTC, 1 Pillow, 2 HexTree, 3 PNGQ(LIQ), 4 pngquant
  (help text con esa numeración)
And fuera de rango → default 3 + warning (como supercli.py:115-117)
And ID default del CLI = 3 (LIQ). El backend real expuesto: 2 (HexTree) y 3 (LIQ)
And -q con backend inexistente (0/1/4) → exit con error "backend not available
  in this build (available: 2, 3)"
And GUI: select de quantizer muestra 2 y 3 con textos SUPer ("HexTree (good,
  very fast)", "PILIQ (best, fast)") vía i18n; default UI 3; omite -q si == 3
```

### US2 (P2) — Shorts exactos de SUPer
```
Given el CLI
When se inspecciona la ayuda / se usan shorts
Then -d/--ahead (alias --overlap), -t/--threads (alias -j deprecado con warn),
  --ssim-tol sin corto, -n/--allow-normal, --debug long-only
And todos los flags largos existentes siguen aceptándose igual
```

### US3 (P3) — prefer-normal expuesto y fiel
```
Given el encoder
When -k/--prefer-normal sin --allow-normal
Then allow se fuerza (supercli.py:148-150) y el encoder möjliggör prefer
  (consumo real en encode_epoch.cpp/overlap.cpp, no storage-only)
And sin -k, streams byte-idénticos al estado previo (gate de hashes)
And con -k (ncbd/ENIGMA), paridad de sets y geometría vs oráculo donde aplique
```

### US4 (P4) — -l/--log-to-file
```
Given el CLI
When -l {0,10,20,30}
Then se escribe archivo de log con el nivel pedido (set_file_output ya existe)
And -l sin flag → logger en stdout como hoy
And -l -10 → warn "succinct report not implemented" + log estándar (D-032-2)
```

### US5 (P5) — Labels exactas + opción GUI nueva
```
Given la GUI Go
When se revisan labels EN/ES vs supergui.py
Then labels de opciones con texto SUPer exacto (selectBdn→"Select bdn.xml file",
  setOutput→"Set output", compression→"Compression [int]%: ", acqrate→
  "Acquisition rate [int]%: ", ssimTol→"SSIM tolerance offset [-100;100]: ",
  extraAcq→"Insert acquisition after N palette updates…", fullPalette→
  "Write the full palette.", colorSpace→"Color space: ", etc. — audit completo)
And el label invertido de alternateOids se corrige a "Force legacy global
  object ids (0/1)." con tooltip corregido (EN+ES)
And nuevo checkbox "Prefer normal case object redefinition." (chk_allow style,
  permite habilitación conjunta)
And combo "Log to file: " con opciones SUPer (Disabled/Succint/Standard/
  Minimalist→no/Warnings/Debug) o subset documentado si minimista faltante
And checkbox/flag -k y -l añadidos al puente runner.go
```

## Decisiones vinculantes
- **D-032-1**: hard cutover de IDs de quantizer sin alias (colisión 0/1).
- **D-032-2**: informe Succint fiel diferido; `-l -10` con warn explícito.
- **D-032-3**: overwrite GUI = paridad ya lograda (silent overwrite) — sin cambio.
- **D-032-4**: GUI muestra solo backends disponibles; el resto visible en CLI help
  como "reserved" y abort con error explícito al usarlo.
