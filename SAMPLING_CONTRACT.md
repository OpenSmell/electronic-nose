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
   7.5 s at 2 Hz but 1.5 s at 10 Hz.
6. **Validate the time column's unit.** The time column has no intrinsic unit.
   If the observed median gap is far from the period implied by the declared
   rate, report a unit mismatch rather than a gap statistic — otherwise a file
   timestamped in seconds produces a continuity score of zero that is
   indistinguishable from real packet loss. `opensmell` implements this as
   `flags.time_unit_mismatch` with reason `time_unit_mismatch`.

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