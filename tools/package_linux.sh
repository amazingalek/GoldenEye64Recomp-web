#!/bin/bash
# Package a Linux build as GoldenEye64Recomp/ (binary, assets, controller db, launcher) in out-dir.
# Usage: tools/package_linux.sh [binary] [out-dir]
#   binary   defaults to build-sniper-clean/GoldenRecomp, the Steam Deck clean build (build_steamdeck.sh --clean)
#   out-dir  defaults to package
# The game loads assets/ and gamecontrollerdb.txt from the working directory, so the launcher changes into its own
# folder first. Built in the sniper runtime, the binary needs only glibc 2.31: it runs on the Steam Deck and on
# most desktop distros.
set -e
cd "$(dirname "$0")/.."

BIN=${1:-build-sniper-clean/GoldenRecomp}
OUT=${2:-package}
DIR="$OUT/GoldenEye64Recomp"

[ -f "$BIN" ] || { echo "ERROR: $BIN not found, build first"; exit 1; }
[ -f gamecontrollerdb.txt ] || { echo "ERROR: gamecontrollerdb.txt not found, the CMake configure step downloads it"; exit 1; }

rm -rf "$DIR"
mkdir -p "$DIR"
cp "$BIN" "$DIR/GoldenRecomp"
strip "$DIR/GoldenRecomp"
cp -R assets "$DIR/assets"
cp gamecontrollerdb.txt "$DIR/"
cp icons/512.png "$DIR/icon.png"

cat > "$DIR/GoldenEye64Recomp.sh" <<'EOF'
#!/bin/sh
cd "$(dirname "$0")" && exec ./GoldenRecomp "$@"
EOF
chmod +x "$DIR/GoldenEye64Recomp.sh" "$DIR/GoldenRecomp"

cat > "$DIR/README.txt" <<'EOF'
GoldenEye 007: Recompiled, Linux x86_64 / Steam Deck

This contains no game code and no ROM. On first launch, pick your own retail
NTSC-U GoldenEye 007 dump (.z64, .v64 or .n64); the game is recompiled from it
in memory.

Desktop Linux: run ./GoldenEye64Recomp.sh. Needs a Vulkan driver, SDL2, GTK 3
and FreeType, which nearly every desktop distro has.

Steam Deck: in desktop mode, add GoldenEye64Recomp.sh to Steam as a non-Steam
game. In its Properties > Compatibility, tick "Force the use of a specific
Steam Play compatibility tool" and pick "Steam Linux Runtime 3.0 (sniper)".

Settings and the stored ROM live in ~/.config/GoldenRecomp.

Source: https://github.com/podulator/GoldenEye64Recomp
EOF

while IFS= read -r f; do
    if [ "$(head -c 4 "$f" | od -An -tx1 | tr -d ' \n')" = "80371240" ]; then
        echo "ERROR: $f is an N64 ROM"
        exit 1
    fi
done < <(find "$DIR" -type f)

echo "Packaged $DIR (needs glibc $(objdump -T "$DIR/GoldenRecomp" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1 | sed 's/GLIBC_//') or newer)"
