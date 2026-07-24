#!/bin/bash
# build_windows.sh — Cross-compile OpenSUP CLI + GUI for Windows 64-bit
# Requires: mingw64-gcc-c++, mingw64-qt6-qtbase, cargo (with x86_64-pc-windows-gnu target)
set -euo pipefail

BUILD_DIR="${1:-build-mingw}"
JOBS=$(nproc 2>/dev/null || echo 4)

MINGW_ROOT="/usr/x86_64-w64-mingw32/sys-root/mingw"

echo "=== OpenSUP Windows cross-compile ==="
echo "  Build dir: ${BUILD_DIR}"
echo "  Jobs:      ${JOBS}"
echo ""

# Configure (build both CLI and GUI)
cmake -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE=mingw-w64-x86_64.cmake \
    -DBUILD_GUI=ON \
    -DBUILD_TESTS=OFF \
    -DOPENSUP_USE_EXTERNAL_LIBIMAGEQUANT=OFF \
    -DWARNINGS_AS_ERRORS=OFF \
    -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build "${BUILD_DIR}" -j"${JOBS}"

echo ""
echo "=== Deploying Qt6 DLLs for GUI ==="

GUI_DIR="${BUILD_DIR}/src/opensup/gui"
PLUGINS_DIR="${GUI_DIR}/platforms"

mkdir -p "${PLUGINS_DIR}"

# Copy Qt6 DLLs
cp -v "${MINGW_ROOT}/bin/Qt6Core.dll"       "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/Qt6Gui.dll"        "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/Qt6Widgets.dll"    "${GUI_DIR}/"

# Copy platform plugin (required for Windows GUI)
cp -v "${MINGW_ROOT}/lib/qt6/plugins/platforms/qwindows.dll" "${PLUGINS_DIR}/"

# Copy MinGW runtime DLLs
cp -v "${MINGW_ROOT}/bin/libgcc_s_seh-1.dll"   "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/libstdc++-6.dll"      "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/libwinpthread-1.dll"  "${GUI_DIR}/"

# Copy Qt6Core transitive dependencies
cp -v "${MINGW_ROOT}/bin/libpcre2-16-0.dll"  "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/zlib1.dll"           "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/icui18n76.dll"      "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/icuuc76.dll"        "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/icudata76.dll"      "${GUI_DIR}/"

# Copy Qt6Gui transitive dependencies
cp -v "${MINGW_ROOT}/bin/libfontconfig-1.dll"  "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/libfreetype-6.dll"    "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/libharfbuzz-0.dll"    "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/libpng16-16.dll"      "${GUI_DIR}/"

# Copy fontconfig transitive dependencies
cp -v "${MINGW_ROOT}/bin/libexpat-1.dll"     "${GUI_DIR}/"

# Copy freetype transitive dependencies
cp -v "${MINGW_ROOT}/bin/libbz2-1.dll"       "${GUI_DIR}/"

# Copy harfbuzz transitive dependencies
cp -v "${MINGW_ROOT}/bin/libglib-2.0-0.dll"  "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/libpcre2-8-0.dll"   "${GUI_DIR}/"

# Copy glib transitive dependencies
cp -v "${MINGW_ROOT}/bin/libintl-8.dll"      "${GUI_DIR}/"
cp -v "${MINGW_ROOT}/bin/iconv.dll"          "${GUI_DIR}/"

echo ""
echo "=== Done ==="
CLI_EXE="${BUILD_DIR}/src/opensup/cli/opensup.exe"
GUI_EXE="${BUILD_DIR}/src/opensup/gui/opensup_gui.exe"

if [ -f "${CLI_EXE}" ] && [ -f "${GUI_EXE}" ]; then
    echo "✅ CLI: ${CLI_EXE}"
    ls -lh "${CLI_EXE}"
    echo "✅ GUI: ${GUI_EXE}"
    ls -lh "${GUI_EXE}"
    echo "✅ Qt6 DLLs deployed to: ${GUI_DIR}/"
    ls -lh "${GUI_DIR}"/*.dll
    echo "✅ Platform plugins: ${PLUGINS_DIR}/"
    ls -lh "${PLUGINS_DIR}"/*.dll
else
    echo "❌ Build failed — binaries not found"
    exit 1
fi
