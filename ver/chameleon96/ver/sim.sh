#!/bin/bash
# Small harness qualification only; the regression corpus runs on the FPGA.
set -euo pipefail
main() {
    src=$(cd -- "$(dirname -- "$0")" && pwd)
    jt51=$(cd "$src/../../.." && pwd)
    jtroot=$(cd "$jt51/../.." && pwd)
    build=${JT51_LANE_BUILD:-${TMPDIR:-/tmp}/jt51-lane-validation}
    mode=${1:-directed}
    shift || true
    mkdir -p "$build"
    quiet=()
    if verilator --help 2>/dev/null | rg -q -- '--quiet '; then quiet=(--quiet); fi
    case "$mode" in
     directed|control|clock)
        [[ $# == 0 ]] || exit 2
        top=lane_tb
        extra=()
        if [[ $mode == clock ]]; then top=clock_tb; fi
        if [[ $mode == control ]]; then
            top=control_tb
            mkdir -p "$build/control-include"
            # Validation-only identity; production build.vh remains build-owned.
            printf '%s\n' "\`define JT51_FPGA_SOURCE_HASH256 256'h0" > "$build/control-include/build.vh"
            extra=("-I$build/control-include" "$jt51/ver/chameleon96/rtl/c96_jt51_control.v")
        fi
        verilator "${quiet[@]}" --binary --timing --build -j 4 -Wno-fatal \
            --timescale 1ns/1ps --top-module "$top" --Mdir "$build/$mode" \
            "$jtroot/modules/jtframe/hdl/ram/jtframe_dual_ram.v" \
            "$jt51/ver/chameleon96/rtl/jt51_replay_clock.v" \
            "$jt51/ver/chameleon96/rtl/jt51_replay_lane.v" \
            "${extra[@]}" "$src/lane_mock.v" "$src/$top.v" > "$build/build-$mode.log" 2>&1
        timeout 30s "$build/$mode/V$top"
        ;;
     parity)
        [[ $# == 7 ]] || { echo 'parity TAPE GOLDEN CSVOUT IDLE SHIFT STRIDE MODE' >&2; exit 2; }
        verilator "${quiet[@]}" --cc --exe --build -j 4 --top-module jt51_replay_lane \
            --timescale 1ns/1ps --Mdir "$build" -F "$jt51/hdl/jt51.f" \
            "$jtroot/modules/jtframe/hdl/ram/jtframe_dual_ram.v" \
            "$jt51/ver/chameleon96/rtl/jt51_replay_clock.v" \
            "$jt51/ver/chameleon96/rtl/jt51_replay_lane.v" "$src/parity.cpp" \
            > "$build/build-parity.log" 2>&1
        timeout 2m "$build/Vjt51_replay_lane" "$@"
        ;;
     stream)
        [[ $# == 4 ]] || { echo 'stream TAPE MODE STRIDE SHIFT' >&2; exit 2; }
        verilator "${quiet[@]}" --cc --exe --build -j 4 --top-module jt51_replay_lane \
            --timescale 1ns/1ps --Mdir "$build" -F "$jt51/hdl/jt51.f" \
            "$jtroot/modules/jtframe/hdl/ram/jtframe_dual_ram.v" \
            "$jt51/ver/chameleon96/rtl/jt51_replay_clock.v" \
            "$jt51/ver/chameleon96/rtl/jt51_replay_lane.v" "$src/stream.cpp" \
            > "$build/build-stream.log" 2>&1
        timeout 2m "$build/Vjt51_replay_lane" "$@"
        ;;
     transport)
        [[ $# == 6 || $# == 9 ]] || { echo 'transport stream MANIFEST JOBS OUTPUT --expect-hash HASH [--then-batch RESIDENT_TAPE OUTPUT]' >&2; exit 2; }
        mkdir -p "$build/transport-include"
        printf '%s\n' "\`define JT51_FPGA_SOURCE_HASH256 256'h1111111111111111111111111111111111111111111111111111111111111111" > "$build/transport-include/build.vh"
        gcc -std=c99 -O2 -Wall -Wextra -Werror -DC96_JT51_TRANSPORT_TEST \
            -Dmain=c96_native_main -c "$jt51/ver/chameleon96/host/c96-jt51.c" -o "$build/native-tool.o"
        verilator "${quiet[@]}" --cc --exe --build -j 4 --top-module c96_jt51_control \
            --timescale 1ns/1ps --Mdir "$build" -I"$build/transport-include" \
            -F "$jt51/hdl/jt51.f" "$jtroot/modules/jtframe/hdl/ram/jtframe_dual_ram.v" \
            "$jt51/ver/chameleon96/rtl/jt51_replay_clock.v" \
            "$jt51/ver/chameleon96/rtl/jt51_replay_lane.v" \
            "$jt51/ver/chameleon96/rtl/c96_jt51_control.v" "$src/transport.cpp" \
            -LDFLAGS "$build/native-tool.o" > "$build/build-transport.log" 2>&1
        timeout 2m "$build/Vc96_jt51_control" "$@"
        ;;
     *) echo 'Usage: sim.sh directed | control | clock | parity TAPE GOLDEN CSVOUT IDLE SHIFT STRIDE MODE | stream TAPE MODE STRIDE SHIFT | transport stream MANIFEST JOBS OUTPUT --expect-hash HASH [--then-batch RESIDENT_TAPE OUTPUT]' >&2; exit 2 ;;
    esac
}

main "$@"
