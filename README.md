# GoldenEye 007: Recompiled, on the web

> [!WARNING]
> **This is amazingalek/GoldenEye64Recomp-web, a fork, not GoldenEye64Recomp or GoldenRecomp.** Its `main` branch is
> cblock85's [GoldenEye64Recomp](https://github.com/cblock85/GoldenEye64Recomp) moved onto
> [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) and built into a web page (WebGPU, through
> [N64Recomp-browser](https://github.com/amazingalek/N64Recomp-browser)), and everything on it past cblock85's commits
> was written by AI (Claude). It is not made, endorsed or supported by GoldenEye64Recomp's or GoldenRecomp's authors:
> nothing here is offered upstream, and please don't take issues with this fork to them. Building it:
> [BUILDING.md](BUILDING.md).

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
  audio thread (about 2% of its frames came up short there, under heavy CPU load). No real gamepad and no other browser
  tried.
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
- Mods a player adds are off on the web (there is no recompiler that emits WebAssembly at run time); natively they
  are supported but untested.
- cblock85's run-time recompilation of the game from the ROM (`LiveGamecode`) is gone: RecompFrontend's launcher
  loads the ROM at run time instead, and the game's code is recompiled at build time.
