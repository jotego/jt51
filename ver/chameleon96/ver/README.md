# Replay lane qualification

Run `bash ver/chameleon96/ver/sim.sh directed` in JT51. Set `JT51_LANE_BUILD`
to a fresh external artifact directory. This bounded, small Verilator simulation
uses `lane_mock.v` instead of JT51 solely to verify the replay/capture harness.
The mock changes output values on each edge, making a one-edge capture error
visible; it is excluded from FPGA synthesis and real-DUT parity builds.

The original 133 directed checks cover synchronous tape upload/readback and output-bank
readout, consecutive timestamps, two-clock write pulses, pre-edge POLL/CHECK,
CHECK masks that deliberately permit busy, delayed post-edge PCM/status,
quantized/auxiliary modes, idle/event shifts, stride 2/1024, native cadence and
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
status and clock label with the saved CPU CSV. Mode 0 compares `left/right`;
mode 1 compares `xleft/xright`. It prints both channel ranges to expose silent
comparisons. The output destination must not already exist. Tape format and
header fields must match `JT51TAP1`. External fixture dependence is why this
folder has no `.simunit` marker. The complete reference corpus runs on FPGA;
this tool is for short harness qualifications only.

The lane error codes are 1 invalid configuration, 2 abort, 3 malformed record or
missing END, 4 non-increasing/overflowed timestamp, 5 missed timestamp, 6 busy at
POLL, 7 masked status disagreement, 8 CAPTURE protocol, 9 native/stored cadence,
10 capture overflow, 11 run-time tape mutation/restart and 12 clock overflow. Stream mode additionally checks PAGE termination, host-stall immutability, continuation accounting/cadence, invalid page length, abort, missed/reordered timestamps and mutation during refill.
The specified native sample is tested before logical edge n; its updated
outputs/status are written on edge n+1 and labelled n+1. Two front registers
plus synchronous RAM lookahead support even adjacent record timestamps.

`bash ver/chameleon96/ver/sim.sh control` runs the controller protocol's
small Verilator test with four mock-backed replay lanes. Its validation-only
`build.vh` lives inside the external build directory and does not alter the
FPGA source identity. Both mock flows use Verilator's `--binary --timing`;
the real-DUT parity flow also uses Verilator. No tri-state model is needed.


`sim.sh clock` checks the falling-edge gate model for complete, aligned pulses under enable changes during high and low phases. `sim.sh stream TAPE MODE STRIDE SHIFT` builds two real unmodified JT51 instances. One runs a resident continuous tape; the other receives the same public writes/clocks through 65 pages with variable host stalls. Idempotent idle-bus records extend the original END-only tail so multiple pages close during developed nonzero feedback. Every logical bus/reset/status/capture journal entry and every final PCM/status/clock row must match exactly. Five deliberate pauses close on emitted sample edges, exercising the pending post-edge BRAM write across page loading. Both channels must be bipolar. The short qualifications cover quantized/full-resolution output, native/stride 2 capture and EVENT shifts 0/47/63; full regressions still execute on FPGA.

The extended controller test validates strict initial/full previous+next hash staging, wrong page and previous hash rejection, duplicate CONTINUE rejection, immutable run configuration, all four paused clock/count identities while tape RAM is replaced, and final per-page/global execution/index/digest. This is protocol qualification with the mock, not a sound comparison or a substitute for fitted clock/timing and real board qualification.


`sim.sh transport stream MANIFEST JOBS OUTPUT --expect-hash HASH [--then-batch RESIDENT_TAPE OUTPUT]` links the actual native C host driver to a four-real-DUT RTL controller through validation-only transport/time hooks. No board I/O occurs. Its validation source hash is 64 digits `1`. The optional resident batch follows in the same controller without resetting it, exposing stream/resident configuration leakage. The short transport fixture uses the existing own-monitor register values with compressed public timing, 40 pages and 5 jobs (four lanes plus a partial second group); it is harness qualification only. Compare both payloads, sample epochs and stereo ranges. Simulated transport timing is not a hardware speed benchmark.
