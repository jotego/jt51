The host compiler converts an INIT and EVENT monitor bytecode program into the revision 1 replay tape in ../INTERFACE.md. It runs no JT51 model and does not access the board.

Build and run from an initialized JTCORES shell:

```sh
./build_tape.sh /nobackup/tasks/example/compile_tape
/nobackup/tasks/example/compile_tape INIT.bin EVENT.bin OUTPUT.tape
go run test_tape.go /nobackup/tasks/example/compile_tape
```

The build requires a C++17 compiler and OpenSSL libcrypto development files; the optional protocol tests require Go. Outputs are created exclusively: the compiler refuses an existing tape or metadata filename. Build products and generated tapes belong in a task directory, not git.

`cpu_schedule.hpp` is a byte-identical copy of the independently qualified own-monitor scheduler from `/nobackup/tasks/jt51-15/round1-20261019-integration/frontend-cpu-v1/cpu_schedule.hpp`, SHA256 `4c5ed67ca129c374a29354e16967be8a0791a02128eb5e0b816094ac5e34d2c4`. It preserves the rational 4/3 CPU/master conversion, fractional carry across INIT/EVENT, table page-crossing costs, and fail-on-busy timing. It contains no third-party sound algorithm. The build embeds the actual compiler and scheduler source SHA256 values in metadata. The archived source is provenance only; building does not depend on task files.

Options `--init-low N` and `--event-low N` specify the initial table pointer low byte (default 0). `--status-mask N` defaults to 3, checking timer flags without requiring the normally transient busy flag to be zero. Supported status mask bits are 7, 1, 0. `--expected-status N` defaults to 0. All options use unsigned decimal values 0…255. The public two-clock write waveform is canonical and is not a measurement of the physical PCB's electrical pulse.

The binary is entirely little-endian. Its fixed 64-byte header is followed by `length` uint64 tape records, with no trailing bytes:

| Byte | Type | Meaning |
|---:|---|---|
|0|8 bytes|ASCII `JT51TAP1`|
|8|uint32|Version 1|
|12|uint32|Header size 64|
|16|uint32|Tape record count, at most 4096|
|20|uint32|First EVENT tape index|
|24|uint64|First active EVENT KEY DATA offset, or UINT64_MAX if absent|
|32|uint64|CAPTURE offset, at INIT END|
|40|uint64|EVENT END offset|
|48|uint64|Flags 0|
|56|uint64|Reserved 0|

Each record is `[timestamp48:opcode4:data8:pins3:reserved0]`, exactly as in INTERFACE.md. Every timestamp is relative to INIT start and strictly increases. Each WRITE emits both busy polls, address assert/release and data assert/release. Polls retain the exact previous din and set bus pins to cs_n=0,wr_n=1,a0=1. CHECK retains din while its data field carries the expected status and its pins field carries the three-bit mask. CAPTURE and END have all low 12 bits zero and leave the bus unchanged. CPU fetches appear in the JSON ledger but require no tape action.

`OUTPUT.tape.json` includes input paths/hashes, compiler/scheduler hashes embedded at build time, tape SHA256, CPU cycle totals and fractional carries, every CPU edge, and every emitted action with held din/bus state. The first active KEY means register 08 DATA with any operator bit in 0x78, matching the frozen frontend's KEY alignment rule; a KEY 06 channel-only event has no active KEY.

Four lanes can use the same immutable tape. Each lane adds `512+idle_clocks` to all timestamps and adds `event_shift` only to indices at or after `event_first`. CAPTURE remains unshifted. To reproduce a saved first-KEY residue `k`, use:

```
event_shift = (k - ((512 + idle_clocks + first_event_KEY_offset) modulo 64)) modulo 64
```

No active KEY means no automatic shift; supply a literal event_shift if needed. These offsets are public elapsed clocks, not internal state deposits. For example, saved AR 12 job r0-d2653-k00 uses idle 169792 and event_shift 47. Capture begins at 1475761, KEY DATA is 1783168 (residue 0), and EVENT END is 1784284.

The permanent tests cover exact binary records, carry/page costs, held bus data, status checks, no-active-KEY handling, overwrite refusal, malformed input and the 4096-record RAM boundary. Additional saved-journal reconstruction evidence belongs in the task artifacts. Full lane/output parity and FPGA qualification are separate gates owned by the harness workflow.

The HPS transport/result layout and TSV job schema are fixed in FORMAT.md. Decode a completed board result using the separately built source signature:

```sh
go run decode.go --expect-hash source-hash.txt INPUT.bin NEW.csv
go test decode.go decode_test.go stream_test.go
```

Use these explicit Go file groups: `test_tape.go` is a separate compiler-test executable. The decoder requires matching source/protocol, a successful terminal status, exact run/lane/count/tape geometry, a consistent native cadence and 48-bit clock range, valid mode, zero reserved fields and exact payload length before creating CSV. It preserves signed 16-bit values and captured status. Mode 0 emits only `sample,left,right,clock,status`; mode 1 emits only `sample,xleft,xright,clock,status`. Timer/busy status bytes in individual samples are recorded verbatim; terminal run errors reject decoding. Failed files remain available as evidence and cannot create a qualified CSV.


Revision 2 `stream` and `validate-stream` use the ordered TSV manifest in FORMAT.md. `c96-jt51.c` includes `sha256.h` and requires only a C99 compiler, with no crypto runtime dependency. `c96-jt51 hash FILE` exposes its streaming SHA256 for known-answer qualification. Native tests compare it with Go's SHA256 across boundary sizes and a million-byte input, reject malformed stream manifests and validate both result-header versions. The new board tool requires protocol 0x965102; the decoder continues to accept sealed revision 1 recordings through their explicit version/protocol path.
