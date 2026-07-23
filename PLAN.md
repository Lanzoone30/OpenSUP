# PLAN: Migración OpenSUP de Python a C++17

> **Fecha de inicio:** Jul 21, 2026
> **Objetivo:** Reescribir completo OpenSUP en C++17 usando CMake, Qt6, y patrones de ingeniería de MKVToolNix.

---

## Tabla de Contenidos

- [1. Decisiones Confirmadas](#1-decisiones-confirmadas)
- [2. Análisis del Proyecto Actual](#2-análisis-del-proyecto-actual)
- [3. Referencia: MKVToolNix](#3-referencia-mkvtoolnix)
- [4. Estructura del Repositorio](#4-estructura-del-repositorio)
- [5. Dependencias C++](#5-dependencias-cpp)
- [6. Plan por Fases](#6-plan-por-fases)
- [7. Convenciones de Código](#7-convenciones-de-código)
- [8. Buenas Prácticas de MKVToolNix](#8-buenas-prácticas-de-mkvtoolnix)
- [9. Mapeo de Módulos Python → C++](#9-mapeo-de-módulos-python--c)
- [10. Estimación de Esfuerzo](#10-estimación-de-esfuerzo)

---

## 1. Decisiones Confirmadas

| Decisión | Elección | Justificación |
|----------|----------|---------------|
| **Lenguaje** | C++17 | Máximo rendimiento, distribución nativa, aprendizaje |
| **Build system** | CMake | Estándar de la industria, multiplataforma, IDE support |
| **GUI** | Qt6 | Como MKVToolNix, maduro, bien documentado |
| **Quantizer** | libimagequant + HexTree con fallback | Mejor calidad + alternativa propia |
| **Ubicación** | Mismo repo, reorganizado | Python → `legacy/`, C++ en raíz |
| **Objetivos** | Rendimiento + distribución + aprendizaje | Full rewrite completo |

---

## 2. Análisis del Proyecto Actual

### 2.1 OpenSUP (Python)

| Métrica | Valor |
|---------|-------|
| Archivos fuente | 24 archivos .py |
| Líneas totales | ~7,700 |
| Entry points | 3 (main.py, __main__.py, cli.py/gui.py) |
| Subpaquetes | 3 (core, media, utils) |
| Dependencias core | Pillow, numpy, OpenCV, libimagequant, SSIM-PIL |
| GUI | CustomTkinter |
| Build | PyInstaller (.spec) |

### 2.2 Arquitectura Actual

```
  UI Layer                    Pipeline Layer               Infrastructure Layer
  ─────────                   ──────────────               ────────────────────
  gui.py (CustomTkinter)  ─┐
  cli.py (argparse)       ─┤
                           │
                     core/interface.py (BDNRender orchestrator)
                       ├── core/renderer.py   (bitmap rendering)
                       ├── core/segments.py   (PGS segment I/O)
                       └── core/filestreams.py (BDN XML parse + SUP write)
                              │
                       media/optimizer.py     (quantization backends)
                       media/palette.py       (palette manipulation)
                       media/pgraphics.py     (PGS graphics objects)
                       media/pgstream.py      (packetised streams)
                              │
                       utils/logging.py       (LogFacility singleton)
                       utils/bdvideo.py       (video properties)
                       utils/geometry.py      (Pos, Shape, Box)
                       utils/color_matrix.py  (BT matrices)
                       utils/timecode.py      (TC conversion)
                       utils/ssim.py          (SSIM comparison)
```

### 2.3 Ficheros Más Complejos (prioridad de migración)

| Archivo | Líneas | Complejidad | Notas |
|---------|--------|-------------|-------|
| `core/renderer.py` | 1,609 | **MUY ALTA** | FSM de encoding, timing model, CTU |
| `core/segments.py` | 1,102 | Media | Serialización binaria big-endian |
| `core/interface.py` | 755 | Alta | Orquestador + multiprocessing |
| `media/optimizer.py` | 656 | Alta | Numpy-heavy palette solver |
| `core/filestreams.py` | 507 | Media | XML parsing + SUP writer |
| `media/pgstream.py` | 434 | Media | Compliance + buffer model |
| `media/pgraphics.py` | 342 | Media | RLE codec + decoder model |
| `media/palette.py` | 260 | Baja | Structs + color math |
| `utils/` (6 archivos) | ~500 | Baja | Tipos y math |

---

## 3. Referencia: MKVToolNix

MKVToolNix es un proyecto C++ de 2,372 archivos que sirve como referencia de ingeniería:

- **C++17** con Qt6 para GUI
- **~1,004 archivos fuente** (.cpp/.h)
- **~784 tests de integración** (Ruby)
- **Build**: Autotools + Ruby Rake
- **Licencia**: GPLv2

### Qué aprendimos de MKVToolNix

1. **Precompiled headers** (`common_pch.h`) para reducir tiempos de compilación
2. **Naming consistente**: `_c` (clases), `_t` (structs), `_e` (enums), `_x` (excepciones)
3. **Prefijo `m_`** para miembros de clase, `ms_` para estáticos
4. **Jerarquía de excepciones** custom con `mxerror()` para errores fatales
5. **Strategy pattern** para readers/packetizers (virtual dispatch)
6. **Namespace organization**: `mtx::` top-level con subnamespaces
7. **fmtlib** para formateo de strings universal
8. **RAII** con scope guards (`at_scope_exit_c`)
9. **Separación de librerías estáticas** por módulo
10. **Tests separados**: Google Test para unit, scripts para integración

---

## 4. Estructura del Repositorio

### 4.1 Después de la reorganización

```
OpenSUP-dev/
│
├── Referencias/                     ← Proyectos de referencia (solo lectura)
│   ├── mkvtoolnix-main/             ← MKVToolNix (referencia de ingeniería)
│   └── legacy/                      ← Código Python original (preservado)
│       ├── src/
│       │   └── opensup/             ← Paquete Python completo
│       │       ├── core/
│       │       ├── media/
│       │       └── utils/
│       ├── main.py
│       ├── pyproject.toml
│       ├── requirements.txt
│       └── *.spec
│
├── assets/                          ← Iconos y recursos
│   └── OpenSup.ico
│
├── cmake/                           ← Módulos CMake
│   └── CompilerWarnings.cmake       ← Warning flags (GCC/Clang/MSVC)
│
├── CMakeLists.txt                   ← Build principal C++17
│
├── src/                             ← Código fuente C++
│   └── opensup/
│       ├── pch.h                    ← Precompiled header
│       │
│       ├── common/                  ← Librería compartida (Fase 1 ✅)
│       │   ├── CMakeLists.txt
│       │   ├── error.h/.cpp         ← opensup_error_x jerarquía
│       │   ├── memory.h/.cpp        ← memory_c (alloc/clone/borrow)
│       │   ├── geometry.h/.cpp      ← box_t, shape_t, pos_t
│       │   ├── bdvideo.h/.cpp       ← fps_e, video_format_e, pcsfps_e
│       │   ├── timecode.h/.cpp      ← tc_t (MPEGTS_FREQ)
│       │   ├── color_matrix.h/.cpp  ← BT.601/709/2020 constexpr
│       │   ├── logger.h/.cpp        ← LogFacility singleton
│       │   └── ssim.h/.cpp          ← stub (requiere OpenCV)
│       │
│       ├── core/                    ← Pipeline de encoding (Fase 2-3)
│       │   ├── segments.h/.cpp      ← PGS segments
│       │   ├── filestreams.h/.cpp   ← BDN XML + SUP writer
│       │   ├── renderer.h/.cpp      ← EpochEncoder, DSNode, CTU
│       │   └── interface.h/.cpp     ← BDNRender orchestrator
│       │
│       ├── media/                   ← Quantización (Fase 2-3)
│       │   ├── optimizer.h/.cpp     ← Quantizer backends
│       │   ├── palette.h/.cpp       ← PaletteEntry, Palette
│       │   ├── pgraphics.h/.cpp     ← RLE, PGDecoder
│       │   └── pgstream.h/.cpp      ← Compliance, LeakyBuffer
│       │
│       ├── cli/                     ← CLI entry point (Fase 4)
│       │   ├── main.cpp
│       │   └── cli_parser.h/.cpp    ← CLI11 wrapper
│       │
│       └── gui/                     ← Qt6 GUI (Fase 5 ✅)
│           ├── CMakeLists.txt
│           ├── main_window.h/.cpp   ← MainWindow + signals/slots
│           ├── encode_worker.h/.cpp ← Worker QThread
│           ├── qt_log_handler.h/.cpp← Logger → Qt bridge
│           ├── main.cpp             ← QApplication entry point
│           └── main_window.ui       ← Diseñado en Qt Designer
│
├── tests/
│   ├── CMakeLists.txt               ← Google Test (FetchContent)
│   │
│   ├── common/                      ← Tests Fase 1 ✅ (38 tests)
│   │   ├── CMakeLists.txt
│   │   ├── geometry_test.cpp
│   │   ├── timecode_test.cpp
│   │   ├── bdvideo_test.cpp
│   │   ├── color_matrix_test.cpp
│   │   └── memory_test.cpp
│   │
│   ├── core/                        ← Tests Fase 2 ✅ (13 tests)
│   │   ├── CMakeLists.txt
│   │   └── segments_test.cpp
│   │
│   └── media/                       ← Tests Fase 2 ✅ (24 tests)
│       ├── CMakeLists.txt
│       ├── palette_test.cpp
│       └── pgraphics_test.cpp
│
├── build/                           ← Build directory (gitignored)
│
├── docs/
│   ├── GUI_PLAN.md                    ← Diseño detallado de GUI Qt6
│   └── diagrams/
│
├── .gitignore
├── LICENSE
├── README.md
└── PLAN.md
```

---

## 5. Dependencias C++

### 5.1 Mapeo de Dependencias

| Python | C++ | Tipo | Uso |
|--------|-----|------|-----|
| `numpy` | Raw arrays + `<algorithm>` | Core | Arrays, math, operaciones vectoriales |
| `Pillow` | stb_image + custom RGBA | I/O | Carga de PNG, manipulación de píxeles |
| `opencv-python` | OpenCV C++ (`find_package`) | Externa | GaussianBlur, Sobel, SSIM |
| `brule` (RLE) | Implementación propia en `pgraphics.cpp` | Interna | ~200 líneas RLE codec |
| `brule` (LayoutEngine) | Port del algoritmo | Interna | ~150 líneas window splitting |
| `brule` (HexTree) | Port a C++ | Interna | Quantizer secundario |
| `brule` (QtzrUTC) | Reemplazado por libimagequant | N/A | Obsoleto |
| `piliq` (libimagequant) | libimagequant C API directo | Externa | Quantizer principal |
| `timecode` | Reimplementación propia en `common/timecode.cpp` | Interna | ✅ 85 líneas |
| `SSIM-PIL` | OpenCV SSIM o custom | Externa | Quality metric (stub en Fase 1) |
| `argparse` | CLI11 v2.6.2 (FetchContent) | Interna/Header | CLI argument parsing |
| `customtkinter` | Qt6 | Externa | GUI (Fase 5) |
| `tqdm` | Custom progress en Qt6/CLI | Interna | UI concern |

### 5.2 Estado Actual de Dependencias (post-Fase 1)

| Librería | Estado | Notas |
|----------|--------|-------|
| **C++17** stdlib | ✅ Instalada | g++ 15.2.1, Fedora 43 |
| **CMake** ≥ 3.16 | ✅ 3.31.11 | |
| **Google Test** | ✅ v1.14.0 (FetchContent) | 38 tests pasando |
| **CLI11** | ✅ v2.6.2 (FetchContent) | Fase 4 |
| **stb_image** | ✅ Descargado a `extern/` | Fase 2 |
| **pugixml** | ✅ 1.16 (dnf) | Fase 3 |
| **libimagequant** | ✅ 4.0.3 (dnf) | Fase 3 |
| **OpenCV** | 🔴 Pendiente | SSIM, filtros (baja prioridad) |
| **Qt6** | 🔴 Pendiente | Fase 5 (GUI) |

---

## 6. Plan por Fases

### Fase 1: Fundación + Reorganización (Semana 1)

**Objetivo:** Reorganizar el repo y establecer la base C++.

**Tareas:**
1. Crear carpeta `legacy/` y mover todo el código Python
2. Actualizar `.gitignore` para C++
3. Setup `CMakeLists.txt` raíz con C++17, PCH, warnings
4. Crear `src/opensup/pch.h` con includes fundamentales
5. Implementar `common/`:
   - `memory_c` — gestión de buffers binarios con factory methods
   - `logger` — LogFacility singleton (port directo de logging.py)
   - `error` — Jerarquía de excepciones `opensup_error_x`
   - `geometry` — `box_t`, `shape_t`, `pos_t` (structs inmutables)
   - `bdvideo` — `fps_e`, `video_format_e`, `pcsfps_e`
   - `timecode` — `tc_t` (reimplementación de 59 líneas)
   - `color_matrix` — matrices BT.601/709/2020 como `constexpr`
   - `ssim` — stub (implementación completa requiere OpenCV, ver Fase 3)
6. Google Test setup + tests para common/

**Criterio de éxito:** Build CMake exitoso, tests de common/ pasando.

---

### Fase 2: Serialización + I/O (Semana 2)

**Objetivo:** Modelos de datos binarios y entrada/salida.

**Tareas:**
1. `core/segments.cpp` — Todos los PGS segment types:
   - `pg_segment_t` base con header de 13 bytes big-endian
   - `pcs_t`, `wds_t`, `pds_t`, `ods_t`, `ends_t`
   - `c_object_t`, `window_definition_t`
   - `display_set_t` (contenedor ordenado)
   - Serialización: `to_bytes()` / `from_bytes()` con `std::vector<uint8_t>`
2. `media/palette.cpp`:
   - `palette_entry_t` — struct {y, cr, cb, alpha}
   - `palette_t` — `std::map<uint8_t, palette_entry_t>` (sparse palette)
   - Operaciones de color: `to_rgba()`, `from_rgba()`, `diff()`
3. `media/pgraphics.cpp`:
   - RLE encoder/decoder (port del C extension de brule, ~200 líneas)
   - `pg_decoder_t` — constantes de timing (constexpr)
   - `prospective_object_t` — datos de visibilidad por frame
   - `pg_object_buffer_t` — simulador de buffer del decoder (64 slots)
   - `palette_manager_t` — 8 slots de paleta versionada
4. Tests para segments + palette + pgraphics

**Criterio de éxito:** Serialización/deserialización de PGS segments verificada con datos reales.

---

### Fase 3: Core Engine (Semanas 3-4)

**Objetivo:** El corazón del encoder — la parte más compleja.

**Tareas:**
1. `core/filestreams.cpp`:
   - `bdn_xml_t` — parser XML con pugixml
   - `bdn_xml_event_t` — evento con lazy loading de imagen (stb_image)
   - `sup_file_t` — lector de archivos .sup existentes
2. `media/optimizer.cpp`:
   - `quantizer_base_t` — interfaz virtual
   - `libimagequant_t` — backend principal (libimagequant C API)
   - `hextree_t` — backend secundario (port de brule)
   - `pilkm_t` — fallback Pillow-equivalent (stb_image quantize)
   - `optimise_t` — solver de paleta multi-frame + diff
3. `media/pgstream.cpp`:
   - `epoch_context_t` — layout del epoch
   - `leaky_buffer_t` — simulador de buffer PGS
   - `is_compliant()` — verificador de compliance
   - `check_pts_dts_sanity()` — validador de timestamps
4. `core/renderer.cpp` (**la parte más compleja**):
   - `epoch_encoder_t` — FSM principal de encoding (~700 líneas port)
   - `ds_node_t` — nodo de display set con timing model
   - `window_analyzer_t` — iterator SSIM para scene changes
   - `ctu_t` — comparación recursiva de imágenes
   - `padding_engine_t` — padding inteligente 8x8
5. `core/interface.cpp`:
   - `bdn_render_t` — orquestador del pipeline completo
   - `epoch_worker_t` — worker con `std::thread` + `std::queue`

**Criterio de éxito:** Encoding de un archivo BDN XML de prueba genera .sup válido y compliant.

---

### Fase 4: CLI (Semana 5)

**Objetivo:** Interfaz de línea de comandos completa con paridad功能。

**Tareas:**
1. `cli/main.cpp` — Entry point completo
2. CLI11 vía FetchContent (v2.6.2) + `cli/cli_parser.h/.cpp` — CLI11 wrapper
3. Paridad de opciones con `cli.py`:
   - `-i` input, `output`, `-c` compression, `-a` acqrate
   - `-q` quantizer (0-3), `-b` BT matrix, `-p` palette
   - `-d` overlap, `-y` overwrite, `-w` both formats
   - `-e` extra acq, `-m` max kbps, `-l` log level
   - `-t` threads, `--layout`, `--capabilities`, `--ssim-tol`
4. Tests de integración (ejecutar vs Python original, comparar .sup output)

**Criterio de éxito:** CLI C++ genera .sup idéntico (o near-identical) al CLI Python.

---

### Fase 5: GUI Qt6 (Semanas 6-8)

**Objetivo:** Interfaz gráfica moderna con Qt6.

**Documento de diseño detallado:** `docs/GUI_PLAN.md`

**Tareas:**
1. **Diseño de UI en Qt Designer** (tú):
   - Crear `src/opensup/gui/forms/main_window.ui` con todos los widgets
   - Seguir la especificación en `docs/GUI_PLAN.md`
2. **Código C++** (yo):
   - `gui/main_window.h/.cpp` — MainWindow + slots
   - `gui/encode_worker.h/.cpp` — Worker para QThread
   - `gui/qt_log_handler.h/.cpp` — Bridge logger → Qt signals
3. **Build system**:
   - `gui/CMakeLists.txt` com `find_package(Qt6)`, `qt_add_executable`, `qt_add_ui`
4. **Integración**:
   - Conectar signals de worker (progress, logLine, finished, etaUpdated)
   - Llamar a `bdn_render_c` desde el worker thread
   - Colorear log por nivel (PASS/FAIL verde/rojo)

**Criterio de éxito:** GUI funcional que puede cargar BDN XML y generar .sup.

### Fase 5.1: Refinamiento Log + Resolución + Extensión (Sesión 2)

**Objetivo:** Mejorar la experiencia de usuario con log detallado, validación de resolución y manejo de extensiones de archivo.

**Tareas:**

1. **Log GUI mejorado** (4 archivos):
   - `gui/main_window.ui`: Ajustar ventana (640x800), fuente Consolas 10pt, minimumSize log (300px)
   - `gui/main_window.cpp`: Formato `HH:MM:SS │ LEVEL │ msg`, arreglar bridge Qt (`qobject_cast` fix), persistencia (eliminar clear en start_encode), auto-append `.sup` extension, leer checkbox `chk_both_formats`
   - `gui/encode_worker.cpp`: Remover banners, mensajes limpios, emitir resultado del pipeline (eventos/epochs/segmentos/duration)
   - `common/logger.cpp`: Callback pasa mensaje crudo (no línea formateada) para evitar double timestamp; `log_fatal` usa level 9

2. **Validación de resolución** (4 archivos):
   - `core/interface.h` (`encode_config_t`): Nuevos campos `ignore_resolution` and `both_formats`
   - `core/interface.cpp`: Pasar `ignore_resolution` a `xml.parse()`, derivar paths si `both_formats`
   - `core/filestreams.cpp`: Validar resolución Blu-ray estándar (1080p/720p/576p/480p)
   - `cli/cli_parser.cpp`: Forward `ignore_resolution` and `both_formats`

3. **Filtrado de mensajes verbosos** (3 archivos, 6 puntos):
   - `core/filestreams.cpp`: Demover "Parsed N events" a hdebug, "Removed N duplicate" a hdebug, "Deduplication range mismatch" a hdebug
   - `core/interface.cpp`: Demover "Output: path" a hdebug
   - `media/optimizer.cpp`: Demover "libimagequant: N colors" e "HexTree: N colors" a hdebug

4. **Extensiones CLI** (2 archivos):
   - `cli/main.cpp`: Auto‑append `.sup` si no hay extensión
   - `cli/cli_parser.cpp`: Agregar opción `-w/--withsup` para both formats

**Criterio de éxito:** Log GUI mostra resumen detallado (resolución, eventos, epochs, segments, duración), sin mensajes verbosos. Auto‑append de extensión `.sup` y soporte para Both formats (PES stub).

### Fase 5.2: Bugfixes Encoding Pipeline (Sesión 2)

**Objetivo:** Corregir 9 bugs en el pipeline de encoding que causaban subtítulos invisibles en el .sup generado.

**Fase A — Críticos (subtítulos invisibles):**

| # | Bug | Archivo | Fix |
|---|-----|---------|-----|
| 1 | RLE encoder sin marcadores EOL/EOB | `media/pgraphics.cpp` | Agregar `0x00 0xFF 0x00` tras cada scanline + `0x00 0xFF 0x01` al final del bitmap |
| 2 | Zero‑run máximo 256 emite escape | `media/pgraphics.cpp` | Limitar chunk a 255 |
| 3 | ODS data_length off by 4 | `core/segments.cpp` | `rle_data.size() + 4` |
| 4 | CObject byte extra reservado | `core/segments.cpp` | Non‑cropped 8B (no 9), cropped 16B (no 17) |

**Fase B — Altos (renderizado incorrecto):**

| # | Bug | Archivo | Fix |
|---|-----|---------|-----|
| 5 | PCS state `normal` para eventos con ODS | `core/renderer.cpp` | Usar `acquisition` (0x40) siempre que haya ODS |
| 6 | Timestamps idénticos para todos los segments | `core/renderer.cpp` | Per‑segment PTS/DTS: PCS=presentation, WDS=‑wipe, PDS=base_dts, ODS=base_pts, ENDS=base_pts |

**Fase C — Medios (completitud):**

| # | Bug | Archivo | Fix |
|---|-----|---------|-----|
| 7 | Bitmap 8x8 padding calculado pero no usado | `core/renderer.cpp` | Pad indexed bitmap + usar dims padded para ODS/WDS |
| 8 | Palette 0xFF colisiona con escape RLE | `core/renderer.cpp` | Limitar quantizer a 254 colores |
| 9 | pcs_c::from_scratch pre‑alloca ceros | `core/segments.cpp` | `pl(11)` en vez de `pl(11 + N*9)` |

**Criterio de éxito:** .sup generado se multiplexa correctamente y los subtítulos son visibles en el reproductor.

---

### Fase 6: Empaquetado y Distribución (Semana 9)

**Objetivo:** Binarios listos para distribución multiplataforma.

**Tareas:**
1. CPack configuration:
   - Linux: .deb + .AppImage
   - macOS: .app bundle
   - Windows: NSIS installer
2. Cross-compilation support
3. GitHub Actions CI/CD
4. Actualizar README.md con instrucciones de build C++

**Criterio de éxito:** Binarios ejecutables en Linux, macOS y Windows sin dependencias externas.

---

## 7. Convenciones de Código

### 7.1 Naming Conventions (de MKVToolNix)

| Elemento | Convención | Ejemplo |
|----------|-----------|---------|
| Clases | `snake_case_c` | `epoch_encoder_c`, `memory_c` |
| Structs | `snake_case_t` | `box_t`, `palette_entry_t`, `epoch_context_t` |
| Enums | `snake_case_e` | `composition_state_e`, `fps_e` |
| Excepciones | `snake_case_x` | `encoding_error_x`, `opensup_error_x` |
| Miembros de instancia | `m_` prefix | `m_width`, `m_events`, `m_data` |
| Miembros estáticos | `ms_` prefix | `ms_freq`, `ms_default_logger` |
| Funciones | `snake_case` | `encode()`, `from_bytes()`, `to_rgba()` |
| Namespaces | `snake_case` | `opensup::core`, `opensup::media` |
| Smart pointer typedefs | `cptr` suffix | `memory_cptr`, `epoch_cptr` |
| Constantes | `constexpr` o `UPPER_CASE` | `constexpr double RX = 2e6;` |

### 7.2 Estructura de Archivos

```
// Ejemplo: common/geometry.h
#pragma once

#include <cstdint>

namespace opensup {
namespace common {

struct pos_t {
    int32_t x = 0;
    int32_t y = 0;
};

struct shape_t {
    int32_t w = 0;
    int32_t h = 0;

    [[nodiscard]] constexpr int32_t area() const noexcept {
        return w * h;
    }
};

struct box_t {
    int32_t y  = 0;
    int32_t dy = 0;
    int32_t x  = 0;
    int32_t dx = 0;

    [[nodiscard]] constexpr int32_t area() const noexcept {
        return dx * dy;
    }

    [[nodiscard]] constexpr int32_t x2() const noexcept {
        return x + dx;
    }

    [[nodiscard]] constexpr int32_t y2() const noexcept {
        return y + dy;
    }

    static box_t intersect(const box_t& a, const box_t& b) noexcept;
    static box_t union_box(const box_t& a, const box_t& b) noexcept;
};

} // namespace common
} // namespace opensup
```

### 7.3 Header vs Source

- **Headers (.h)**: Declaraciones, constexpr, inline functions, templates
- **Source (.cpp)**: Implementaciones, incluye su propio header + `pch.h`
- **PCH**: Todo `.cpp` empieza con `#include "opensup/pch.h"`

### 7.4 Gestión de Memoria

```cpp
// Usar smart pointers para todo objeto heap-allocated
using memory_cptr = std::shared_ptr<memory_c>;

// RAII para recursos
auto buffer = memory_c::alloc(size);
// buffer se libera automáticamente al salir del scope

// Scope guards para cleanup
auto guard = at_scope_exit_c([&]() { cleanup(); });
```

### 7.5 Error Handling

```cpp
// Excepciones para errores recuperables
throw opensup::common::encoding_error_x("Failed to encode frame {}", frame_id);

// Fatal errors (como mxerror en MKVToolNix)
void log_fatal(const std::string& msg);

// Warnings e info
void log_warn(const std::string& msg);
void log_info(const std::string& msg);
```

---

## 8. Buenas Prácticas de MKVToolNix

### 8.1 Precompiled Headers

MKVToolNix usa un `common_pch.h` que incluye todas las headers fundamentales. Esto reduce drásticamente los tiempos de compilación因为我们不需要 que cada archivo incluya individualmente 20+ headers.

```cpp
// pch.h - Incluido en todo .cpp
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <fmt/format.h>
// ... headers del proyecto
```

### 8.2 Strategy Pattern para Backends

```cpp
// Interfaz abstracta para quantizers
class quantizer_base_t {
public:
    virtual ~quantizer_base_t() = default;
    virtual std::pair<std::vector<uint8_t>, std::vector<palette_entry_t>>
        quantize(const uint8_t* rgba, int width, int height, int colors) = 0;
    virtual const char* name() const = 0;
};

// Backends concretos
class libimagequant_t : public quantizer_base_t { ... };
class hextree_t : public quantizer_base_t { ... };

// Fallback chain
std::unique_ptr<quantizer_base_t> select_quantizer(int id) {
    switch (id) {
        case 0: return std::make_unique<libimagequant_t>();
        case 1: return std::make_unique<hextree_t>();
        default: return create_fallback();
    }
}
```

### 8.3 Factory Methods para Serialization

```cpp
// Como memory_c en MKVToolNix
class memory_c {
public:
    static std::shared_ptr<memory_c> alloc(size_t size);
    static std::shared_ptr<memory_c> clone(const uint8_t* data, size_t size);
    static std::shared_ptr<memory_c> borrow(uint8_t* data, size_t size);

    const uint8_t* get_buffer() const;
    uint8_t* get_buffer();
    size_t get_size() const;
};
```

### 8.4 Namespace Organization

```cpp
// Jerarquía de namespaces como MKVToolNix
namespace opensup {
    namespace common { ... }   // Utilidades compartidas
    namespace core { ... }     // Pipeline de encoding
    namespace media { ... }    // Quantización y graphics
    namespace cli { ... }      // CLI entry point
    namespace gui { ... }      // Qt6 GUI
}
```

### 8.5 Logging Estructurado

```cpp
// Como mxerror/mxwarn/mxinfo en MKVToolNix
namespace opensup {
namespace log {

enum class level_t { debug, info, warn, error, fatal };

void set_level(level_t level);
void log(level_t level, const std::string& msg);

// Macros convenientes
#define SUP_LOG_ERROR(msg) opensup::log::log(level_t::error, msg)
#define SUP_LOG_WARN(msg)  opensup::log::log(level_t::warn, msg)
#define SUP_LOG_INFO(msg)  opensup::log::log(level_t::info, msg)

} // namespace log
} // namespace opensup
```

### 8.6 RAII y Scope Guards

```cpp
// Como at_scope_exit_c en MKVToolNix
class at_scope_exit_c {
public:
    explicit at_scope_exit_c(std::function<void()> func)
        : m_func(std::move(func)) {}
    ~at_scope_exit_c() { if (m_func) m_func(); }

    at_scope_exit_c(const at_scope_exit_c&) = delete;
    at_scope_exit_c& operator=(const at_scope_exit_c&) = delete;

private:
    std::function<void()> m_func;
};

// Uso
void process_file(const std::string& path) {
    auto file = open_file(path);
    auto guard = at_scope_exit_c([&]() { close_file(file); });
    // ... procesamiento
}
```

### 8.7 Testing Patterns

```cpp
// Google Test como MKVToolNix
#include <gtest/gtest.h>
#include "opensup/common/geometry.h"

namespace {

TEST(BoxTest, AreaCalculation) {
    opensup::common::box_t box{.y=0, .dy=100, .x=0, .dx=200};
    EXPECT_EQ(box.area(), 20000);
}

TEST(BoxTest, Intersection) {
    auto a = box_t{0, 50, 0, 50};
    auto b = box_t{25, 50, 25, 50};
    auto result = box_t::intersect(a, b);
    EXPECT_EQ(result.area(), 625);
}

} // anonymous namespace
```

---

## 9. Mapeo de Módulos Python → C++

### 9.1 utils/ → common/

| Python | C++ | Líneas est. |
|--------|-----|-------------|
| `geometry.py` (Box, Shape, Pos) | `common/geometry.h/.cpp` | ~150 |
| `timecode.py` (TC) | `common/timecode.h/.cpp` | ~100 |
| `bdvideo.py` (BDVideo, FPS) | `common/bdvideo.h/.cpp` | ~200 |
| `color_matrix.py` (BT matrices) | `common/color_matrix.h/.cpp` | ~120 |
| `ssim.py` (SSIMPW) | `common/ssim.h/.cpp` | ~60 |
| `logging.py` (LogFacility) | `common/logger.h/.cpp` | ~200 |

### 9.2 core/ → core/

| Python | C++ | Líneas est. |
|--------|-----|-------------|
| `segments.py` (PGSegment, PCS, ODS...) | `core/segments.h/.cpp` | ~1,200 |
| `filestreams.py` (BDNXML, SUPFile) | `core/filestreams.h/.cpp` | ~600 |
| `renderer.py` (EpochEncoder, DSNode) | `core/renderer.h/.cpp` | ~1,800 |
| `interface.py` (BDNRender) | `core/interface.h/.cpp` | ~800 |

### 9.3 media/ → media/

| Python | C++ | Líneas est. |
|--------|-----|-------------|
| `optimizer.py` (Quantizer, Optimise) | `media/optimizer.h/.cpp` | ~700 |
| `palette.py` (PaletteEntry, Palette) | `media/palette.h/.cpp` | ~300 |
| `pgraphics.py` (PGraphics, PGDecoder) | `media/pgraphics.h/.cpp` | ~400 |
| `pgstream.py` (EpochContext, compliance) | `media/pgstream.h/.cpp` | ~500 |

### 9.4 Entry Points

| Python | C++ |
|--------|-----|
| `cli.py` | `cli/main.cpp` (~300 líneas) |
| `gui.py` | `gui/main_window.cpp` + tools (~2,000 líneas) |

---

## 10. Estimación de Esfuerzo

| Fase | Descripción | Líneas C++ est. | Semanas |
|------|-------------|------------------|---------|
| 1 | Fundación + Reorganización | ~800 | 1 |
| 2 | Serialización + I/O | ~1,500 | 1 |
| 3 | Core Engine | ~3,000 | 2 |
| 4 | CLI | ~300 | 1 |
| 5 | GUI Qt6 | ~2,000 | 3 |
| 6 | Empaquetado | ~200 | 1 |
| **Total** | | **~7,800** | **~9** |

### Orden de Migración por Complejidad

```
Trivial (< 100 líneas)     → Fase 1
  ├── geometry.py → geometry.cpp
  ├── timecode.py → timecode.cpp
  ├── ssim.py → ssim.cpp
  ├── color_matrix.py → color_matrix.cpp
  └── bdvideo.py → bdvideo.cpp

Mecánico (serialización)    → Fase 1-2
  └── segments.py → segments.cpp (~1100 líneas, pero es struct packing)

Moderado                    → Fase 2-3
  ├── palette.py → palette.cpp
  ├── filestreams.py → filestreams.cpp
  ├── pgraphics.py → pgraphics.cpp
  ├── pgstream.py → pgstream.cpp
  └── optimizer.py → optimizer.cpp

Complejo (algoritmos)       → Fase 3
  └── renderer.py → renderer.cpp (1609 líneas, FSM + timing model)

Orquestación                → Fase 3-4
  └── interface.py → interface.cpp
```

---

## Estado del Proyecto

| Fase | Estado |
|------|--------|
| Fase | Estado |
|------|--------|
| Fase 1: Fundación | ✅ Completada |
| Fase 2: Serialización + I/O | ✅ Completada |
| Fase 3: Core Engine | ✅ Completada |
| Fase 4: CLI | ✅ Completada |
| Fase 5: GUI Qt6 | ✅ Completada |
| Fase 5.1: Refinamiento (Log/Resolución/Extensión) | ✅ Completada |
| Fase 5.2: Bugfixes Pipeline (9 bugs) | ✅ Completada |
| Fase 6: Empaquetado | 🔴 No iniciada |

---

*Última actualización: Jul 21, 2026*
