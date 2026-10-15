# GoldenEye 007: Recompiled, on the web

> [!WARNING]
> **This is amazingalek/GoldenEye64Recomp-web, a fork, not GoldenEye64Recomp or GoldenRecomp.** Its `main` branch is
> cblock85's [GoldenEye64Recomp](https://github.com/cblock85/GoldenEye64Recomp) moved onto
> [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) and built into a web page (WebGPU, through
> [N64Recomp-browser](https://github.com/amazingalek/N64Recomp-browser)), and everything on it past cblock85's commits
> was written by AI (Claude). It is not made, endorsed or supported by GoldenEye64Recomp's or GoldenRecomp's authors:
> nothing here is offered upstream, and please don't take issues with this fork to them. Building it:
> [BUILDING.md](BUILDING.md).
>
> This port is here to build out and test N64Recomp-browser's web support on another game than the one it was written
> for: what it turns up is fixed there where it can be, so every recomp's page gets the fix.

**Play it in the browser: https://amazingalek.github.io/GoldenEye64Recomp-web/**, with your own ROM, picked in the
launcher and kept in the browser. It needs WebGPU (Chrome or Edge 113 and up). The page is built from this branch
and published with N64Recomp-browser's `tools/publish_page.sh`, so it can be a commit or two behind.

GoldenEye 007 (N64, NTSC-U) statically recompiled with [N64Recomp](https://github.com/N64Recomp/N64Recomp) into C,
run by [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) and drawn by [RT64](https://github.com/rt64/rt64),
from the [GoldenEye decompilation](https://gitlab.com/kholdfuzion/goldeneye_src). It descends from kholdfuzion's
[GoldenRecomp](https://github.com/kholdfuzion/goldenrecomp) (Windows) through cblock85's GoldenEye64Recomp (macOS
and Linux). This fork builds as a native program (checked on Windows) and as a page that runs in a browser with
WebGPU, both with RecompFrontend's launcher and menus, on upstream RT64, N64ModernRuntime and RecompFrontend at the
commits N64Recomp-browser's web forks are built on, with its `patches/` applied (BUILDING.md).

## No Nintendo assets

This repository holds no game code, no game assets and no ROM data. You need your own GoldenEye 007 NTSC-U dump
(big-endian `.z64`, sha1 `abe01e4aeb033b6c0836819f549c791b26cfde83`).

What is committed is symbol data (function names, addresses and sizes in `dump.toml` and `data_dump.toml`), the
port's own code, and two patches that rearrange *your* dump into the "TLBFREE" layout the recompiled code was made
from: `vanilla_to_tlbfree.xdelta` to build with, and `vanilla_to_tlbfree.gep1`, which the program applies to the
ROM you load, so the launcher takes the retail dump as it is.

## Status

- Native (Windows, clang-cl): the launcher, the gun-barrel intro and the attract demo.
- The page, in headless Chrome (2026-10-12): the launcher, Load ROM with the retail dump, the intro and its
  credits, the menus, and Dam walked on the keyboard, at the game's 60 retraces a second, with sound on the browser's
  audio thread (about 2% of its frames came up short there, under heavy CPU load). No real gamepad tried.
- Played from the published page (main 775e1b3, N64Recomp-browser 7b80155) on 2026-10-14 (RTX 2070, an idle host), by
  `tools/shader_plan.json` with N64Recomp-browser's `tools/play_page.mjs`: the launcher, Load ROM, the boot and Dam
  in both browsers, with no WebGPU errors. On a first visit a local build of the same commit in headless Chrome 155
  never waited: it builds the whole list behind the game, all 202 pipelines 72 s after the page opened, and drew 48-60
  frames a second meanwhile, the credits included (7-36 on v0.28.0, before rt64-web 6734747 paced presents only while
  the GPU falls behind), about 2% of the sound short. Firefox 158 waited 8.1 s for the boot's 59 and 13.4 s for Dam's
  53, and then drew 60, about 2% of the sound short. On a 3840x2000 canvas, the stand-in for integrated graphics,
  Chrome never waited and drew 53-59, 6% of the sound short; Firefox waited 8-9 s and then drew 52-61 in one run,
  10% short; in another, alone on the GPU, it presented 171 a second for 5 s as the wait ended, the GPU's memory up
  3.5 GB, then logged "Out of Memory" and drew 22-30 (Firefox's animation frames come in bursts there, which the page
  reads as a faster display).
  On the page built on N64Recomp-browser b1c35d1 (main 97f81af), 2026-10-14, the same canvas, an idle host, one page
  at a time: Firefox 158 waited 8.4 s and then drew 58-60, the memory flat, 8% of the sound short; Chrome 155 never
  waited and drew 52-56, 8% short.
  About 2% of that, a steady 960 frames a second, was the game's audio manager making every buffer its minimum
  (`get_frames_remaining` in `src/main/main.cpp` says why); since main 59dc8e8 it writes 48,000 frames a second and
  plays without a gap between the boot's shader wait and its first burst. A/B on 2026-10-14, the same canvas, a cold
  profile each, an idle host: frames short per 5 s from 4,800 to 0 in both browsers outside the bursts; Firefox 158
  161k short in 45 s before, 136k after. What was left then: in Firefox about 0.4 s at a few fixed moments of the boot, and
  in Chrome, in some cold runs before and after, ~20% for ~40 s while the shader list builds behind the game.
  On the page built on N64Recomp-browser ed8764a (v0.36.0, main 0ceb0be), 2026-10-14, the same canvas, an idle host,
  one page at a time, the boot, the credits and the attract demo's Runway: Firefox 158 waited 8.4 s and then drew 60
  through the boot and the credits, about 0.4 s of sound short three times, then waited 12.6 s for Runway and drew
  44-51 there; Chrome 155 never waited and drew 45-60 until all 202 were built at 65 s, 2.7 s of sound short meanwhile,
  then 60. rt64's pacing never switched on.
  Played again from the page built on N64Recomp-browser 2efc795 (rt64-web 9ed449d, main e8aa16e) on 2026-10-14, on that
  canvas, a quiet host, one page at a time, 25 s after the waits: Firefox 158 waited 8.6 s for the boot and then
  drew 57-59, Chrome 155 never waited and drew 56-57, rt64's pacing never on, no sound short, no WebGPU errors.
  Those bursts and Chrome's 20% were the main loop's wait for its next tick (`patches/fps.c`), which polled the count
  without an OS call: on the console the scheduler and the audio thread preempt it, and here they waited behind it.
  The intro's copy of it waits as long as the frame before took, so one slow frame (a shader built behind the game, a
  load) held every frame of the intro to that length, the main thread polling 85% of the time, ~1.5 million counts a
  second, and the audio thread getting in every ~85 ms instead of 33. Each poll yields now. Local builds on
  N64Recomp-browser b1c35d1, with and without that, 2026-10-14, a cold profile each, alone on the GPU: Chrome 155
  headless at 1256x645, 0 frames short in 85 s in each of three runs, against 630k (~20% for ~40 s) without it;
  Firefox 158 at 3840x2000, 0 short in 95 s, against 27k, 67k and 28k in three 5 s windows. From Pages (main
  e8aa16e, N64Recomp-browser 2efc795), 2026-10-14, 3840x2000, a cold profile each, alone on the GPU, an idle host,
  the boot and the credits playing by themselves: Chrome 155 never waited and drew 55-60 a second while all 202 built
  (77 s in), Firefox 158 waited 8.5 s and then drew 58-60, 0 frames of sound short in 60 s in both after the first
  second, and no WebGPU errors.
- The page builds the game's shaders before it draws with them, so weak graphics don't draw with RT64's slow general
  shader: the 465 in `src/rt64_shaders.txt`, recorded by a script that plays the intro and the credits, shows every
  menu and starts every level (`tools/record_shaders.sh`), are built a part at a time with the game waiting under
  "Preparing graphics": the intro's and the menus' before the game starts, and each level's when the level first
  draws. Cold in Chrome 155 (2026-10-13, RTX 2070, headless) the menus took 2.5 s and Dam 6.6 s, and with the intro
  5.0 s against the menus' 3.9 s on a busier host; on a later visit 0.3 s and 4.5 s, since Chrome's cache holds
  only part of them. The script records each level's start, so a shader a level draws later is built when it first
  draws, and the page remembers it and builds it with that level on the next visit.
- Render to RAM stays on: the game reads a frame back 3-33 times a second (up to 120 in some scenes), each read
  waiting for the GPU. The page copies a frame into RDRAM a display list late (rt64-web 73e9ca0): in Dam 3.8-5.2 ms a
  list against 8.0-9.7 with the exact copy, the lamps' glare still drawn (headless Chrome 155, RTX 2070, 2026-10-13).
- Firefox, on a laptop's Intel UHD (scripted first visits, 2026-10-13, before N64Recomp-browser v0.21.0): the
  intro's blood overlay, whose shaders the first section didn't hold then, drew ~180 draws a frame with RT64's
  ubershader, which halved the frame rate, and in 3 of 3 runs that reached it the game then fell to about a frame a
  second for good, each frame waiting on its readback. Firefox looks for finished GPU work only after a submission or
  every 100 ms, and since v0.21.0 the page submits nothing once a millisecond while a readback waits (rt64-web
  d070e5c): those runs then kept 28-45 frames a second and a second visit never fell. The first section now holds
  the intro and the cast credits. On an RTX 2070 slowed by a heavy pass added to every present, d070e5c took Firefox
  from 33-37 frames a second to 42-45. Firefox builds a section's shaders one at a time and presents nothing
  meanwhile: the game stood still 23 s for Dam's 55 on a first visit there (RTX 2070).
- Played on that laptop's Intel UHD (Tiger Lake, Windows 11 in its best power efficiency mode) on 2026-10-14, from the
  published page (main e8aa16e, N64Recomp-browser 2efc795), by `tools/shader_plan.json` (`--play 90` and `--section
  dam`) with N64Recomp-browser's `play_page.mjs` at 1256x645, Firefox 158 and headless Chrome 155, a fresh profile
  each, one page at a time, presents a second as medians after the waits, the RTX beside it drawing nothing: Firefox
  waited 7.7 s for the boot's 59 and drew 46.7 (24.4-51.0), rt64 pacing it twice to the game's 6 while the logo
  turned; for Dam it waited 8.5 s and then 11.8 s for the section's 53, about 20 s in all, and drew 56.6 over the rest
  of the run, about 20 s. Chrome never waited and drew 47.7 and, at Dam, 54.7. The Intel chip 27-48% busy, no sound
  short, no WebGPU errors.
- Played again from the page built on N64Recomp-browser d3b9031 (N64ModernRuntime-web f0e0884, RecompFrontend-web
  75fe596, rt64-web f34b19c; main 82b2a37, gh-pages bf095ed) on 2026-10-15, its files served locally with the
  cross-origin headers, RTX 2070, 1256x645, a fresh profile each, the host's every core busy with other work, the boot
  and then 40 s with `play_page.mjs`: headless Chrome 155 never waited and drew 56.8 presents a second (median),
  Firefox 158 waited 18.9 s for the boot's 59 and then drew 58.8. No sound short, no WebGPU errors or exceptions in
  either.
- Played again from Pages, built on N64Recomp-browser 7277a2e (rt64-web 8bea892, tile copies reused from one display
  list to the next; main b94f28f, gh-pages bff2ce6) on 2026-10-15, RTX 2070, 1256x645, a fresh profile each, the host's
  every core busy with other work, by `tools/shader_plan.json`'s `dam` section with `play_page.mjs`: headless Chrome 155
  never waited and drew 58.3 presents a second (median), Firefox 158 waited 8.3 s for the boot's 59 and 14.4 s for Dam's
  52 and drew 59.9. No sound short, no WebGPU errors in either. Firefox's one exception is the first load's wasm fetch,
  cut off by `coi_serviceworker.js`'s reload.
- Mods a player adds are off on the web (there is no recompiler that emits WebAssembly at run time); natively they
  are supported but untested.
- cblock85's run-time recompilation of the game from the ROM (`LiveGamecode`) is gone: RecompFrontend's launcher
  loads the ROM at run time instead, and the game's code is recompiled at build time.
