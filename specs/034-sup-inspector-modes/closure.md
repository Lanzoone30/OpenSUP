# Cierre 034 — Modos inspector del CLI (`validate` / `inspect` / `stats`)

**Fecha**: 2026-10-19 · **Estado**: Implementada (sin commit, sin CHANGELOG).

## Alcance entregado

- **US4/T002** — `media/rle_decode.{h,cpp}`: `decode_rle`, `decode_rle_checked`,
  `rle_stats`. Espejo de `encode_rle` (`brule`); roundtrip probado sobre imagen
  plana, transparente, con bandas, runs cortos y runs >63 px (14 bits).
- **T000/T001** — `tests/fixtures/inspector/{gen_mini_sup.py,mini.sup}` (566 B:
  1 epoch, 2 DS — EPOCH_START con paleta completa + ACQUISITION, 1 ventana
  64x32, 1920x1080) y `read/sup_reader.{h,cpp}`: parser de cabeceras/segmentos a
  los structs existentes (`pcs_c`/`wds_c`/`pds_c`/`ods_c`/`ends_c`), agrupación
  en display sets (cierre en ENDS) y epochs (PCS con `EPOCH_START`).
- **T003** — dispatch en `main.cpp` vía `cli::is_inspect_subcommand` (literal
  `argv[1]`) + `cli::run_inspector` (app CLI11 propia con los tres subcomandos).
- **T004** — `inspect`: dump humano (PCS/WDS ventanas/PDS/ODS con dims y flags),
  `--ds N`.
- **T005** — `validate` (`read/inspect.{h,cpp}`): porte de `test_diplayset`,
  `is_compliant` y `check_pts_dts_sanity` de `pgstream.py`, más `test_rx_bitrate`
  opt-in (`--max-kbps`). Exit 0 conforme / 1 violaciones duras / 2 fichero
  ilegible. `--json` emite NDJSON (`error`/`warn` por hallazgo + resumen).
- **T006** — `stats` humano + `--json`; censo del encode en el `done`: nuevo
  `core/pgstats.{h,cpp}` (`pg_counters_t::count_pg_segments`) cableado en
  `main.cpp` desde `bdn_render_c::segments()`/`reuse_candidates()`, emitido por
  `json_emitter.cpp` y espejado en `ui/engine/protocol.go`.

## Decisiones y desviaciones

- **D-034-A (T003, desviación)**: NO se quitó el `->required()` de encode-io ni
  se aplicó `sc->fallthrough()`. Se implementó la vía del ``research §4``/
  D-034-1: **pre-scan de `argv[1]` + app inspector separada**. El parser de
  encode queda intacto (riesgo de regresión del CLI = 0; G2 pasa a trivial).
- **D-034-B**: dos implementaciones de contadores, verificadas equivalentes:
  `read::stream_stats` sobre display sets parseados (oráculo `debug_stats`) y
  `core::count_pg_segments` sobre lista plana (lado encode). El test
  `InspectorTest.StatsMatchEncodeSideCounters` fija la equivalencia.
- **D-034-C**: no se portó el `is_dupe` profundo (`pgstream.py:227-243`). El aviso
  de número de composición se inhibe cuando `comp_n == prev` (señal de
  duplicado). Es *warning*, no afecta a `compliant`.
- **D-034-D**: dos avisos difieren en alcance del oráculo por diseño:
  `object/palette` (entradas de paleta no definidas) y «dos DS con el mismo
  PTS». Ambos son blandos.
- **D-034-E**: el modelo `PGObjectBuffer` se portó sin `writable_at`/bloqueo de
  versiones (el oráculo en `is_compliant` sólo usa `get`/`allocate_id`): 64 slots
  y presupuesto `w*h` contra `DECODED_BUF_SIZE`.
- **D-034-F**: además de `test_diplayset`, se portaron las reglas duras que el
  oráculo aplica *dentro* de `is_compliant` (paleta limited-range/0xFF, buffer
  de objetos, cadena y buffer codificado de ODS, RLE, colocación/caducidad de
  CObjects). Sin ellas la paridad medida cae de 193/193 a 179/193.

## Verificación

- **Paridad con el oráculo**: sobre los **267** `.sup` del repo, `validate`
  coincide con `SUPer.pgstream.is_compliant` + `check_pts_dts_sanity` en
  **193/193** de los que el propio parser de SUPer acepta (74 los rechaza SUPer,
  excluidos): **0 discrepancias**. Evidencia:
  `temp/`… script de comparación (ver «Reproducir»).
- **G1**: `validate`/`inspect`/`stats` sobre los 267 artefactos sin crash (rc ≤ 2
  en todos; 0 señales). **17/17** streams generados por el oráculo (`py_*`,
  `oracle_*`) en **GREEN** (exit 0).
- **Censo idéntico al oráculo** en un encode fresco byte-idéntico:
  `n(ES)=1, n(ACQ)=1, n(NC)=1 (n(ODS)=0, n(PU)=0). Buffered: n(PU)=0.` ==
  `debug_stats`.
- **G0/G2**: suite **203/203** (1 skip preexistente y no tocado:
  `OverlapGapClear.WdsOdsGeometry`); `go test ./...` verde; no hay cambios en
  `ui/frontend/` (build no ejecutado por no haber diff).
- **No regresión del encoder**: `sha256sum -c final_hashes_phase32.txt` =
  **12/12 OK** (incluye `nc_cpp_base` 75087816); el encode actual del fixture
  ncbdn es **byte-idéntico** al artefacto versionado.
- **Tests nuevos**: `rle_decode_test` (8), `sup_reader_test` (4),
  `inspector_test` (8) — incluido el **RED por fixture envenenado en el test**
  (paleta fuera de rango, EPOCH_START intra-epoch, ventana fuera de pantalla).

## Hallazgo fuera de alcance (031/035, no 034)

El encode multi-ventana del fixture ncbdn produce un stream **no conforme** según
SUPer, reproducido por el binario actual (byte-idéntico a
`tests/fixtures/ncbdn/nc_cpp_base.sup`):

- DS0 y DS1 emiten **dos PDS con el mismo `p_id=0`** (`p_vn` 0 y 1) en lugar del
  **único PDS** (`p_vn=0`) que emite SUPer antes de ambos ODS →
  `test_diplayset/pds` (id de paleta duplicado).
- El segundo PDS tiene **DTS anterior** al primero (893919 < 894060) y menor que
  el DTS del PCS → `pts_dts/sanity`. SUPer: `is_compliant (False,1)` +
  `check_pts_dts_sanity False` ⇒ su propio `valid_stream=False`.

Es un defecto del renderer (paleta dedicada por ventana), no del parser ni del
inspector; 034 es de sólo lectura y no lo toca. Requiere spec de comportamiento
propia.

## Residuos

- `inspect --json` no se implementó: `spec.md` US2 lo marca «(--json migrable)»
  y el volcado humano ya cubre US2. Se añade cuando se pida.
- Cross-build MinGW no ejecutado (034 no añade dependencias: `opensup_read` es
  C++17 puro sobre `opensup_core`).
- En el commit hay que **`git add -f tests/fixtures/inspector/mini.sup`**
  (`*.sup` está en `.gitignore`; los `ncbdn/*.sup` siguen la misma convención).
  `gen_mini_sup.py` queda versionado para regenerarlo.
- CIEGO heredado del oráculo: `pds_vn` NO se limpia en adquisición
  (`pgstream.py:199/250`), así que un ACQ que no redefina su paleta pasa.
- `ui/engine/protocol.go` tiene un newline final ausente preexistente (gofmt lo
  marca); no tocado (fuera de alcance).
- No ejecutado por instrucción: `CHANGELOG.md`, commit y cambio de estado en
  `spec.md`.

## Reproducir

```bash
cd build && cmake --build . -j4 && ctest -j2
./src/opensup/cli/opensup_cli validate tests/fixtures/inspector/mini.sup
./src/opensup/cli/opensup_cli inspect --ds 0 tests/fixtures/inspector/mini.sup
./src/opensup/cli/opensup_cli stats --json tests/fixtures/inspector/mini.sup
./src/opensup/cli/opensup_cli -i tests/fixtures/ncbdn/BDN_Index.xml /tmp/o.sup -y --json | tail -1
```
