# Sampling Contract

This file is the **single source of truth for sampling cadence** across the
OpenSmell hardware and software. Every firmware variant, the code generator, and
every downstream consumer of per-second features reads its rate from here or
from a value the device declares on the wire. If you change a cadence, change it
here first.

Sampling rate is treated in the paper as a **fourth device constant** alongside
$V_{cc}$, $R_L$, and $R_0$ (paper §5.4). Every temporal feature divides a sample
count by the rate, so an undeclared or wrong rate silently rescales rise time,
decay time, latency, and hysteresis by the corresponding factor.

## Nominal cadence

| Collector | Nominal period | Nominal rate | Source of truth |
|---|---|---|---|
| `electronic-nose/firmware` (`src/main.cpp`, `variants/ble_main.cpp`) | `OSMOGRAPH_SAMPLE_INTERVAL_MS`, default 500 ms | 2 Hz | build flag in `firmware/platformio.ini` |
| `Osmograph/board/compiler.py` (generated firmware) | `PRINT_INTERVAL`, 500 ms | 2 Hz | `board/compiler.py` |
| `osmograph-desktop` ingest | real clock `now_secs()` | measured per sample | `advance_sample_gate` clamps dt to `[1e-4, 60]` s |

Measured on the reference recordings (`~/Osmograph_Recordings/*.csv`): median
`timestamp_ms` gap 472.5–535.5 ms across 13/13 files (1.87–2.12 Hz), consistent
with the 500 ms nominal and independently confirmed by row count against
recording duration.

### Overriding the cadence

Both firmware paths take the interval as a build flag, never as an inline
literal:

```bash
# 10 Hz build
pio run -e esp32-wifi --project-option="build_flags=-DOSMOGRAPH_SAMPLE_INTERVAL_MS=100"
```

A `static_assert` rejects intervals below 10 ms, which sequential ADC reads
across six channels cannot sustain.

## The cadence is declared on the wire

At boot each firmware emits its own interval, so a host never has to assume:

```
INFO,universal-esp32,1.0.0,6,interval_ms=500
```

Hosts should parse `interval_ms` from this line and record it as
`sensor.samplingRateHz` in the `.osmell` manifest. A host that falls back to a
hardcoded rate should mark the recording's provenance accordingly — see rule 1.

## Hard rules

1. **Never treat row index as seconds.** If a file carries no time column, mark
   it `synthetic` and use a named reference rate constant
   (`DEFAULT_SYNTHETIC_RATE_HZ` in the web twin, `SMELLNET_SR` / `RIG_SR` in the
   Python research code) — never an inline magic `10`.
2. **Per-second features divide by the true rate.** Slope is `Δy/Δt`, AUC is
   `∫y dt`, latency/rise/decay are `samples / sr`, and the adsorption/desorption
   path ratio is `(∫y dt)_ads / (∫y dt)_des` so that hysteresis is a
   dimensionless time integral rather than a sample count.
3. **Threshold crossings are interpolated.** Take the crossing time by linear
   interpolation between the bracketing samples, not at the first sample past
   the threshold. Measured on synthetic exposures with known analytic rise
   times: naive first-sample estimation gives 1.47% mean / 8.98% worst-case
   error, interpolation gives 0.56% / 1.63%.
4. **dt-aware filters use `exp(-Δt/τ)` / `1-(1-α)^(Δt/T)` semantics**
   (see `opensmell-rs/src/anomaly/ewma.rs::update_with_dt`), not per-step α/q.
   The Rust `FailSafeSystem` advances by the measured `dt_s`.
5. **R0/recovery baselines are durations, not sample counts.** Derive sample
   counts from `duration_s * sr` (`r0_samples_for()` in
   `interoperability/canonical_experiments/config.py`). A fixed 15 samples means
   7.5 s at 2 Hz but 1.5 s at 10 Hz. The resolution order for a recorded window
   is specified in [the R0 window contract](#the-r0-window-contract) below.
6. **Validate the time column's unit.** The time column has no intrinsic unit.
   If the observed median gap is far from the period implied by the declared
   rate, report a unit mismatch rather than a gap statistic — otherwise a file
   timestamped in seconds produces a continuity score of zero that is
   indistinguishable from real packet loss. `opensmell` implements this as
   `flags.time_unit_mismatch` with reason `time_unit_mismatch`.

## The R0 window contract

Rule 5 says what an R0 baseline *is* — a duration. It does not say how long, and
there are two different jobs, so the window is resolved in this order.

**1. A declared window wins verbatim.** `manifest.baseline.r0Samples`,
`baseline.r0_samples` in a preset, `r0_samples` in trained-model metadata, or an
explicit `r0_samples` argument. Whoever declares a window owns the
duration-to-count conversion, which is rule 5: `round(duration_s * sr)`.
`opensmell/opensmell/presets.py::BaselineSpec.resolve_r0_samples` and
`r0_samples_for()` implement it, and `resolve_r0_samples` is validated at load
time against `duration_s * rate`. These are the 18000-sample figures in
`opensmell/README.md` — `DEFAULT_BASELINE_SECONDS = 1800` at 10 Hz, the 30-minute
clean-air collection in `HARDWARE.md` §Baseline Collection.

**2. With no declared window, the default is a floored, capped fraction of the
recording:**

```
n = clamp(floor(0.15 * N), 5, 30)
```

`N` is the sample count of the window being reduced, and `n` is the number of
leading samples whose median becomes R0. All three constants are named
(`R0_WINDOW_FRACTION`, `R0_WINDOW_MIN_SAMPLES`, `R0_WINDOW_MAX_SAMPLES` in
Python `opensmell/opensmell/types.py`, `opensmell-js/src/types.ts`,
`opensmell-rs/src/lib.rs`).

**Cadence.** The default window is a fraction of a recording, not a fixed sample
count, so for a recording of a given duration it is cadence-independent: `N = T *
fs` grows with the rate, so `0.15 * N` samples is `0.15 * T` *seconds* whether the
recording was made at 1, 2, 10 or 100 Hz. That is the same invariance rule 5
demands, obtained without needing a declared rate — which matters because the
auto-R0 path exists precisely for recordings that have no separate baseline
session and therefore no trusted `sr`. The superseded fixed 15-sample default was
cadence-*dependent*: 1.5 s at 10 Hz against 15 s at 1 Hz, a 10x rescale of R0 and
of every feature divided by it.

Two things this does *not* claim, both stated by the tests:

- The clamps are sample counts, so outside the fraction region they are
  cadence-*dependent*. A 300 s recording at 1 Hz resolves to 30 samples and
  therefore spans 30 s, while the same recording at 100 Hz spans 0.3 s.
- The fraction cannot recover a baseline shorter than `0.15 * T`. If the
  clean-air plateau is shorter than the resolved window, R0 reads the response
  ramp — consistently across all three SDKs, but wrong. Only a declared window
  fixes that, which is what rule 5 is for.

| Bound | Binds when | Why |
|---|---|---|
| `min 5` | `N < 34` | below ~5 samples the median is one or two readings and a single ADC LSB moves it by 10–20%; the baseline then spans at most `5/fs` s |
| `max 30` | `N > 200` | on a long recording an unbounded 15% would swallow the onset; the baseline must stay inside the leading plateau |

The fraction rule holds exactly for `34 <= N <= 200`; outside it, a consumer that
needs exact rule-5 invariance across rates must declare the window, and a consumer
that wants to know which bound bit can compare the resolved window against
`floor(0.15 * N)`. The SDKs do not emit a warning for it — the clamps are a
documented contract, not an anomaly to be diagnosed at runtime.

Every SDK carries this rule's tests case-for-case
(`opensmell/tests/test_r0_contract.py`, `opensmell-js/test/r0-window.test.ts`,
`opensmell-rs/tests/r0_window_contract.rs`), and
`opensmell-rs/examples/r0_probe.rs` prints the resolved window, R0, and
`sensitivity_decay` as CSV for one input across four cadences and four durations,
so a Python or JS run of the same loop can be diffed against it byte-for-byte.

`0.15` is the fraction already in `HARDWARE.md`
(`int cutoff = bl.sample_count * 0.15;`) and in
`data-commons/docs/wire-protocol.md` ("computes R0 (median of first 15%)"), so the
default follows the device-side documents rather than inventing a third number.
At the canonical `DEFAULT_WINDOW_SIZE = 100` / `WINDOW_SIZE = 100` the rule
evaluates to `clamp(15, 5, 30) = 15`, so the 100-sample framework vector is
bit-identical to the fixed-15 default it replaces — no published figure moves.

**One divergence, on purpose.** `HARDWARE.md` §Baseline Collection and
`wire-protocol.md` §Calibration Sequence apply the 15% to a *device calibration
collection* whose length they fix separately at 30 min (1800 s). Those two
statements contradict each other: 15% of 1800 s is 270 s, while the generated
firmware collects `BASELINE_SECONDS 30`
(`opensmell-rs/src/protocol/mod.rs`). The fraction is correct for reducing an
arbitrary recorded window and wrong for a calibration session of known length,
where rule 5's `duration_s * sr` applies and the duration is the authority. So:
device calibration uses the declared duration; windowed auto-R0 uses the floored
fraction. `NEW_SENSOR_WARMUP_SECONDS = 172800` (48 h) is a warm-up, not a
baseline, and is never an R0 window.

## Plausibility gate for ingestion

A consumer that receives a recording with no usable time axis should reject or
explicitly downgrade it rather than proceed. The checks, in order:

| Check | Condition | Action |
|---|---|---|
| Time column present | missing | flag `no_time_axis`; require an explicit synthetic rate |
| Median gap vs declared rate | ratio outside 0.5×–2× | flag `time_unit_mismatch`; withhold continuity |
| Implied duration | `< 1 s` or `> 24 h` | flag `implausible_duration`; do not compute temporal features |
| Sample count vs duration | inconsistent by >10% | flag `count_duration_mismatch` |
| Monotonic timestamps | any negative step | flag `unsorted_rows`; sort and record that it was sorted |

The reference August recordings fail several of these: no timestamp column, and
`session_index` durations of 0.00–0.11 s for 600–660 row files, which implies
rates near 6000 Hz and cannot be physical. Their true cadence is unrecoverable
from the files; they are retained as a timing test fixture, not as kinetics data.

## Sequential ADC skew

The ESP32 reads six ADC1 channels in a loop, so samples within one reported frame
are not simultaneous. Measured skew is roughly 1–12 ms, negligible against the
500 ms sample period and against second-scale MOX chemistry. It is *not*
negligible for cross-channel coherence during sharp common-mode events:

- Record the scan order if the host needs to de-skew.
- Restrict coherence-based anomaly tests to quasi-steady windows.
- Report the minimum detectable anomaly duration: an event shorter than about
  `2 Δt` cannot be resolved from the sample sequence at all.