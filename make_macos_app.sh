#!/bin/bash
# Package GoldenRecomp as a double-clickable macOS app bundle.
# Run from the kit root after a successful build: ./make_macos_app.sh [binary] [out-dir]
# Produces "GoldenEye 007 Recompiled.app" in out-dir, the kit root by default (drag it to /Applications if you like).
#
# If dylibbundler is installed (brew install dylibbundler), the Homebrew libraries the game uses (SDL2, the SDL3
# that Homebrew's sdl2-compat loads, FreeType) are copied into the app, so it runs on a Mac without Homebrew.
# Without it the app still works, but only where those Homebrew libraries are installed.
set -e
cd "$(dirname "$0")"

BIN=${1:-build/GoldenRecomp}   # pass build-clean/GoldenRecomp for the no-game-code variant
OUT=${2:-.}
APP="$OUT/GoldenEye 007 Recompiled.app"

if [ ! -f "$BIN" ]; then
    echo "ERROR: $BIN not found — build first (./build_macos.sh or ninja -C build GoldenRecomp)"
    exit 1
fi

# The version shown in Finder: APP_VERSION if set (CI), else the newest v* tag (v1.2.0 -> 1.2.0). macOS wants numbers only.
VERSION=${APP_VERSION:-$(git describe --tags --match '[vV][0-9]*' 2>/dev/null)}
VERSION=$(printf '%s' "$VERSION" | sed -e 's/^[vV]//' -e 's/[^0-9.].*//')
VERSION=${VERSION:-1.0}

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" "$APP/Contents/Frameworks"

# --- Info.plist ---------------------------------------------------------------
cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>              <string>GoldenEye 007 Recompiled</string>
    <key>CFBundleDisplayName</key>       <string>GoldenEye 007 Recompiled</string>
    <key>CFBundleIdentifier</key>        <string>io.github.goldenrecomp</string>
    <key>CFBundleVersion</key>           <string>${VERSION}</string>
    <key>CFBundleShortVersionString</key><string>${VERSION}</string>
    <key>CFBundlePackageType</key>       <string>APPL</string>
    <key>CFBundleExecutable</key>        <string>launch</string>
    <key>CFBundleIconFile</key>          <string>AppIcon</string>
    <key>NSHighResolutionCapable</key>   <true/>
    <key>LSMinimumSystemVersion</key>    <string>13.0</string>
    <key>LSApplicationCategoryType</key> <string>public.app-category.games</string>
</dict>
</plist>
EOF

# --- Launcher stub (sets cwd so runtime files resolve) ------------------------
cat > "$APP/Contents/MacOS/launch" <<'EOF'
#!/bin/bash
DIR="$(cd "$(dirname "$0")/../Resources" && pwd)"
cd "$DIR"
exec "$DIR/GoldenRecomp" "$@"
EOF
chmod +x "$APP/Contents/MacOS/launch"

# --- Payload ------------------------------------------------------------------
cp "$BIN" "$APP/Contents/Resources/GoldenRecomp"
cp gamecontrollerdb.txt "$APP/Contents/Resources/" 2>/dev/null || true
cp -R assets "$APP/Contents/Resources/assets"

# --- Icon (built with Apple's own tools) --------------------------------------
if command -v sips >/dev/null 2>&1 && command -v iconutil >/dev/null 2>&1; then
    ICONSET=$(mktemp -d)/AppIcon.iconset
    mkdir -p "$ICONSET"
    for sz in 16 32 64 128 256 512; do
        sips -z $sz $sz icons/512.png --out "$ICONSET/icon_${sz}x${sz}.png" >/dev/null
        dbl=$((sz*2))
        if [ $dbl -le 1024 ]; then
            sips -z $dbl $dbl icons/512.png --out "$ICONSET/icon_${sz}x${sz}@2x.png" >/dev/null
        fi
    done
    iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/AppIcon.icns"
    rm -rf "$(dirname "$ICONSET")"
    echo "Icon created."
else
    echo "NOTE: sips/iconutil not found; app will use a generic icon."
fi

# --- Homebrew libraries into Frameworks -----------------------------------------
# The binary lives in Resources, so @executable_path/../Frameworks is Contents/Frameworks.
if command -v dylibbundler >/dev/null 2>&1; then
    GAME="$APP/Contents/Resources/GoldenRecomp"
    FW="$APP/Contents/Frameworks"
    dylibbundler -of -b -x "$GAME" -d "$FW" -p @executable_path/../Frameworks/ >/dev/null
    # Homebrew's sdl2 is sdl2-compat, which dlopens SDL3 from its own folder at run time, so dylibbundler
    # can't see it: add it by hand.
    SDL3=$(brew --prefix sdl3 2>/dev/null)/lib/libSDL3.0.dylib
    if [ -f "$SDL3" ]; then
        cp "$SDL3" "$FW/libSDL3.dylib"
        chmod u+w "$FW/libSDL3.dylib"
        install_name_tool -id @executable_path/../Frameworks/libSDL3.dylib "$FW/libSDL3.dylib" 2>/dev/null
        dylibbundler -of -b -x "$FW/libSDL3.dylib" -d "$FW" -p @executable_path/../Frameworks/ >/dev/null
    fi
    for f in "$GAME" "$FW"/*.dylib; do
        if otool -L "$f" | tail -n +2 | grep -qE '/opt/homebrew|/usr/local'; then
            echo "ERROR: $f still links to Homebrew:"
            otool -L "$f" | grep -E '/opt/homebrew|/usr/local'
            exit 1
        fi
    done
    echo "Bundled $(ls "$FW" | wc -l | tr -d ' ') libraries."
else
    rmdir "$APP/Contents/Frameworks"
    echo "NOTE: dylibbundler not found; the app needs Homebrew's sdl2 and freetype to run."
fi

# --- No ROM may end up in the app -------------------------------------------------
while IFS= read -r f; do
    if [ "$(head -c 4 "$f" | od -An -tx1 | tr -d ' \n')" = "80371240" ]; then
        echo "ERROR: $f is an N64 ROM"
        exit 1
    fi
done < <(find "$APP" -type f)

# --- Ad hoc signature (install_name_tool invalidates the linker's one) -----------
if command -v codesign >/dev/null 2>&1; then
    for f in "$APP"/Contents/Frameworks/*.dylib; do
        [ -f "$f" ] && codesign --force --sign - "$f"
    done
    codesign --force --sign - "$APP/Contents/Resources/GoldenRecomp"
    codesign --force --sign - "$APP"
    codesign --verify --strict "$APP"
fi

echo ""
echo "Done: \"$APP\""
echo "Double-click it in Finder, or:  open \"$APP\""
echo "Config and the stored ROM now live in ~/Library/Application Support/GoldenRecomp"
echo "(first launch will ask for your ROM again — your retail .z64 works directly)."
