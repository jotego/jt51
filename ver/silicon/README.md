# Public silicon regressions

Run `bash ver/silicon/sim.sh all` from the JT51 repository. Individual groups
are `analysis`, `timer`, `attack`, `zero`, `d1l`, `granularity`, `csm`,
`routes`, `noise`, `slowhold`, `csm_states`, `release`, `mixing`,
`mixing_mirror`, `nominal_pitch`, `stereo_age_analysis`, and `stereo_age`.
Set `JT51_SILICON_BUILD` to an absolute, fresh artifact directory. Builds and
logs are external to this source tree by default. Every execution has a
15-minute external timeout. `--keep` enables `test.fst` in that directory;
otherwise no waveform is created. The installed Verilator version is checked
for `--quiet` support because older versions reject it.

The harness uses public reset, clock enables, busy-safe CPU writes, status and
Yamaha-quantized `left`/`right` outputs. It never observes internal EG/PG state.
The register bus and sample timing follow the previously qualified measurement
wrapper. It does not include any external emulator algorithm. All stimuli are
literal register settings generated in C++; no external fixture is required.

These regression expectations were frozen from Yamaha application notes and
physical measurement reports in the parent jtcores checkout, before candidate
RTL was tested:

* `tasks/jtcores-143/plans/timer-measurements.md`: all16 register14 controls,
  disabled overflow, enabled flag, sticky disable/stop, and explicit clear.
  IRQ is deliberately not described as physically measured.
* `tasks/jtcores-143/plans/envelope-measurements.md`: four measured effective
  AR30/31 attack boundaries must be gradual (35–55ms interior10–90%, a coarse
  discrimination around measured39–46ms), raw AR0 must remain exactly silent,
  and raw D1R/D2R0 holds must preserve measured projected levels within2PCM.
  KS0–3 are exercised, with bipolar AR1 and held-tone positive controls.
* `tasks/jtcores-143/plans/envelope-stages.md`: D1L15 must have an exact-zero
  final-quarter tail after a nonzero transient. Adjacent D1L14 peaks60–65PCM
  qualify the low-level path. D1/D2 rate12 at KC4A/KS0 must resolve attenuation
  steps0.089–0.099dB and70–80 plateaus between38% and85% full amplitude.
  These deliberately coarse bounds surround the independently qualified
  physical0.093349–0.094489dB/74–75 plateaus and exclude measured JT0.1875dB.
* `tasks/jtcores-143/plans/csm-measurements.md`: automatic Timer-A CSM must
  create bipolar audio and a finite energy cadence256/512/1024±64 native
  samples. Disabling CSM and timers must yield quiet after0.5s. An exact
  automatic attack shape, pulse width or accepted-write phase is not asserted.
* `tasks/jt51-13/plans/low-release.md`: rawRR0 at KS0/KC00, KS3/KC04 and adjacent
  KC08/KC10 must visibly attenuate in the finite16384-frame/stride32 window.
  The late/early1024-frame RMS ratio must be0.2–0.8, a coarse hold discriminator
  encompassing the measured ratios0.30–0.56. Both output paths must be bipolar.
  This self-contained initialized test does not replace the archived literal
  preserved-history regression or claim exact key-off latency.

Projection is ordinary least squares against a fixed known carrier
65984/2^20 cycles per native sample plus DC, in32-sample windows at hop4.
Plateau classification follows the recorded output-domain method: less than
0.015dB variation over16 native samples, adjacent levels separated by0.03dB.
No candidate frequency/gain/phase/threshold is fitted. The retained physical
source reports and archives remain the authority for narrower comparisons.

The suite is a representative regression for confirmed gaps, not a full
chip conformance claim or a substitute for broader archived interaction sweeps.
There is no `.simunit` marker: the slower release family and standalone
measurement workflow are intentionally launched through `sim.sh`.

The `analysis` group also qualifies the fixed observer against analytic
0.09375/0.1875dB staircases at three carrier phases plus zero output. The
`routes` group expands automatic CSM to every ALG7 operator/channel route and
checks four manually held operator routes while enabling CSM. These use the
public register-slot offsets0/8/16/24 and distinct manual key masks8/32/16/64.
Both groups are included in `all`.

The additional `noise` group addresses jt51#16's maximum-off observation,
independently audited in the parent jtcores report
`tasks/jt51-16/plans/offline-dispositions.md` and tracked in
[jt51#16](http://gitea.jotego.es/jotego/jt51/issues/16)
and originally documented in `tasks/jtcores-143/plans/noise-measurements.md`.
It requires exact-zero NE-on output after never-keyed and released histories,
while preserving bipolar keyed-noise positives and the exact keyed TL127
−9/+8PCM endpoints. The modeled0.5s wait and16,384-frame observation are finite
controls; they do not infer silicon's internal envelope values. This group is
also included in `all`.

`slowhold` adds a documentation control for the scope of the low-rate release
correction. Yamaha's table describes arithmetic RATE0–3 as infinite for
attack/decay. AR1/D1R1/D2R1 at KS0/KC00 and KS3/KC04 (RATE2/3) must therefore
remain off/held during the finite65,536-native-frame window. These particular
programs are documentary expectations, **not claimed physical measurements**.
A held sine's first/last8192-frame raw peak may differ by at most16PCM to allow
quantization and carrier-phase coverage; the output must remain bipolar/full
level. The directly measured nonzero low-RR0 release expectations remain
unchanged and distinct. This group is included in `all`.

`csm_states` expands the physically measured state controls: CSM off while
Timer A runs, CSM on with neither timer running, Timer B alone, enabling CSM on
an already-running Timer A, AR30/31 siblings, enabled status-flag creation, and
quiet output after disabling CSM while A keeps running or stopping A while
CSM stays set. It is included in `all`. The maximum Timer-A preload1023 is
already exercised by every measured/documented flag-control case in `timer`;
no unqualified exact automatic waveform claim is made for its one-frame period.

`mixing`, also included in `all`, preserves the prospectively frozen raw-output
method used for jt51#16's independently initialized native-record confirmations.
Literal ALG7/FB0, KC4A/KF0, MUL0, AR31/KS0, D1/D2=0, noise/LFO/CSM-off images
key N5 or N32 operators once per history. Only pan/TL registers change during
component exposure. RIGHT retains channel1 M1 (N5) or channel0 (N32), while LEFT
exposes each channel at TL32, then TL0, followed by aggregate and low-return
controls. Native16384-frame records use179000 master clocks of settling,
128-frame guards,8192 training samples and4096 untouched heldout samples.

Raw RIGHT bins are2048PCM wide, with separate strictly monotonic rising/falling
branches. Adjacent-bin component min/max hulls require complete observed-domain
bin support and at least3 samples, with64PCM component and aggregate output
quantization allowances. Each heldout, aggregate and low-return assessment must
have zero missing support, at least75% monotonic covered samples and at least3
points in every positive/negative RIGHT × rising/falling quadrant. Ambiguous
turnpoints remain outside the conditional subset; no phase/gain fit or internal
tap is used. Endpoint-censored components fail qualification.

The frozen physical expectation is signed18-bit modular wrapping before16-bit
clipping and unchanged DAC quantization. TL32 component closure, initial/return
history controls, N5 clipping positives and exact-zero cleanup must also pass.
N32 must distinguish the18-bit map from clip16/wrap17/wrap19 on both observed
output signs. Signed wrap discontinuities retain their disjoint image intervals.
This is the measured finite external transfer, not a claim of internal adder
topology or complete-chip conformance. Source evidence and retained failed
cosine/harmonic/short-hull methods are recorded in the parent task report
`/nobackup/tasks/jt51-16/plans/round2-20261018-mixing/production-requirement.md`.

`mixing_mirror` adds the independently measured RIGHT overflow control. It uses
the same N32 history, swaps only literal PAN bits64/128 and swaps public output
columns before the unchanged raw-bin observer. Timing, quantization allowances,
coverage and expected18-bit map remain identical. Both groups run in `all`; the
mirrored supplement protects against a correction applied to only one output.

`nominal_pitch` adds five public-output checks from Yamaha application notes
PDF page 7, section 2.1.2: KC4A, KF0, MUL1 and DT1=DT2=PMS=0 produce nominal
440 Hz at a 3.579545 MHz master clock. Both outputs must be bipolar beyond 1000
PCM, have at least three rising crossings, and measure 439–441 Hz. A 4096-frame
exact stereo-zero check follows KEYOFF and settling. This documentary
expectation is independent of the anomalous near-static output observed on the
measured PCB.

`stereo_age` and `stereo_age_analysis` add 183 checks. Public-pin stereo age
regression from the independently bound 96 saved captures covering every
operator/channel in three repeats. The 32 full INIT write sequences match
stereo-age-map-v1 canonical fixtures exactly. No physical files are needed at
runtime. Reset each case, initialize one keyed operator, settle 1790000 master
clocks (0.5 seconds), capture 2048 frames at stride8. Fit the fixed 32992/2^20
retained carrier over [128,1920), amplitude≥32, bipolar, relative RMS≤.003.
Each output fits independently; raw wrapped LEFT−RIGHT phase must be within
.001 radians of −one native step at slots15..30, zero elsewhere. Register order
is M1,M2,C1,C2; key bits are08,20,10,40. No per-case offset or frequency fit.
Early/late1000-point windows overlap208 and test stability only. Every case
ends with a whole2048-frame stereo quiet control after keyoff and0.5-second
settle. Analytic signed0/±1, other-age, quiet, changed return, minimum window
and wrong-carrier controls qualify the kernel. 160 physical-route checks plus23
analytic checks =183 total.
