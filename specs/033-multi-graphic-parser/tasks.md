# Tasks: 033 - Parser multi-`<Graphic>` (canvas unión por evento)

**Input**: `spec.md` + `plan.md`. TDD, fases acotadas, gates.

## Phase 0 — Baseline y firma de doctrina

- [x] **T000** Congelar `baseline_hashes_phase33.txt` (artefactos batería 1 +
  ncbd, estado pre-spec, = post-030 congelado en phase30).
  → `baseline_hashes_phase33.txt` = `final_hashes_phase31.txt` (12 líneas); `sha256sum -c` OK.
- [x] **T001** Firmar D-033-1 con citas del oráculo (filestreams.py:190-246 y
  :306-326 canvas+validación; render2.py:157-201 window-split; :1304-1383
  objeto=ventana) — doc corto en plan.md, sinheder antes del fix.
  → D-033-1 FIRMADA en `plan.md` con las 5 citas del oráculo (sin reformular).

## Phase 1 — RED (tests geométricos)

- [x] **T002** Test RED geometría ncbd: WDS=((200,953,718x53),(1000,913,833x106)),
  ODS 718x53 / 833x106, cobjs por DS == oráculo (4 variantes flags) — RED contra
  port actual (WDS (200,953,551x43)+(767,953,151x53)).
  → RED duro en `d7_cobjs_reacq_parity_test.cpp:245` (3 DS divergentes) contra el fixture viejo.
- [x] **T003** Quitar el SKIP del test geométrico D7 (:248) — RED dura.
  → SKIP eliminado; `EXPECT_EQ(divergent,0)`.

## Phase 2 — Fix parser+modelo

- [x] **T004** `filestreams.cpp:152-184`: iterar todos los `<Graphic>` por
  evento; construir box unión y canvas (paste con offset relativo); el
  evento expone 1 imagen+box al pipeline (como oráculo) con la lista por
  Graphic cuando un test la necesite.
  → `load_image()` compone el canvas (paste RGBA en offset relativo); box = unión; evento = 1 imagen+box.
- [x] **T005** `filestreams.h:25-58`: extender modelo evento (lista Graphics +
  canvas), títulos de validación (no-solape, declared==real) con error explícito
  (US2).
  → `bdn_graphic_t` + `m_graphics`; US2 (declared==real + no-solape, `stbi_info`) acotado a N>1 (ver disclosure).
- [x] **T006** GREEN T002/T003 (encadenar con render 027 por ventana intacto).
  → `DFGsReacqParity.Geometry` verde duro tras regenerar `nc_cpp_*`; 4 variantes casan en estado+cobjs+WDS+ODS.

## Phase 3 — Gates y cierre

- [x] **T007** Gate G1: batería 1 — hashes byte-idénticos vs phase30 (caso
  N=1 sin branch nuevo). Cualquier drift → STOP y abrir decision D-033-4.
  → `cpp_base`/`cpp_an`/`cpp_ov`/`cpp_an_ov` byte-idénticos (2b1ed728 / 97badc03): sin drift (D-033-4 no abierta).
- [x] **T008** Gate G2: ncbd re-baseline `final_hashes_phase33.txt` (solo
  nc_cpp_* cambian); 4 variantes de flags; test geométrico verde duro.
  → Solo las 4 `nc_cpp_*` (92fbeb09→75087816 base/an, 2305a7bf→fd92c6a7 overlap/an_ov); `nc_py_*` intactos; `sha256sum -c` OK.
- [x] **T009** Suite completa verde (sin skips nuevos, salvo fixtures por diseño);
  CHANGELOG [033]; cambiar status de spec a Implementada; memoria + commits.
  → NO ejecutado por instrucción ("NO COMMITEES, NO CHANGELOG"). Suite 173/173 (1 skip por diseño: OverlapGapClear.WdsOdsGeometry, de 031).
