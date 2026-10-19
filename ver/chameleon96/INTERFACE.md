# Accelerated JT51 regression interface, revision 2

Four unmodified JT51 instances run on a globally routed 50 MHz divide-by-two of the existing 100 MHz HPS user0 clock, subject to post-fit timing. The initial direct 100 MHz build failed setup timing and must not be used. During execution one enabled DUT edge is one logical YM master edge, cen=1 and cen_p1=logical_clock[0]. External PCM header pins remain inputs. Small simulations qualify the harness; comparison corpora execute on FPGA. Production HDL is unchanged.

Each lane owns a replicated 4096×64 tape RAM (independent read port) and a 16384×40 output RAM. Tape writes can broadcast to all four replicas. Four capture banks plus four tape replicas cost approximately 384 M10Ks before the JT51 cores; actual fitter usage decides feasibility. Capture words are {post-edge 8-bit status, left16, right16}; mode 1 substitutes xleft/xright. Clock labels are stored as 48-bit first_clock plus verified native 64-clock cadence and stored 64*stride-clock cadence. No fabricated xleft/xright columns are exported in quantized mode.

A START begins logical clock 0, holds DUT reset for clocks 0…255, releases for 256…511, then INIT begins at 512+idle_clocks. Resident tapes run continuously. Streamed tapes stop the complete DUT clock at explicit PAGE boundaries; host time never advances logical time. cen alone does not freeze JT51 and is not used as the suspension mechanism. Completion safely drains the last capture write before asserting DONE/reset. Native sample is tested before edge n; outputs/status are captured after edge n, labelled n+1, by a one-edge delayed BRAM write. Missed timestamps, unexpected busy, bad cadence, overflow and abort are failures.

## Tape

64-bit record: [63:16] timestamp48 relative to INIT start; [15:12] opcode; [11:4] data8; [3:1] bus pins {cs_n,wr_n,a0}; [0] reserved0. Timestamps strictly increase. Core bus values must be prepared one edge before the specified timestamp; reads inspect pre-edge public dout. A two-clock WR pulse has explicit assert and release records. Synchronous tape RAM prefetch must handle consecutive actions two clocks apart.

- op0 BUS: set all three bus pins and data.
- op1 POLL: set supplied bus pins/data; require pre-edge dout[7]==0.
- op2 CHECK: set cs_n=0,wr_n=1,a0=1, retain previous din; data is expected 8-bit status; bits[3:1] encode mask {busy,timerB,timerA}, expanded to status bits {7,1,0}. Require masked equality. No implicit wait/stretch.
- op3 CAPTURE: enable recording before the timestamp edge; all low 12 bits zero.
- op4 END: finish tape, continue clocks until capture_limit samples, then drain and finish.
- op5 PAGE: stream mode only; zero low 12 bits, last record of a nonfinal page. Prepare at its normal timestamp T, then account the record on the following logical edge T+1 and assert paused. The first record of the next page must be later than T+1. Resident mode rejects PAGE. All low 12 bits zero.

Host exports the exact frozen CPU schedule with rational 4/3 carry/page costs. Include address/data pulses, both clear-busy polls, exact releases, a CAPTURE record at INIT end, and END at EVENT end. Source fetches affect timestamps but do not need hardware actions. At every record index >= event_first, add lane event_shift 0…63 to its timestamp. CAPTURE is before event_first and is unshifted. idle_clocks is literal elapsed public clocks, not an internal state deposit. Host proves every tape action against saved CPU/public journals before FPGA qualification.

## Replay lane ports

`jt51_replay_lane #(AW=12,CAP_AW=14)`:

Inputs: clk,rst,start,abort_run,stream_enable,continue_page,stream_event_first[31:0]; prog_we,prog_addr[AW-1:0],prog_data[63:0]; length[AW:0],event_first[AW:0],idle_clocks[47:0],event_shift[5:0],capture_limit[CAP_AW:0],stride[23:0],output_mode; read_addr[CAP_AW-1:0].

Outputs: prog_q[63:0],read_data[39:0]; busy,done,failed,paused; total_executed[31:0],page_index[31:0]; error_code[7:0]; count[CAP_AW:0],executed[AW:0]; logical_clock[47:0],first_clock[47:0],last_clock[47:0],failure_clock[47:0],last_status[7:0]. Additional debug bus outputs may be exposed for the small testbench; leave them unconnected at system level.

The host port of tape RAM uses prog_addr for synchronous read and optional writes; execution uses its independent port. Capture read is synchronous. Stride must be nonzero; CAPTURE resets native_phase to 0, emits when phase 0, then increments/wraps at stride exactly as the C++ wrappers do. Start/config validation and run IDs are handled by the system controller; the lane still guards invalid length/event_first/capture_limit and malformed/missed records. Failed/aborted data never qualify. Already written evidence is preserved until a deliberate new START.

## HPS protocol

Reuse GPO `0xff706010`/GPI `0xff706014` held toggle protocol: bit 31 toggle, bit 30 reserved/error, op 29:24, payload 23:0; distinct INFO `0x965102`. Delayed stable acknowledgment and existing exclusive lock remain. DUT/epoch/job configuration rejects while busy. Tape upload/readback and per-page length/event_first configuration are allowed only when every busy lane is paused. CONTINUE requires every selected lane busy and paused with matching page sequence and previous 256-bit SHA256. ABORT remains available. Selectors clear half-word staging. Full tape upload is read back before committing.

Exact command opcodes and file headers are owned by the system-controller/host implementation; both are kept in this folder and versioned together. Source signature is derived from actual DUT+harness HDL at build time and read back before accepting results. Build/program artifacts stay in task artifacts. Original c96-yamaha104 firmware/tool/loader and stock boot files remain unchanged and recoverable.

## Retained history suspension

A per-lane Intel altclkctrl uses a falling-edge registered enable and dedicated global clock routing. The controller and tape/capture RAM remain on the parent 50 MHz clock. PAGE closes the DUT gate at the falling edge following T+1, after that normal DUT edge. All subsequent host clocks leave logical_clock, bus, reset, sample stride, public status and the pending PCM write unchanged. Both cen and cen_p1 are zero during this interval. Clock gating also holds JT51 registers that operate outside cen.

CONTINUE latches the next page length and resets only the page-local record index/executed counter. It primes the two synchronous tape front registers over three physical host edges with the DUT still stopped, then reopens the gate on a falling edge. The next DUT/logical edge is T+2. Capture arm/first/last clocks, pending post-edge sample, previous timestamp, phase, global execution total, actual run ID, generation and every DUT register retain their values. No reset, elapsed-clock addition or state deposit occurs.

stream_event_first is the complete transport record index, including PAGE records, bound by the controller/native configuration. The native manifest validator derives each page's local event_first by clamping the global boundary minus all preceding transport records, and reads back that configuration on every lane after upload. START/CONTINUE latch this local threshold; the original short page-index comparison decides EVENT shift during replay. No global counter addition enters the per-edge target path. A one-bit had_record flag preserves cross-page timestamp ordering. total_executed counts all normal and PAGE controls, executed counts the current/final page, and page_index starts at 0. All global counts are 32-bit. The native manifest and hardware chain bind each continuation to its previous page SHA256 and index; full RAM readback proves uploaded content before continuation.

PAGE timestamps are chosen in existing public idle-bus gaps with WR deasserted; the native tool rejects active-WR boundaries. Logical gaps remain exactly as recorded. Linux can take arbitrary time to load the next page without changing any emulated interval. The gate model under VERILATOR samples enable on the falling edge and masks the parent clock; synthesis always uses the Intel primitive. Hardware admission additionally requires fitted dedicated clock routing, all-corner timing and short direct FPGA/Verilator output equivalence.
