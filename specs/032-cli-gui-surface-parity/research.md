# Research: 032 — Superficie CLI/GUI con paridad exacta a SUPer

Fuentes: inventarios exp-1 (SUPer original) y exp-2 (OpenSUP-go) + verificación
directa del orquestador (labels `ui/i18n/strings.go`, logger, sites quantizer y
`allow_normal_case`). Objetivo del proyecto: paridad "lo más idéntica posible" en
nombre de superficie; el stream por defecto NO cambia.

## 1. Audit de labels i18n (EN) vs supergui.py:316-390

| Clave ours | Texto EN ours | Texto supergui | Paridad |
|---|---|---|---|
| selectBdn | "Select BDN XML" | "Select bdn.xml file" | ✗ |
| setOutput | "Set SUP Output" | "Set output" | ✗ |
| colorSpace | "Color Space" | "Color space: " | ✗ |
| quantizer | "Quantizer" | "Quantizer: " | ~ (sin ":") |
| threads | "Threads" | "Threads: " | ~ |
| compression | "Compression" | "Compression [int]%: " | ✗ |
| acqrate | "Acq. Rate" | "Acquisition rate [int]%: " | ✗ |
| ssimTol | "SSIM Tol." | "SSIM tolerance offset [-100;100]: " | ✗ |
| extraAcq | "Extra Acq" | "Insert acquisition after N palette updates [0-100]: " | ✗ |
| allowNormal | "Allow normal case object redefinition." | idéntico | ✓ |
| fullPalette | "Write full palette." | "Write the full palette." | ✗ |
| bothFormats | "Generate both SUP and PES+MUI files." | idéntico | ✓ |
| overlapBuf | "Allow palette update buffering." | idéntico | ✓ |
| alternateOids | "Alternate per-window object ids…" | (n/a) | **INVERTIDA**: el flag fuerza OIDs globales legacy (help `cli_parser.cpp:54-55`); ES copy = inglés |

La implementación hace el audit completo letra-a-letra del resto (params
restantes, tooltips, y el combo quantizer hardcoded EN en
`left-control-panel.html`); esta sección da el método y ejemplos revisados.

Opciones GUI que SUPer tiene y ours no: checkbox "Prefer normal case object
redefinition." y combo "Log to file: ".

## 2. Quantizer → IDs SUPer exactos

Sites actuales: `cli_parser.cpp:201-205` (validación 0..1), `interface.h:40`,
`interface.cpp:267`, `epoch_encoder.h:119`, fallback en `encode_epoch.cpp:174,179`,
backends en `optimizer.cpp:132-138` (vector fijo LIQ + HexTree).

Mapa SUPer (`optim.py:44-49`, `supercli.py:81-83`): **0 QtzrUTC, 1 Pillow,
2 HexTree, 3 PNGQ/LIQ, 4 pngquant-forzado**. Fuera de rango → 3 + warning
(`supercli.py:115-117`). CLI default **3**. En ours: 2=HexTree coincide; 3=PNGQ/LIQ
expone nuestro LIQ con el ID/etiqueta SUPer. 0/1/4 inexistentes → **fail fast:
"backend not available in this build (available: 2, 3)"**.

**D-032-1**: hard cutover sin alias — los IDs viejos (0=LIQ, 1=HexTree) colisionan
con el nuevo significado (una nota en CHANGELOG; `-q 0` viejo → nuevo
comportamiento Qtzr-not-available, el usuario migra a `-q 3`).

## 3. Shorts CLI exactos de SUPer (`supercli.py:80-99`)

| SUPer exacto | Hoy ours | Cambio |
|---|---|---|
| `-d/--ahead` | `-d/--debug` y `--overlap` | `-d/--ahead` nuevo; `--overlap` queda alias; `--debug` long-only |
| `-t/--threads` | `-j/--threads` | `-t/--threads` exacto; `-j` alias deprecado (warn) |
| `--ssim-tol` | `-t/--ssim-tol` | pierde el corto |
| `-n/--allow-normal` | `--allow-normal` | añade `-n` |
| `-k/--prefer-normal` | no existe | nuevo |

Sites: `cli_parser.cpp` (defs de flags ~:20-60), `ui/engine/runner.go:285-337`
(buildArgs usa literales "-j"/"--overlap"/"--ssim-tol"), tests
`runner_test.go:43-129`, `cli_parser_test.cpp:38-207`.

## 4. prefer_normal_case — semántica del oráculo y sites

- Oráculo: `filter_events` con ambos flags (`render2.py:464,514,544`); CLI `-k`
  **fuerza allow si prefer sin -n** (`supercli.py:148-150`).
- Ours: `interface.cpp:271` lo pasa pero el encoder **nunca lo consume**
  (`epoch_encoder.h:189-193` `[[maybe_unused]]`; `encode_epoch.cpp:143` solo copia).
  La selección normal-case rige por `allow_normal_case`
  (`encode_epoch.cpp:870-874`, `overlap.cpp:301`).
- Scope: exponer `-k` + cablear consumo fiel mínimo (preferir redefinición
  normal-case cuando la secuencia lo permita). Gate: default (sin -k) byte-idéntico;
  con -k, paridad contra oráculo en ncbd/ENIGMA (batería).

## 5. -l/--log-to-file

- Oráculo: niveles {0 off, 10 debug, 20 info, 30 warn} y **-10 Succint** =
  informe `<outfile>.txt` con las decisiones de filtrado
  (`interface.py:192-196`; MT → `_<iid>.txt` :582-584).
- Ours: `common/logger.cpp` ya tiene `set_level` (:24) y `set_file_output`
  (:81-84); `main.cpp:59-64` fija nivel/mode. `-l` = set_level + file output;
  el informe Succint fiel requiere canal estructurado de decisiones que el core
  no emite → **D-032-2**: Succint completo se difiere a un spec propio; en 032,
  `-l -10` se documenta como divergencia aceptada (msg warn "succinct report not
  implemented; writing standard log"). El resto de niveles: paridad exacta.

## 6. Overwrite y GUI parity lograda

`supergui.py` no chequea overwrite (escribe directamente) y nuestra GUI hardcodea
`Overwrite: true` (`btns-actions.js:36`, `app.go:136`) = mismo efecto (overwrite
silencioso). El CLI de ambos aborta sin `-y`. ⇒ **Sin cambio de código**; se
documenta la paridad de comportamiento (decision D-032-3).

## 7. Proceso i18n y riesgos

- Añadir clave: `ui/i18n/strings.go` (Table) + espejo
  `ui/frontend/src/shared/i18n.js` + `strings_test.go:TestTableComplete` (guard).
- Riesgos: (a) churn de flags rompe scripts de usuario (documentado); (b) GUI
  default de quantizer debe pasar de 0→3 (markup valor select y omit-lógica en
  `runner.go`); (c) `--json` primeros args (`runner.go:285-286`) intacto.
