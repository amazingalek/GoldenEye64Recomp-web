# Building

The game's code is recompiled at build time from the TLBFREE ROM, which you make from your own NTSC-U dump. The
program, native or the page, then takes the retail dump itself from its launcher's Load ROM and converts it in
memory (`src/main/rom_conversion.cpp`).

## 1. Clone with the submodules

```bash
git clone --recurse-submodules https://github.com/n64hub/GoldenEye64Recomp-web
```

`lib/` holds RT64, N64ModernRuntime (with N64Recomp) and RecompFrontend from their upstream repositories, at the
commits [N64Recomp-browser](https://github.com/n64hub/N64Recomp-browser)'s web forks are built on (GoldenEye64Recomp
itself predates RecompFrontend, so it pins none of its own); N64Recomp-browser, whose `patches/` puts the web support
and the recompiler changes this game needs into their checkouts and whose `frontend/` makes the page; and the
GoldenEye decompilation (`lib/ge`, whose headers the patches use).

## 2. Tools

- CMake 3.20+, Ninja, a clang (clang-cl on Windows) for the program.
- A clang with a MIPS target and `ld.lld`, plus `make`, for the patches (`patches/`). On Windows llvm.org's clang
  has no MIPS target: MSYS2's clang64 does, with llvm.org's `ld.lld` (MSYS2's rejects `--emit-relocs`).
- `xdelta3`, to make the TLBFREE ROM.
- For the page: emsdk (`emsdk install latest`) and [naga](https://github.com/gfx-rs/wgpu/tree/trunk/naga)
  (`cargo install naga-cli`).

## 3. The TLBFREE ROM and the recompiled code

```bash
xdelta3 -d -s baserom.u.z64 vanilla_to_tlbfree.xdelta ge007.tlbfree.z64   # your dump as baserom.u.z64

# N64Recomp-browser's patches in lib/rt64, lib/N64ModernRuntime and lib/RecompFrontend
cmake -P lib/N64Recomp-browser/patches/apply.cmake lib

# N64Recomp and RSPRecomp, from the patched N64Recomp in lib/
cmake -S lib/N64ModernRuntime/N64Recomp -B build-n64recomp -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-n64recomp --target N64RecompCLI RSPRecomp
cp build-n64recomp/N64Recomp build-n64recomp/RSPRecomp .                  # .exe on Windows

./N64Recomp us.toml            # RecompiledFuncs/
./RSPRecomp aspMain.us.toml    # rsp/aspMain.cpp
```

The native program needs the patches as much as the page does: the patched N64Recomp reads GoldenEye's jump tables
from the `.rodata` section that holds them and renames its own `strtol` and the rest in the patches' calls, and the
code it writes uses the patched `recomp.h`. The web support in them is behind `__EMSCRIPTEN__` and changes nothing
natively. Configuring the program (step 4, or the page) applies them too if they aren't in yet, and does nothing if
they are; `git apply --reverse lib/N64Recomp-browser/patches/<name>.patch` in a checkout takes one out.

The build recompiles the patches itself (`./N64Recomp patches.toml`, after `make` in `patches/`).

## 4. Native

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DPATCHES_C_COMPILER=<clang with a MIPS target> -DPATCHES_LD=<ld.lld>    # on Windows also -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build --target GoldenRecomp
```

Run it from the repository's root (it reads `assets/`). A `portable.txt` there keeps settings, saves and the stored
ROM beside it. `--boot` skips the launcher once a ROM is stored.

## The web page

With the patches built natively first (step 4's `patches/patches.bin` and `RecompiledPatches/`; emsdk's clang has no
MIPS target):

```bash
embuilder build sdl2-mt freetype-legacysjlj      # once
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release -DRT64_NAGA_EXECUTABLE=<naga>
cmake --build build-web --target GoldenRecomp
```

`build-web/GoldenRecomp.html` must be served with `Cross-Origin-Opener-Policy: same-origin` and
`Cross-Origin-Embedder-Policy: require-corp` (threads need SharedArrayBuffer), or with `coi_serviceworker.js` beside
it where headers can't be set. Open it in a browser with WebGPU and pick your retail dump with Load ROM: it is kept
in the browser's storage, with settings and saves.

`tools/play_plan.json` plays the page as a player would, for N64Recomp-browser's `tools/play_page.mjs`, in 2-3
minutes: the boot, then Dam, started by `tools/shader_survey.js` as mission select does, and about 35 s of Bond
walking, turning, strafing and firing; then Facility in the same load, started from Dam and played about 25 s.
`tools/play_plan_extended.json`, for when a longer play is needed, goes on to Surface 1 and Frigate that way, in at
most 5 minutes (their headers say more).
