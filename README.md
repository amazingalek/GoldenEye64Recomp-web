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
- The page: the launcher, Load ROM with the retail dump, the intro and its credits, the menus, and Dam walked on the
  keyboard, at the game's 60 retraces a second, with sound on the browser's audio thread. No real gamepad tried. How
  it plays, in which browser and on what graphics, is recorded in N64Recomp-browser's
  [plays.jsonl](https://github.com/amazingalek/N64Recomp-browser/blob/main/plays.jsonl) and its
  [games table](https://github.com/amazingalek/N64Recomp-browser#games).
- A steady 960 frames a second of the sound, about 2%, came up short in both browsers and natively: the game's audio
  manager made every buffer its minimum (`get_frames_remaining` in `src/main/main.cpp` says why). Since main 59dc8e8 it
  writes 48,000 frames a second and plays without a gap between the boot's shader wait and its first burst: on a
  3840x2000 canvas, a cold profile each, frames short per 5 s went from 4,800 to 0 in both browsers outside the bursts,
  and Firefox 158's from 161k in 45 s to 136k.
- The bursts were the main loop's wait for its next tick (`patches/fps.c`), which polled the count without an OS call:
  on the console the scheduler and the audio thread preempt it, and here they waited behind it. In Firefox that was
  about 0.4 s at a few fixed moments of the boot, and in Chrome, in some cold runs, ~20% for ~40 s while the shader list
  builds behind the game. The intro's copy of the wait lasts as long as the frame before took, so one slow frame (a
  shader built behind the game, a load) held every frame of the intro to that length, the main thread polling 85% of
  the time, ~1.5 million counts a second, and the audio thread getting in every ~85 ms instead of 33. Each poll yields
  now (main 7f40089). With and without it, cold, alone on the GPU: Chrome 155 headless at 1256x645 had 0 frames short
  in 85 s in each of three runs, against 630k (~20% for ~40 s) without it; Firefox 158 at 3840x2000 0 short in 95 s,
  against 27k, 67k and 28k in three 5 s windows.
- The page builds the game's shaders before it draws with them, so weak graphics don't draw with RT64's slow general
  shader: the 465 in `src/rt64_shaders.txt`, recorded by a script that plays the intro and the credits, shows every
  menu and starts every level (`tools/record_shaders.sh`), are built a part at a time: the intro's and the menus'
  before the game starts, and each level's when the level first draws. Chrome's pipeline cache holds only part of
  them, so a later visit builds most of a level's again. The script records each level's start, so a shader a level
  draws later is built when it first draws, and the page remembers it and builds it with that level on the next visit.
- Render to RAM stays on: the game reads a frame back 3-33 times a second (up to 120 in some scenes), each read
  waiting for the GPU. The page copies a frame into RDRAM a display list late (rt64-web 73e9ca0): in Dam 3.8-5.2 ms a
  list against 8.0-9.7 with the exact copy, the lamps' glare still drawn (headless Chrome 155, RTX 2070).
- Firefox looks for finished GPU work only after a submission or every 100 ms, so a frame that waits on its readback
  can wait that long. On a laptop's Intel UHD, before N64Recomp-browser v0.21.0, the intro's blood overlay, whose
  shaders the first section didn't hold then, drew ~180 draws a frame with RT64's ubershader, which halved the frame
  rate, and in 3 of 3 scripted first visits that reached it the game then fell to about a frame a second for good, each
  frame waiting on its readback. The page submits nothing once a millisecond while a readback waits (rt64-web
  d070e5c): those visits then kept 28-45 frames a second and a second visit never fell. On an RTX 2070 slowed by a
  heavy pass added to every present, d070e5c took Firefox from 33-37 frames a second to 42-45. The first section now
  holds the intro and the cast credits.
- Firefox builds the shaders one at a time and presents nothing meanwhile, so its waits are the WGSL's size. Since
  rt64-web b052049 (N64Recomp-browser v0.41.0) a pixel shader that picks its tile per pixel (mip LOD) loads a level's
  four taps in one switch over the draw's slots instead of one switch a tap, the list's WGSL 19.0 → 16.3 MB and the
  largest RasterPS 224 → 185 KB: Firefox 158 waited 8.7-9.4 s for the boot and 14.0-21.7 s for Dam with it, against
  10.4-11.6 and 17.7-23.3 without (RTX 2070, 1256x645, a fresh profile each, three runs each, interleaved, beside
  another program's game on the GPU). Since rt64-web 008f380 (v0.50.0) such a shader samples its levels and tiles in
  loops, and Firefox's waits for the boot and Dam together went from 18.6 s to 10.8 s.
- What holds Chrome below 60 in the levels on a big canvas is the levels' own drawing, not the shaders: in Chrome 155
  with a window (`play_page.mjs --headed`) at 3840x2000 it draws 52-58 while the list builds behind the game, 59-60 in
  the title and the credits once all 202 are built (88 s), and then 48-55 in the attract demo's levels with nothing
  left to build and the GPU 75-78% busy.
- In Dam, render to RAM's late copy had the thread that runs the display lists asking at every retrace whether the
  copy could land, and each question waited behind everything queued on the page's thread. Since plume-web 81344f5
  and ba93421 (rt64-web a5bc25d) the readback's answer, the swap chain's size and its refresh rate are published by
  the page's thread and read without waiting. CPU profiles over 13 s in Dam at 3840x2000, Chrome 155 with a window,
  RTX 2070, a busy host:
  - The display list thread's waits for the page's thread fell from 2,335 ms to 151 ms.
  - The present thread's waits fell from 963 ms to 106 ms.
  - The page's thread fell from 47% busy to 32%.

  Presents stayed at 57-59.5 a second.
- The 1.6 s with nothing presented after Start on a first visit is the game's own boot: the boss thread's first
  retraces (~0.6 s), then, with no save yet, `fileValidateSaves` resetting and writing every save to the EEPROM, 48
  EEPROM calls each between `joyDisablePoll` and `joyEnablePoll`, which wait a retrace for the controller thread
  (~0.77 s). A second visit has the save and waits 1.05 s (headless Chrome 155, 3840x2000, RTX 2070; N64Recomp-browser's
  [docs/findings.md](https://github.com/amazingalek/N64Recomp-browser/blob/main/docs/findings.md), Present gaps after
  Start).
- Mods a player adds are off on the web (there is no recompiler that emits WebAssembly at run time); natively they
  are supported but untested.
- cblock85's run-time recompilation of the game from the ROM (`LiveGamecode`) is gone: RecompFrontend's launcher
  loads the ROM at run time instead, and the game's code is recompiled at build time.

## Keyboard and mouse

The `mouse` branch plays the game with keyboard and mouse as the PC ports do (the design of
[goldeneye-pc-port](https://github.com/jkdansereau/goldeneye-pc-port)'s `port/src/input.c`, and the feel of the
[Perfect Dark port](https://github.com/fgsfdsfgs/perfect_dark)), all of it in this fork's own code: `patches/mouse.c` is
the game's side and `src/game/mouse_keyboard.cpp` the program's. It stays off `main` and the page until it has been
played by hand.

- Outside aim mode the mouse turns Bond and tilts his view, 0.055 degrees a count at the default 50% as Perfect Dark's
  port does, less as the view zooms. In aim mode it moves the crosshair, and the view turns while the crosshair is past
  70% of the way to an edge. Mouse up looks up, whatever the game's Look Up/Down option says, unless Settings >
  General > Invert Mouse Y is on.
- In the menus' folders (file, mission and difficulty select) the mouse moves the game's cursor, as the stick does, and
  left click selects.
- Left click fires (also F), right click aims (also Left Shift), the wheel changes weapon, R reloads, and Left Ctrl or
  C crouches, a toggle aim mode keeps. WASD walks and strafes, the arrows turn and look, Space or Q is A (weapon), E is
  B (use), X is L, Return or Tab is Start (the watch). Motion and keys while the watch is up do nothing.
- Settings > General > Mouse Sensitivity sets how far a count goes; 0 turns mouse look off and frees the pointer.
  Above 0 RecompFrontend asks for the browser's pointer lock once the game starts, which a browser grants only at a
  click or a key.
- Played in Dam in Chrome (headless and off the desktop) and Firefox, with synthetic mouse events and Bond's angles,
  crouch, aim mode and watch read from RDRAM: in both the mouse turns him the way the stick does, up looks up, the
  crosshair follows the mouse, the edge turns the view that way, and the watch takes no motion; in Chrome the crouch
  lasts through aim mode and fire, reload and the wheel work. Neither browser gave those runs the pointer lock, so how
  it feels, and the lock itself, are left to a play by hand.
