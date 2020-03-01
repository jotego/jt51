#!/bin/bash

TRACE=
TIME=

if [ -e set_trace.h ]; then
    mv set_trace.h old_trace.h
fi

echo "// no trace" > set_trace.h

while [ $# -gt 0 ]; do
    case "$1" in
        --trace|-trace)
            TRACE=--trace
            echo "#define TRACE" > set_trace.h
            ;;
        -t|-time)
            shift
            TIME="-time $1";;
        *)  echo "Unknown argument " $1
            exit 1;;
    esac
    shift
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
        obj_dir/Vtest -trace $TIME |  grep -v "^INFO: " | vcd2fst -v - -f test.fst
    else
        obj_dir/Vtest $TIME
    fi
fi