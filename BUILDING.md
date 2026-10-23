# Building

The game's code is recompiled at build time from the TLBFREE ROM, which the build scripts make from your own NTSC-U
dump. The program, native or the page, then takes the retail dump itself from its launcher's Select ROM and converts
it in memory (`src/main/rom_conversion.cpp`).

## 1. Clone with the submodules

```bash
git clone --recurse-submodules https://github.com/n64hub/GoldenEye64Recomp-web
```

`lib/` holds RT64, N64ModernRuntime (with N64Recomp) and RecompFrontend from their upstream repositories, at the
commits [N64Recomp-browser](https://github.com/n64hub/N64Recomp-browser)'s web forks are built on; N64Recomp-browser,
whose `patches/` puts the web support and the recompiler changes this game needs into their checkouts and whose
`frontend/` makes the page; and the GoldenEye decompilation (`lib/ge`, whose headers the patches use).

## 2. Native

podulator's build scripts, as his README has them: put your dump in the repository's root as `baserom.u.z64` and run
`./build_windows.sh` (from Git Bash), `./build_linux.sh` or `./build_macos.sh`, or `./build_steamdeck.sh` for the
Steam Linux Runtime. `--clean` builds the variant that holds no game code and recompiles it from the ROM when it
starts. Each script says what it needs at its top.

Before they build N64Recomp, the scripts patch `lib/`:

```bash
cmake -P lib-patches/apply.cmake lib
```

That puts this game's changes to RT64, RecompFrontend and N64ModernRuntime (`lib-patches/`) into their checkouts,
then N64Recomp-browser's patches on top. The native program needs N64Recomp-browser's patches as much as the page
does: the patched N64Recomp reads GoldenEye's jump tables from the `.rodata` section that holds them and renames its
own `strtol` and the rest in the patches' calls, and the code it writes uses the patched `recomp.h`. The web support
in them is behind `__EMSCRIPTEN__` and changes nothing natively. Configuring the program applies them too if they
aren't in yet, and running it again does nothing; `lib-patches/apply.cmake` says how to take one out.

Run the program from the repository's root (it reads `assets/`). A `portable.txt` there keeps settings, saves and the
stored ROM beside it.

## The web page

With the native build done first (its `patches/patches.bin`, `RecompiledPatches/`, `RecompiledFuncs/` and
`rsp/aspMain.cpp`: emsdk's clang has no MIPS target), emsdk (`emsdk install latest`) and
[naga](https://github.com/gfx-rs/wgpu/tree/trunk/naga) (`cargo install naga-cli`):

```bash
embuilder build sdl2-mt freetype-legacysjlj      # once
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release -DRT64_NAGA_EXECUTABLE=<naga> \
    -DPATCHES_C_COMPILER=<clang with a MIPS target> -DPATCHES_LD=<ld.lld>
cmake --build build-web --target GoldenRecomp
```

`build-web/GoldenRecomp.html` must be served with `Cross-Origin-Opener-Policy: same-origin` and
`Cross-Origin-Embedder-Policy: require-corp` (threads need SharedArrayBuffer), or with `coi_serviceworker.js` beside
it where headers can't be set. Open it in a browser with WebGPU and pick your retail dump with Select ROM: it is kept
in the browser's storage, with settings and saves.

`tools/play_plan.json` plays the page as a player would, for N64Recomp-browser's `tools/play_page.mjs`, in 2-3
minutes: the boot, then Dam, started by `tools/shader_survey.js` as mission select does, and about 35 s of Bond
walking, turning, strafing and firing; then Facility in the same load, started from Dam and played about 25 s.
`tools/play_plan_extended.json`, for when a longer play is needed, goes on to Surface 1 and Frigate that way, in at
most 5 minutes (their headers say more).
