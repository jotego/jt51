#!/bin/bash

# This simulation compares the phase modulation RTL implementation
# with the C++ model

verilator --cc ../../hdl/jt51_pm.v test.cc --exe

if ! make -j -C obj_dir -f Vjt51_pm.mk Vjt51_pm > mk.log; then
    cat mk.log
    exit $?
fi

rm mk.log
obj_dir/Vjt51_pm