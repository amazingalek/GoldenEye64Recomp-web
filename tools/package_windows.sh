#!/bin/bash
# Package a Windows build as GoldenEye64Recomp/ (exe, DLLs, assets, controller db) in out-dir.
# Usage (Git Bash): tools/package_windows.sh [build-dir] [out-dir]
#   build-dir  defaults to build-clean, the clean build (build_windows.sh --clean)
#   out-dir    defaults to package
# The game loads assets/ and gamecontrollerdb.txt from the working directory, which is the exe's folder when it's
# started from Explorer. The Visual C++ runtime DLLs are copied next to the exe (app-local), found through vswhere,
# so the package runs on a Windows without the VC++ redistributable installed.
set -e
cd "$(dirname "$0")/.."

BUILD=${1:-build-clean}
OUT=${2:-package}
DIR="$OUT/GoldenEye64Recomp"

[ -f "$BUILD/GoldenRecomp.exe" ] || { echo "ERROR: $BUILD/GoldenRecomp.exe not found, build first"; exit 1; }
[ -f gamecontrollerdb.txt ] || { echo "ERROR: gamecontrollerdb.txt not found, the CMake configure step downloads it"; exit 1; }

VSWHERE="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
[ -f "$VSWHERE" ] || { echo "ERROR: vswhere.exe not found, is Visual Studio (or its Build Tools) installed?"; exit 1; }
VS=$("$VSWHERE" -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | tr -d '\r')
CRT=$(ls -d "$(cygpath -u "$VS")"/VC/Redist/MSVC/*/x64/Microsoft.VC*.CRT 2>/dev/null | sort -V | tail -1)
[ -n "$CRT" ] || { echo "ERROR: no Visual C++ runtime redistributable under $VS/VC/Redist"; exit 1; }

rm -rf "$DIR"
mkdir -p "$DIR"
cp "$BUILD/GoldenRecomp.exe" "$BUILD/SDL2.dll" "$BUILD/dxil.dll" "$BUILD/dxcompiler.dll" "$DIR/"
cp "$CRT"/*.dll "$DIR/"
cp -R assets "$DIR/assets"
cp gamecontrollerdb.txt "$DIR/"

cat > "$DIR/README.txt" <<'EOF'
GoldenEye 007: Recompiled, Windows x64

This contains no game code and no ROM. On first launch, pick your own retail
NTSC-U GoldenEye 007 dump (.z64, .v64 or .n64); the game is recompiled from it
in memory.

Run GoldenRecomp.exe. Needs Windows 10 or 11 (x64) and a GPU with Direct3D 12
or Vulkan.

Settings and the stored ROM live in %LOCALAPPDATA%\GoldenRecomp.

Source: https://github.com/podulator/GoldenEye64Recomp
EOF
# Notepad-friendly line endings.
sed -i 's/$/\r/' "$DIR/README.txt"

while IFS= read -r f; do
    if [ "$(head -c 4 "$f" | od -An -tx1 | tr -d ' \n')" = "80371240" ]; then
        echo "ERROR: $f is an N64 ROM"
        exit 1
    fi
done < <(find "$DIR" -type f)

echo "Packaged $DIR (VC++ runtime from $CRT)"
