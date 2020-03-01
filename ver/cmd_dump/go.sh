#!/bin/bash

TRACE=

if [ -e set_trace.h ]; then
    mv set_trace.h old_trace.h
fi

echo "// no trace" > set_trace.h

for i in $*; do
    case "$i" in
        -trace)
            TRACE=--trace
            echo "#define TRACE" > set_trace.h
            ;;
    esac
done

# delete the old files if the trace condition has changed
if  [ -e old_trace.h ]; then
    if ! diff -q set_trace.h old_trace.h >/dev/null; then
        rm -rf obj_dir
    fi
    rm old_trace.h
fi

if ! verilator --cc test.v -f gather.f --top-module test --exe test.cpp WaveWritter.cpp $TRACE; then
    exit $?
else
    if ! make -j -C obj_dir -f Vtest.mk Vtest; then
        exit $?
    fi
    if [ -n "$TRACE" ]; then
        obj_dir/Vtest $* |  grep -v "^INFO: " | vcd2fst -v - -f test.fst
    else
        obj_dir/Vtest $*
    fi
fi