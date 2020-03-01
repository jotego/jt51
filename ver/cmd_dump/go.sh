#!/bin/bash

TRACE=
TIME="-time 3000"

if ! verilator --cc test.v -f gather.f --top-module test --exe test.cpp WaveWritter.cpp --trace; then
    exit $?
else
    if ! make -j -C obj_dir -f Vtest.mk Vtest; then
        exit $?
    fi
    if [ "$TRACE" = "-trace" ]; then
        obj_dir/Vtest -trace $TIME |  grep -v "^INFO: " | vcd2fst -v - -f test.fst
    else
        obj_dir/Vtest $TIME
    fi
fi