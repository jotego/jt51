#!/bin/bash

SIMULATOR=ncverilog

$SIMULATOR test.v -F ../../hdl/jt51.f +access+r +define+SIMULATION