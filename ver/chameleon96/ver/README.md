# Replay lane qualification

Run `bash ver/chameleon96/ver/sim.sh directed` in JT51. Set `JT51_LANE_BUILD`
to a fresh external artifact directory. This bounded, small Verilator simulation
uses `lane_mock.v` instead of JT51 solely to verify the replay/capture harness.
The mock changes output values on each edge, making a one-edge capture error
visible; it is excluded from FPGA synthesis and real-DUT parity builds.

The 133 directed checks cover synchronous tape upload/readback and output-bank
readout, consecutive timestamps, two-clock write pulses, pre-edge POLL/CHECK,
CHECK masks that deliberately permit busy, delayed post-edge PCM/status,
quantized/auxiliary modes, idle/event shifts, stride2/1024, native cadence and
missing-sample watchdogs, terminal buffer retention, all configuration bounds,
reserved fields, non-increasing timestamps, duplicate/missing CAPTURE, missing
END, program mutation while running and abort. A failed run preserves its
already committed prefix; it never qualifies as successful evidence.

For a single selected saved CPU output use:

```
bash ver/chameleon96/ver/sim.sh parity TAPE GOLDEN.csv NEW.csv IDLE SHIFT STRIDE MODE
```

This builds the real unmodified JT51 and the lane, reads back every uploaded
tape word, executes the exact tape, then compares every retained PCM value,
status and clock label with the saved CPU CSV. Mode0 compares `left/right`;
mode1 compares `xleft/xright`. It prints both channel ranges to expose silent
comparisons. The output destination must not already exist. Tape format and
header fields must match `JT51TAP1`. External fixture dependence is why this
folder has no `.simunit` marker. The complete reference corpus runs on FPGA;
this tool is for short harness qualifications only.

The lane error codes are1 invalid configuration,2 abort,3 malformed record or
missing END,4 non-increasing/overflowed timestamp,5 missed timestamp,6 busy at
POLL,7 masked status disagreement,8 CAPTURE protocol,9 native/stored cadence,
10 capture overflow,11 run-time tape mutation/restart and12 clock overflow.
The specified native sample is tested before logical edge n; its updated
outputs/status are written on edge n+1 and labelled n+1. Two front registers
plus synchronous RAM lookahead support even adjacent record timestamps.

`bash ver/chameleon96/ver/sim.sh control` runs the controller protocol's
small Verilator test with four mock-backed replay lanes. Its validation-only
`build.vh` lives inside the external build directory and does not alter the
FPGA source identity. Both mock flows use Verilator's `--binary --timing`;
the real-DUT parity flow also uses Verilator. No tri-state model is needed.
