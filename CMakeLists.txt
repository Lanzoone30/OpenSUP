cmake_minimum_required(VERSION 3.16)

project(OpenSUP
  VERSION 1.0.0
  DESCRIPTION "PGS subtitle encoder for Blu-ray"
  LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/third_party")

if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND CMAKE_PREFIX_PATH "/usr/x86_64-w64-mingw32/sys-root/mingw")
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(CompilerWarnings)
include(GNUInstallDirs)

option(BUILD_TESTS "Build unit tests" ON)
option(OPENSUP_USE_EXTERNAL_LIBIMAGEQUANT "Use system-installed libimagequant" ON)

# ── libimagequant: cross-platform quantizer ──
set(LIBIMAGEQUANT_INCLUDE_DIRS "" CACHE STRING "libimagequant include path")
set(LIBIMAGEQUANT_LIBRARIES "" CACHE STRING "libimagequant library path")

if(OPENSUP_USE_EXTERNAL_LIBIMAGEQUANT)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND AND NOT (CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows"))
        pkg_check_modules(LIBIMAGEQUANT QUIET imagequant)
    endif()
    if(NOT LIBIMAGEQUANT_FOUND)
        find_library(LIBIMAGEQUANT_LIBRARIES imagequant)
        find_path(LIBIMAGEQUANT_INCLUDE_DIRS libimagequant.h)
    endif()
endif()

if(LIBIMAGEQUANT_FOUND OR LIBIMAGEQUANT_LIBRARIES)
    # Find the actual library file for proper linking
    find_library(LIBIMAGEQUANT_SO imagequant)
    if(LIBIMAGEQUANT_SO)
        add_library(imagequant UNKNOWN IMPORTED GLOBAL)
        set_target_properties(imagequant PROPERTIES
            IMPORTED_LOCATION "${LIBIMAGEQUANT_SO}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBIMAGEQUANT_INCLUDE_DIRS}"
        )
        message(STATUS "libimagequant: found at ${LIBIMAGEQUANT_SO}")
    else()
        add_library(imagequant INTERFACE)
        target_link_libraries(imagequant INTERFACE ${LIBIMAGEQUANT_LDFLAGS})
        target_include_directories(imagequant INTERFACE ${LIBIMAGEQUANT_INCLUDE_DIRS})
        message(STATUS "libimagequant: using linker flags ${LIBIMAGEQUANT_LDFLAGS}")
    endif()
else()
    message(STATUS "libimagequant: building from source via Cargo")
    include(FetchContent)

    # Determine libimagequant source directory (vendored or downloaded)
    set(LIBIMAGEQUANT_SRC_DIR "${CMAKE_SOURCE_DIR}/extern/libimagequant")
    if(EXISTS "${LIBIMAGEQUANT_SRC_DIR}/Cargo.toml")
        message(STATUS "libimagequant: found vendored source at ${LIBIMAGEQUANT_SRC_DIR}")
    else()
        message(STATUS "libimagequant: downloading source...")
        FetchContent_Declare(libimagequant_src
            GIT_REPOSITORY https://github.com/imageoptim/libimagequant.git
            GIT_TAG 4.3.1
            SOURCE_DIR "${LIBIMAGEQUANT_SRC_DIR}"
        )
        FetchContent_MakeAvailable(libimagequant_src)
    endif()

    # Build libimagequant from source using Cargo
    set(LIBIMAGEQUANT_BUILD_DIR "${CMAKE_BINARY_DIR}/libimagequant-build")

    if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(LIBIMAGEQUANT_CARGO_TARGET "--target" "x86_64-pc-windows-gnu")
        set(LIBIMAGEQUANT_STATIC_LIB "${LIBIMAGEQUANT_BUILD_DIR}/x86_64-pc-windows-gnu/release/libimagequant_sys.a")
    else()
        set(LIBIMAGEQUANT_CARGO_TARGET "")
        set(LIBIMAGEQUANT_STATIC_LIB "${LIBIMAGEQUANT_BUILD_DIR}/release/libimagequant_sys.a")
    endif()

    file(MAKE_DIRECTORY "${LIBIMAGEQUANT_BUILD_DIR}")

    add_custom_command(
        OUTPUT "${LIBIMAGEQUANT_STATIC_LIB}"
        # ponytail: --no-default-features disables rayon threading which crashes
        # on Windows (MinGW-w64 + Rust runtime incompatibility)
        COMMAND cargo build --release --lib --no-default-features ${LIBIMAGEQUANT_CARGO_TARGET} --target-dir="${LIBIMAGEQUANT_BUILD_DIR}"
        WORKING_DIRECTORY "${LIBIMAGEQUANT_SRC_DIR}/imagequant-sys"
        COMMENT "Building libimagequant (cargo build --release)..."
    )

    add_custom_target(libimagequant_build
        DEPENDS "${LIBIMAGEQUANT_STATIC_LIB}"
    )

    add_library(imagequant STATIC IMPORTED GLOBAL)
    set_target_properties(imagequant PROPERTIES
        IMPORTED_LOCATION "${LIBIMAGEQUANT_STATIC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBIMAGEQUANT_SRC_DIR}/imagequant-sys"
    )
    add_dependencies(imagequant libimagequant_build)

    if(UNIX AND NOT APPLE)
        set_property(TARGET imagequant APPEND PROPERTY INTERFACE_LINK_LIBRARIES pthread dl)
    endif()

    if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set_property(TARGET imagequant APPEND PROPERTY INTERFACE_LINK_LIBRARIES ws2_32 ntdll userenv iphlpapi bcrypt)
    endif()
endif()

# ── pugixml: XML parsing (HEADER-ONLY para compilación cruzada) ──
include(FetchContent)
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG v1.14
)
FetchContent_GetProperties(pugixml)
if(NOT pugixml_POPULATED)
    FetchContent_Populate(pugixml)
    # ponytail: EXCLUDE_FROM_ALL evita que pugixml instale sus .hpp/.a/cmake en el package
    add_subdirectory(${pugixml_SOURCE_DIR} ${pugixml_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
set(PUGIXML_INCLUDE_DIRS "${pugixml_SOURCE_DIR}/src")
set(PUGIXML_LIBRARIES pugixml)

# ── CLI11: header-only CLI parsing ──
include(FetchContent)
FetchContent_Declare(cli11_proj QUIET
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.6.2
)
FetchContent_MakeAvailable(cli11_proj)

add_subdirectory(src/opensup/common)
add_subdirectory(src/opensup/media)
add_subdirectory(src/opensup/core)
add_subdirectory(src/opensup/cli)

option(BUILD_GUI "Build Qt6 GUI" ON)
if(BUILD_GUI)
  add_subdirectory(src/opensup/gui)
endif()

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ── Windows: install + CPack ──
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # GUI .exe
    install(TARGETS OpenSUP
        RUNTIME DESTINATION .
    )

    # CLI .exe
    install(TARGETS opensup_cli
        RUNTIME DESTINATION .
    )

    # ── DLLs (Qt6 + MinGW runtime + transitive deps) ──
    set(MINGW_BIN "/usr/x86_64-w64-mingw32/sys-root/mingw/bin")
    set(MINGW_PLUGINS "/usr/x86_64-w64-mingw32/sys-root/mingw/lib/qt6/plugins")

    # Qt6
    install(FILES
        ${MINGW_BIN}/Qt6Core.dll
        ${MINGW_BIN}/Qt6Gui.dll
        ${MINGW_BIN}/Qt6Widgets.dll
        DESTINATION .
    )

    # MinGW runtime
    install(FILES
        ${MINGW_BIN}/libgcc_s_seh-1.dll
        ${MINGW_BIN}/libstdc++-6.dll
        ${MINGW_BIN}/libwinpthread-1.dll
        DESTINATION .
    )

    # Qt6Core transitive deps
    install(FILES
        ${MINGW_BIN}/libpcre2-16-0.dll
        ${MINGW_BIN}/zlib1.dll
        ${MINGW_BIN}/icui18n76.dll
        ${MINGW_BIN}/icuuc76.dll
        ${MINGW_BIN}/icudata76.dll
        DESTINATION .
    )

    # Qt6Gui transitive deps
    install(FILES
        ${MINGW_BIN}/libfontconfig-1.dll
        ${MINGW_BIN}/libfreetype-6.dll
        ${MINGW_BIN}/libharfbuzz-0.dll
        ${MINGW_BIN}/libpng16-16.dll
        ${MINGW_BIN}/libbz2-1.dll
        ${MINGW_BIN}/libexpat-1.dll
        ${MINGW_BIN}/libglib-2.0-0.dll
        ${MINGW_BIN}/libintl-8.dll
        ${MINGW_BIN}/iconv.dll
        ${MINGW_BIN}/libpcre2-8-0.dll
        DESTINATION .
    )

    # Platform plugin (required for Qt GUI on Windows)
    install(FILES
        ${MINGW_PLUGINS}/platforms/qwindows.dll
        DESTINATION platforms/
    )

    # ── CPack: NSIS installer + ZIP fallback ──
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_PACKAGE_NAME "OpenSUP")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_VENDOR "OpenSUP")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PGS subtitle encoder for Blu-ray")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "OpenSUP")
    set(CPACK_NSIS_DISPLAY_NAME "OpenSUP ${PROJECT_VERSION}")
    set(CPACK_NSIS_HELP_LINK "https://github.com/lanzoone30/OpenSUP")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/lanzoone30/OpenSUP")
    set(CPACK_NSIS_MODIFY_PATH OFF)

    # Start Menu shortcut
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\OpenSUP.lnk' '$INSTDIR\\\\OpenSUP.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\OpenSUP.lnk'"
    )

    include(CPack)
endif()