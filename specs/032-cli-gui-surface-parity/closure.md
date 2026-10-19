# Cierre spec 032 (2026-10-19)

Superficie CLI/GUI alineada con SUPer; el stream del encode por defecto no
cambia (gate byte-identity 12/12 OK).

### Cambios

- **CLI surface (`cli_parser.{h,cpp}`)**: light cutover de shorts —
  `-d/--ahead` (antes `--overlap`), `-t/--threads` (antes `-j`),
  `-n/--allow-normal`, `--ssim-tol` y `--debug` long-only. `--overlap` y `-j`
  se conservan como alias deprecados con warning de un disparo.
- **Quantizer IDs SUPer (`cli_parser.cpp`, `optimizer.cpp`, `interface.h`,
  `epoch_encoder.h`, `encode_epoch.cpp`)**: `-q` acepta 0..4, default 3; fuera
  de rango → 3 + warning; 0/1/4 (no compilados) → fail-fast
  `backend not available in this build (available: 2, 3)`. Nuevo
  `optimiser_c::index_for` mapea 2→HexTree, 3→LIQ (único sitio de verdad). El
  default 3 cae en el índice LIQ histórico, por lo que el stream es idéntico.
- **prefer-normal (`-k/--prefer-normal`)**: flag + regla de fuerza
  `max(prefer, allow)` (supercli.py:148-150).
- **log-to-file (`-l/--log-to-file`)**: int `{0,10,20,30}`; `-10` (Succint)
  avisa y cae a estándar (D-032-2); `>40` desactiva con warn; escribe
  `<outfile>_build.log` vía `set_file_output` (`main.cpp`).
- **GUI (Go + frontend)**: `runner.go` emite los shorts nuevos y pasa
  `PreferNormalCase`/`LogToFile`; combo quantizer con ids 2/3 y textos SUPer;
  checkbox `chk_prefer_normal`; combo `combo_log`; labels EN alineadas al audit
  y ES corregidas; `alternateOids` des-invertido (label + tooltip).
- **i18n**: `strings.go` y `shared/i18n.js` espejo (74 claves).

### Acotación — D-032-5 (prefer normal case)

La regla fiel del oráculo (`render2.py:544`) necesita la cadena de nodos de
decodificación (buffer `j`/`j_nc`), que el encoder no modela; en el modelo
actual `allow` ya se comporta como prefer-always. Cablear `prefer` exigiría una
spec de comportamiento 030-style y tocaría el stream. Scope revertido a
**flag + storage** según D-032-5; `-k` fuerza `allow`. Documentado en
`epoch_encoder.h`/`interface.h`. T012 (paridad `-k` vs oráculo) queda diferido
por el mismo motivo.

### Gates

- baseline / final: `baseline_hashes_phase32.txt`, `final_hashes_phase32.txt`.
  `sha256sum -c` = **12/12 OK**: `cpp_base`/`cpp_an` (2b1ed728), `cpp_ov`/
  `cpp_an_ov` (97badc03), `nc_cpp_base`/`nc_cpp_an` (75087816), `nc_cpp_overlap`/
  `nc_cpp_an_ov` (fd92c6a7) y los `py_*` intactos. Stream default byte-idéntico.
- CLI help exacto; fail-fast 0/1/4 y alias-warn verificados end-to-end.
- Suite C++ **183/183** (1 skip: `OverlapGapClear.WdsOdsGeometry`); Go
  `go test ./...` verde.

### Extra (031)

Re-evaluación post-033 de `OverlapGapClear.WdsOdsGeometry`: sigue divergiendo
(30 DS). 033 no cubre este fixture (ENIGMA-1080p es single-graphic por evento),
así que la residencia WDS/ODS no es regresión del gap-clear ni del parser
multi-`<Graphic>`. Skip conservado con mensaje actualizado.

### No ejecutado por instrucción

`CHANGELOG.md` (prohibido), commits y cambio de status en `spec.md`.
