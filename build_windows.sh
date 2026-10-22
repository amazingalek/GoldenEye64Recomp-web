#!/bin/bash
# GoldenEye 007: Recompiled — Windows build script (run it from Git Bash or an MSYS2 shell)
# Usage:
#   ./build_windows.sh          # standard build (game code compiled in at build time; needs ROM present)
#   ./build_windows.sh --clean  # clean build: no game code; recompiled from the user's ROM at launch
#
# The program builds with LLVM's clang-cl against Visual Studio's C++ libraries and the Windows SDK.
# The gameplay patches (patches/) are MIPS code: they need a clang with a MIPS target, which LLVM's
# Windows release lacks and MSYS2's clang64 clang has, linked with LLVM's ld.lld (MSYS2's rejects
# --emit-relocs). Point MIPS_CC / MIPS_LD at them if they aren't the clang / ld.lld on PATH.
set -e
cd "$(dirname "$0")"

BUILD_DIR="build"
MODE_FLAG=""
if [ "$1" = "--clean" ]; then
    MODE_FLAG="-DLIVE_GAMECODE=ON"
    BUILD_DIR="build-clean"
fi
MIPS_CC="${MIPS_CC:-clang}"
MIPS_LD="${MIPS_LD:-ld.lld}"
# LLVM's lld-link rather than Visual Studio's link.exe: in Git Bash, /usr/bin/link (coreutils) comes first on
# PATH, and CMake can pick that up as the linker.
WIN_LINKER="${WIN_LINKER:-lld-link}"
# CMake 4.x compat for old third-party cmake_minimum_required declarations (lunasvg and others).
export CMAKE_POLICY_VERSION_MINIMUM=3.5

echo "== [1/4] Checking dependencies"
MISSING=""
for tool in cmake ninja clang-cl make; do
    command -v $tool >/dev/null 2>&1 || MISSING="$MISSING $tool"
done
command -v "$MIPS_CC" >/dev/null 2>&1 || MISSING="$MISSING MIPS_CC($MIPS_CC)"
command -v "$MIPS_LD" >/dev/null 2>&1 || MISSING="$MISSING MIPS_LD($MIPS_LD)"
command -v "$WIN_LINKER" >/dev/null 2>&1 || MISSING="$MISSING WIN_LINKER($WIN_LINKER)"
if command -v py >/dev/null 2>&1; then
    PYTHON="py -3"
elif command -v python3 >/dev/null 2>&1; then
    PYTHON="python3"
else
    MISSING="$MISSING python3"
fi
if [ -n "$MISSING" ]; then
    echo "Missing tools:$MISSING"
    echo "Needed: Visual Studio 2022 or its Build Tools (C++ workload, for the MSVC libraries and"
    echo "the Windows SDK), LLVM (clang-cl, ld.lld), CMake, Ninja, Python 3, and from MSYS2's"
    echo "clang64 environment a clang with a MIPS target and make:"
    echo "    pacman -S mingw-w64-clang-x86_64-clang make"
    exit 1
fi
if ! "$MIPS_CC" --print-targets 2>/dev/null | grep -qw mips; then
    echo "$MIPS_CC has no MIPS target (LLVM's Windows release doesn't): set MIPS_CC to MSYS2's"
    echo "clang64 clang, e.g. MIPS_CC=/c/msys64/clang64/bin/clang.exe"
    exit 1
fi

echo "== [2/4] ROM preparation"
# The build needs the TLBFREE ROM to generate the game code. vanilla_to_tlbfree.gep1 is the
# patch the app itself embeds; applying it needs nothing but Python. A clean build only reads
# the ROM to translate the audio microcode, so once rsp/aspMain.cpp exists it needs no ROM.
NEED_ROM=1
if [ "$BUILD_DIR" = "build-clean" ] && [ -f rsp/aspMain.cpp ]; then
    NEED_ROM=0
fi
if [ "$NEED_ROM" = 1 ] && [ ! -f ge007.tlbfree.z64 ]; then
    if [ -f baserom.u.z64 ]; then
        $PYTHON tools_apply_gep1.py baserom.u.z64 vanilla_to_tlbfree.gep1 ge007.tlbfree.z64
    else
        echo "ERROR: place your NTSC-U GoldenEye ROM here as baserom.u.z64"
        exit 1
    fi
fi
if [ ! -f ./N64Recomp.exe ] || [ ! -f ./RSPRecomp.exe ]; then
    cmake -S n64recomp-src -B n64recomp-src/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl "-DCMAKE_LINKER=$WIN_LINKER" \
        "-DCMAKE_PROJECT_INCLUDE=$(cygpath -m "$PWD")/cmake/ccache_clang_cl.cmake" >/dev/null
    ninja -C n64recomp-src/build N64RecompCLI RSPRecomp
    cp n64recomp-src/build/N64Recomp.exe n64recomp-src/build/RSPRecomp.exe .
fi
if [ "$BUILD_DIR" = "build" ] && [ ! -f RecompiledFuncs/funcs_0.c ]; then
    echo "Generating recompiled game code from the ROM..."
    ./N64Recomp.exe us.toml
    $PYTHON tools_weaken_patched.py
fi
# The audio microcode (aspMain) is translated at build time too.
if [ ! -f rsp/aspMain.cpp ]; then
    ./RSPRecomp.exe aspMain.us.toml
fi

# The patches build needs the decompilation's headers (lib/ge submodule), and the program
# needs FreeType's Windows binaries (lib/freetype-windows-binaries submodule).
if [ ! -f lib/ge/include/PR/ultratypes.h ] || [ ! -f lib/freetype-windows-binaries/include/ft2build.h ]; then
    echo "Fetching submodules ..."
    git submodule update --init lib/ge lib/freetype-windows-binaries
fi

echo "== [3/4] Configuring"
# -Wno-error=...: rt64 builds with -Werror; newer clang adds these warnings, all hit only in its
#   bundled imgui/json (as in build_linux.sh). CXXFLAGS rather than -DCMAKE_CXX_FLAGS, which would
#   drop CMake's own clang-cl defaults (/EHsc and the rest).
export CXXFLAGS="${CXXFLAGS:-} -Wno-unknown-warning-option -Wno-error=nontrivial-memcall -Wno-error=deprecated-literal-operator -Wno-error=uninitialized-const-pointer"
cmake -S . -B $BUILD_DIR -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl "-DCMAKE_LINKER=$WIN_LINKER" \
    "-DPATCHES_C_COMPILER=$MIPS_CC" "-DPATCHES_LD=$MIPS_LD" $MODE_FLAG ${EXTRA_CMAKE_ARGS:-}

echo "== [4/4] Building"
ninja -C $BUILD_DIR GoldenRecomp

echo ""
echo "Done! Run with:  ./$BUILD_DIR/GoldenRecomp.exe   (from this folder: it reads assets/)"
echo "Pick your retail NTSC-U .z64 in the ROM picker — it converts automatically."
