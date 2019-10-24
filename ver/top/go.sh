#!/bin/bash

if ! which ncverilog; then
    SIMULATOR=iverilog
    MACRO=-D
else
    MACRO=+define+
    SIMULATOR=ncverilog
fi


function append_file {
    for i in $(cat $1/$2); do
        echo -e $1/$i ' '
    done
}

EXTRA=${MACRO}SIMULATION

case $SIMULATOR in
    ncverilog)
        $SIMULATOR test.v -F ../../hdl/jt51.f +access+r $EXTRA
        ;;
    iverilog)
        $SIMULATOR test.v $(append_file ../../hdl jt51.f) $EXTRA -s test \
            -o sim && sim -lxt
        ;;
esac