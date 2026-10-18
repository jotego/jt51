#!/bin/bash
set -euo pipefail
src=$(cd -- "$(dirname -- "$0")" && pwd)
root=$(cd "$src/../.." && pwd)
build=${JT51_SILICON_BUILD:-/nobackup/tasks/jt51-14/artifacts/silicon-regressions-v1/build}
trace=()
if [[ ${1:-} == --keep ]]; then trace=(--trace-fst -CFLAGS -DTRACE); shift; fi
mkdir -p "$build"
quiet=()
if verilator --help 2>/dev/null | rg -q -- '--quiet '; then quiet=(--quiet); fi
verilator "${quiet[@]}" --cc --exe --build -j 4 --top-module jt51 --timescale 1ns/1ps --Mdir "$build" "${trace[@]}" -F "$root/hdl/jt51.f" "$src/test.cpp" > "$build/build.log" 2>&1
cd "$build"
timeout 15m "$build/Vjt51" "$@"
