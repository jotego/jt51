# Chameleon96 FPGA regression runner

Four independent JT51 instances replay public bus transactions and capture public stereo output. Linux uploads each immutable tape once, starts groups of four jobs, and exports their PCM. Register writes, polling, EVENT shifts, and sample clocks advance entirely in the fabric after START; Linux scheduling does not control their timing.

The target is the NovTech Chameleon96 Cyclone V 5CSEBA6U19C8. The whole runner uses a constrained 50 MHz clock divided from HPS user0. Each instance advances one JT51 master clock per fabric clock. The design preserves the running HPS and drives no expansion-header pins. It requires no arcade PCB for model execution.

Source the JTCORES root `setprj.sh` before building. Build products, tapes, jobs and recorded outputs belong outside git.

1. Build the FPGA using [board/build.sh](board/build.sh) with a new output directory. Check the actual post-fit timing and resources; compilation success alone does not qualify an image.
2. Build and run the tape compiler as described in [host/README.md](host/README.md). Prepare TSV jobs with independent idle clocks, EVENT shifts and capture settings.
3. On the board, compile `host/c96-jt51.c` with `gcc -std=c99 -O2 -Wall -Wextra -Werror`. Use [board/load-fpga.sh](board/load-fpga.sh) with the matching source-hash file to load the runtime image and check its identity.
4. Run `c96-jt51 batch CASE.tape JOBS.tsv NEW_OUTPUT_DIRECTORY --expect-hash source-hash.txt`. Decode each completed binary with `host/decode.go` before reference analysis.

[host/FORMAT.md](host/FORMAT.md) defines the command, job and result formats. [INTERFACE.md](INTERFACE.md) defines replay semantics. Each result binds source identity, actual run and lane, sample cadence, execution count, error state and output mode. The outer regression controller must also bind the tape/jobs/tool hashes and board boot identity, require the entire planned candidate set, and preserve failed results.

Small changes and harness checks use Verilator through [ver/sim.sh](ver/sim.sh); the comparison corpus runs on the FPGA. Before admitting a new hardware build, compare short FPGA runs exactly against saved Verilator output, including independent lanes, boundary EVENT shifts, full-resolution output and decimation. Audio must be nonzero. Production HDL is shared directly with the core; this runner introduces no alternate sound algorithm.

The initial on-chip capacities are 4096 replay records and 16384 stereo samples per lane. Long retained-history experiments need a separately validated replay extension; host pauses or clock-enable stalls must not silently substitute for continuous logical time.
