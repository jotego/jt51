# HPS regression transport revision 2

`c96-jt51 probe --expect-hash source-hash.txt` validates protocol `0x965102` and the full 256-bit source signature. `c96-jt51 batch CASE.tape JOBS.tsv NEW_OUTPUT_DIRECTORY --expect-hash source-hash.txt` uploads and reads back all four tape replicas once, runs groups of up to four jobs, and writes exclusive `NAME.bin` files. The JOBS header contains these tab-separated names: `name`, `run_id`, `idle_clocks`, `event_shift`, `frames`, `stride`, `mode`.

Names contain 1–79 ASCII letters, digits, underscores or dashes. Run IDs are 1…0xffffff; idle clocks are unsigned 48-bit; EVENT shifts are 0…63; frame counts are 1…16384; strides are 1…0xffffff. Output mode 0 captures quantized PCM; mode 1 captures full-resolution output. Resident runs execute continuously. Streamed runs wait for host page upload at explicit PAGE boundaries while the complete DUT clock and logical time remain stopped.

## Commands and configuration

| GPO/GPI opcode | Operation |
|---:|---|
| 0 | INFO |
| 1 | CAPABILITIES, packed CAP_AW:AW:lanes bytes |
| 2 | SELECT_LANE |
| 3 | SELECT_MASK |
| 4 | TAPE_ADDR |
| 5 | TAPE_LOW24 |
| 6 | TAPE_MID24 |
| 7 | TAPE_HIGH16, commit and increment |
| 8 | TAPE_READ_LOW24, latch |
| 9 | TAPE_READ_MID24 |
| 10 | TAPE_READ_HIGH16, increment |
| 11 | COMMIT |
| 12 | START_MASK |
| 13 | STATUS |
| 14 | COUNT |
| 15 | ABORT_MASK |
| 16 | CFG_INDEX |
| 17 | CFG_WRITE24 |
| 18 | CFG_READ, index argument |
| 19 | SAMPLE_ADDR |
| 20 | SAMPLE_LOW24, latch |
| 21 | SAMPLE_HIGH16, increment |
| 22 | META, index argument |
| 23 | SOURCE_WORD, index 0…15, least-significant 16-bit word first |
| 24 | PAGE_INDEX, expected next page index in 24 bits |
| 25 | PAGE_NEXT_HASH_WORD |
| 26 | PAGE_PREVIOUS_HASH_WORD |
| 27 | CONTINUE_MASK |
| 28 | ACTIVE_PAGE_HASH_WORD |

Hash writes pack a four-bit word index in bits 23:20, reserved zero bits in 19:16 and 16-bit data in 15:0. Hash reads use an index in the low four bits. Word 0 is the least-significant SHA256 word; byte strings and hexadecimal digests remain canonical big endian. All 16 next-hash words and all 16 previous-hash words must be written after each upload before COMMIT/CONTINUE.

| CFG index | Field |
|---:|---|
| 0 | length |
| 1 | local event_first |
| 2, 3 | idle_clocks low/high 24 bits |
| 4 | event_shift |
| 5 | capture_limit |
| 6 | output_mode |
| 7 | run_id |
| 8 | stride |
| 9 | stream_enable |
| 10, 11 | global_event_first low 24/high 8 bits |

STATUS bits 0…3 are ready, busy, done and failed. Bits 15:8 contain the lane error.

| META index | Field |
|---:|---|
| 0 | executed in current/final page |
| 1 | error |
| 2, 3 | logical_clock low/high 24 bits |
| 4, 5 | first_clock low/high 24 bits |
| 6, 7 | last_clock low/high 24 bits |
| 8, 9 | failure_clock low/high 24 bits |
| 10 | last_status |
| 11, 12 | clock_Hz low/high 24 bits |
| 13 | capture_capacity |
| 14 | tape_capacity |
| 15 | mode |
| 16 | active_run_id |
| 17 | generation |
| 18, 19 | total_executed low 24/high 8 bits |
| 20, 21 | page_index low 24/high 8 bits |
| 22 | paused |
| 23 | stream_enable |

COMMIT makes the selected validated configuration and page identity ready. START requires all lanes idle, initial page index 0 and an all-zero previous hash in stream mode. CONTINUE requires selected lanes to be busy, paused and ready, with expected index equal to actual index+1 and the previous hash equal to each lane's active hash. It consumes identity validity and latches the next hash. Changing global page-index/hash staging invalidates readiness on every lane, regardless of the selected upload mask. START independently revalidates the initial stream identity.

Upload, length and local EVENT configuration may change while every busy lane is paused. Other run configuration remains immutable. ABORT is always available; lane selection and read-only queries are allowed while running. START clears readiness. The actual run ID is latched independently of future configuration.

## Stream manifest

`c96-jt51 validate-stream MANIFEST JOBS.tsv` checks inputs without hardware. `c96-jt51 stream MANIFEST JOBS.tsv NEW_OUTPUT_DIRECTORY --expect-hash source-hash.txt` runs groups of four. The manifest is ASCII TSV with mandatory final newlines:

```
JT51STREAM2<TAB>TOTAL_RECORDS<TAB>GLOBAL_EVENT_FIRST<TAB>PAGE_COUNT<TAB>RECORDS_SHA256
0<TAB>page-000.tape<TAB>PAGE_FILE_SHA256<TAB>RECORDS<TAB>LOCAL_EVENT_FIRST
1<TAB>page-001.tape<TAB>PAGE_FILE_SHA256<TAB>RECORDS<TAB>LOCAL_EVENT_FIRST
```

Every integer is unsigned decimal, pages are ordered 0…count−1, hashes are 64 lowercase hex digits, and basenames permit ASCII letters, digits, underscores, dashes and dots without a leading dot or parent traversal. Files are relative to the manifest. PAGE_FILE_SHA256 hashes the complete 64-byte header and records. RECORDS_SHA256 hashes the concatenation of every page's eight-byte little-endian records, including PAGE controls, in order. Total records include PAGE and are bounded to 4,000,000; page count is 1…4096.

Each page uses the existing JT51TAP1 header with length≤4096. Its local `event_first` is the global boundary minus all preceding record counts, clamped to 0…page length. `capture_offset` and `first_event_KEY_offset` repeat the complete stream identities on every page; `end_offset` is that page's final PAGE/END timestamp. Exactly one CAPTURE occurs in the complete stream. Every nonfinal page ends in PAGE; the final page ends in END. Timestamps remain absolute relative to the original INIT, with no rebasing or reset. The next page's first timestamp must exceed PAGE timestamp+1 and PAGE must occur with WR deasserted. Host validation and every RAM replica readback precede continuation.

`stream-pages.jsonl` is created exclusively and flushed per boundary. It binds source, manifest and ordered-record identities; group, lane, run and generation; previous and next page indices/digests; frozen logical/first/last clocks; count; public status; and global executed count. The host verifies those fields remain unchanged during page loading before issuing CONTINUE, then reads back the latched active index and digest. Missing, reordered or mutated pages and incomplete captures never qualify.

## Current result binary

The current tool writes a 256-byte little-endian header, magic `JT51FP02`, version 2, protocol `0x965102`, followed by count×5 sample bytes. Base offsets retain the meanings in the revision 1 table below, with these changes and additions:

| Byte | Type | Revision 2 meaning |
|---:|---|---|
| 8 | u32 | Header size 256 |
| 12 | u32 | Version 2 |
| 16 | u32 | Protocol 0x965102 |
| 56 | u32 | Total global executed records |
| 60 | u32 | Total transport records |
| 144 | u32 | Global EVENT boundary |
| 160…191 | 32 bytes | Ordered-record SHA256 |
| 192…223 | 32 bytes | Final active page SHA256; zero in resident mode |
| 224 | u32 | Actual page count |
| 228 | u32 | Final-page executed count |
| 232 | u32 | Stream flag, 0 or 1 |
| 236…255 | 20 bytes | Reserved zero |

Final-page/global execution and chain identity must match the complete planned stream. Resident mode has page count 1 and final-page executed=total≤4096.

## Archived revision 1 result binary

The archived format has a 160-byte header, magic `JT51FP01`, followed by count×5 sample bytes. All integers are little endian.

| Byte | Type | Meaning |
|---:|---|---|
| 8 | u32 | header_bytes=160 |
| 12 | u32 | version=1 |
| 16 | u32 | protocol=0x965101 |
| 20 | u32 | physical_clock_Hz |
| 24 | u32 | active_run_id |
| 28 | u32 | lane |
| 32 | u32 | count |
| 36 | u32 | requested_frames |
| 40 | u32 | stride |
| 44 | u32 | mode |
| 48 | u32 | terminal_STATUS |
| 52 | u32 | error_code |
| 56 | u32 | executed_tape_records |
| 60 | u32 | tape_length |
| 64 | u64 | first_clock |
| 72 | u64 | last_clock |
| 80 | u64 | logical_clock |
| 88 | u64 | failure_clock |
| 96 | u32 | generation |
| 100 | u32 | event_shift |
| 104 | u64 | idle_clocks |
| 112…143 | 32 bytes | Canonical big-endian SHA256 source signature |
| 144 | u32 | event_first |
| 148 | u32 | sample_bytes=5 |
| 152…159 | 8 bytes | Reserved zero |

Each five-byte little-endian word is `{status8,left16,right16}`: right occupies bits 15:0, left 31:16 and status 39:32. Signed PCM uses 16-bit two's complement. Mode 1 uses xleft/xright in the same positions and must be labelled accordingly. Native sample clocks are `first_clock+i×64×stride`; driver and decoder require last_clock consistency. Every run must be terminal done, nonfailed, count=requested, executed=length and error=0 before qualification. Failures remain files and metadata and never qualify.

The decoder emits quantized CSV `sample,left,right,clock,status`, or full-resolution `sample,xleft,xright,clock,status`. It never invents uncaptured columns and creates only new files. The outer controller binds source, tape, jobs, tool and binary SHA256 values plus board boot identity. Reference comparison uses the existing unmodified finite observer on recorded public PCM; model execution runs on FPGA.
