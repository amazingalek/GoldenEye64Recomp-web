#!/usr/bin/env bash
# Records src/rt64_shaders.txt, the shaders the page builds before the game and each level start, by code rather than
# by playing: N64Recomp-browser's tools/record_shaders.mjs serves the page built in <build dir>, drives it in headless
# Chrome through tools/shader_plan.json, a page load a section, in which patches/shader_survey.c shows each menu and
# starts each level (five of them a second time, played about 45 s in), and writes the list. Needs node 22 or later
# and Chrome, and takes about 30 minutes. Then build the page again, so that it prebuilds the new list.
#
#   bash tools/record_shaders.sh <build dir> <retail ROM> [--profile <dir>] [--port <n>] [--only <section>,...]
set -euo pipefail
[ $# -ge 2 ] || { sed -n '8p' "$0"; exit 1; }
cd "$(dirname "$0")/.."
build="$1" rom="$2"
shift 2
exec node lib/N64Recomp-browser/tools/record_shaders.mjs tools/shader_plan.json "$build" "$rom" src/rt64_shaders.txt "$@"
