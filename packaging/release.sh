#!/usr/bin/env bash
# Release build: compila engine + GUI y arma los paquetes finales en builds/.
#
# Uso: packaging/release.sh [windows|linux|all]   (default: all)
#
# Requisitos: cmake, mingw-w64 (para windows), rust target x86_64-pc-windows-gnu,
#             wails, go, nfpm (~/go/bin/nfpm), python3+PIL (iconos).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

TARGET="${1:-all}"
VERSION="$(grep -m1 '  VERSION ' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')"
NFPM="${NFPM:-$HOME/go/bin/nfpm}"
WIN_CMAKE="builds/.cmake-windows"

echo "== OpenSUP $VERSION =="

build_engine_linux() {
    echo "-- engine linux --"
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF >/dev/null
    cmake --build build --target opensup_cli -j"$(nproc)" >/dev/null
    cp build/src/opensup/cli/opensup_cli ui/engine/bin/linux_amd64/opensup_engine
}

build_engine_windows() {
    echo "-- engine windows (cross mingw) --"
    cmake -S . -B "$WIN_CMAKE" \
        -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF >/dev/null
    cmake --build "$WIN_CMAKE" --target opensup_cli -j"$(nproc)" >/dev/null
    cp "$WIN_CMAKE/src/opensup/cli/opensup_cli.exe" ui/engine/bin/windows_amd64/opensup_engine.exe
}

gen_icons() {
    echo "-- iconos --"
    mkdir -p packaging/icons
    python3 - "$ROOT/ui/build/appicon.png" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGBA")
for s in (32, 64, 128, 256, 512):
    im.resize((s, s), Image.LANCZOS).save(f"packaging/icons/opensup-{s}.png")
PY
}

package_linux() {
    echo "-- linux: gui --"
    rm -rf builds/linux
    mkdir -p builds/linux
    (cd ui && wails build -platform linux/amd64 -clean >/dev/null)
    cp ui/build/bin/OpenSUP builds/linux/OpenSUP
    gen_icons
    echo "-- linux: paquetes --"
    "$NFPM" package -f packaging/nfpm.yaml -p deb -t builds/linux/
    "$NFPM" package -f packaging/nfpm.yaml -p rpm -t builds/linux/
    cp LICENSE builds/linux/LICENSE
}

package_windows() {
    echo "-- windows: gui --"
    rm -rf builds/windows
    mkdir -p builds/windows
    (cd ui && wails build -platform windows/amd64 -clean >/dev/null)
    cp ui/build/bin/OpenSUP.exe builds/windows/OpenSUP.exe
    (cd builds/windows && zip -q "OpenSUP-${VERSION}-windows-amd64.zip" OpenSUP.exe)
    cp LICENSE builds/windows/LICENSE
}

case "$TARGET" in
    windows) build_engine_windows && package_windows ;;
    linux)   build_engine_linux   && package_linux ;;
    all)     build_engine_windows && package_windows && build_engine_linux && package_linux ;;
    *) echo "uso: $0 [windows|linux|all]" >&2; exit 1 ;;
esac

echo "-- listos --"
ls -la builds/windows builds/linux
