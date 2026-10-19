# Accelerated JT51 regression interface, revision 1

Four unmodified JT51 instances run on a globally routed 50 MHz divide-by-two of the existing 100 MHz HPS user0 clock, subject to post-fit timing. The initial direct100MHz build failed setup timing and must not be used. One fabric edge is one logical YM master edge, cen=1 and cen_p1=logical_clock[0]. External PCM header pins remain inputs. Small simulations qualify the harness; comparison corpora execute on FPGA. Production HDL is unchanged.

Each lane owns a replicated 4096×64 tape RAM (independent read port) and a 16384×40 output RAM. Tape writes can broadcast to all four replicas. Four capture banks plus four tape replicas cost approximately 384 M10Ks before the JT51 cores; actual fitter usage decides feasibility. Capture words are {post-edge status8, left16, right16}; mode1 substitutes xleft/xright. Clock labels are stored as first_clock48 plus verified native64-clock cadence and stored64*stride-clock cadence. No fabricated xleft/xright columns are exported in quantized mode.

A START begins logical clock0, holds DUT reset for clocks0…255, releases for256…511, then INIT begins at512+idle_clocks. The DUT remains clocked during a run. No cen-based pausing or HPS traffic affects timing. Completion safely drains the last capture write before asserting DONE/reset. Native sample is tested before edge n; outputs/status are captured after edge n, labelled n+1, by a one-edge delayed BRAM write. Missed timestamps, unexpected busy, bad cadence, overflow and abort are failures.

## Tape

64-bit record: [63:16] timestamp48 relative to INIT start; [15:12] opcode; [11:4] data8; [3:1] bus pins {cs_n,wr_n,a0}; [0] reserved0. Timestamps strictly increase. Core bus values must be prepared one edge before the specified timestamp; reads inspect pre-edge public dout. A two-clock WR pulse has explicit assert and release records. Synchronous tape RAM prefetch must handle consecutive actions two clocks apart.

- op0 BUS: set all three bus pins and data.
- op1 POLL: set supplied bus pins/data; require pre-edge dout[7]==0.
- op2 CHECK: set cs_n=0,wr_n=1,a0=1, retain previous din; data is expected status8; bits[3:1] encode mask {busy,timerB,timerA}, expanded to status bits {7,1,0}. Require masked equality. No implicit wait/stretch.
- op3 CAPTURE: enable recording before the timestamp edge; all low12 bits zero.
- op4 END: finish tape, continue clocks until capture_limit samples, then drain and finish. All low12 bits zero.

Host exports the exact frozen CPU schedule with rational4/3 carry/page costs. Include address/data pulses, both clear-busy polls, exact releases, a CAPTURE record at INIT end, and END at EVENT end. Source fetches affect timestamps but do not need hardware actions. At every record index >= event_first, add lane event_shift0…63 to its timestamp. CAPTURE is before event_first and is unshifted. idle_clocks is literal elapsed public clocks, not an internal state deposit. Host proves every tape action against saved CPU/public journals before FPGA qualification.

## Replay lane ports

`jt51_replay_lane #(AW=12,CAP_AW=14)`:

Inputs: clk,rst,start,abort_run; prog_we,prog_addr[AW-1:0],prog_data[63:0]; length[AW:0],event_first[AW:0],idle_clocks[47:0],event_shift[5:0],capture_limit[CAP_AW:0],stride[23:0],output_mode; read_addr[CAP_AW-1:0].

Outputs: prog_q[63:0],read_data[39:0]; busy,done,failed; error_code[7:0]; count[CAP_AW:0],executed[AW:0]; logical_clock[47:0],first_clock[47:0],last_clock[47:0],failure_clock[47:0],last_status[7:0]. Additional debug bus outputs may be exposed for the small testbench; leave them unconnected at system level.

The host port of tape RAM uses prog_addr for synchronous read and optional writes; execution uses its independent port. Capture read is synchronous. Stride must be nonzero; CAPTURE resets native_phase to0, emits when phase0, then increments/wraps at stride exactly as the C++ wrappers do. Start/config validation and run IDs are handled by the system controller; the lane still guards invalid length/event_first/capture_limit and malformed/missed records. Failed/aborted data never qualify. Already written evidence is preserved until a deliberate new START.

## HPS protocol

Reuse GPO0xff706010/GPI0xff706014 held toggle protocol: bit31 toggle, bit30 reserved/error, op29:24, payload23:0; distinct INFO0x965101. Delayed stable acknowledgment and existing exclusive lock remain. All mutations except ABORT reject if any lane busy. Selectors clear half-word staging. Full tape upload is read back before committing.

Exact command opcodes and file headers are owned by the system-controller/host implementation; both are kept in this folder and versioned together. Source signature is derived from actual DUT+harness HDL at build time and read back before accepting results. Build/program artifacts stay in task artifacts. Original c96-yamaha104 firmware/tool/loader and stock boot files remain unchanged and recoverable.
