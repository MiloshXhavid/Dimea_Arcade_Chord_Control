# Phase 12: LFO Engine Core — Context

**Date**: 2026-02-26
**Phase Goal**: A fully tested, audio-thread-safe LFO DSP class (`LfoEngine.h/cpp`) with all 7 waveforms and performance guarantees met.

---

## Decisions

### 1. S&H vs Random Waveform Distinction

**S&H (Sample and Hold)**
- Holds one LCG-generated value for a full cycle period
- Jumps instantaneously at the cycle boundary — no interpolation
- Rate knob governs cycle duration

**Random**
- Generates a new LCG value on **every sample** (audio-rate white noise)
- **Ignores the Rate knob entirely** — rate has no effect on Random output
- Effectively true per-sample noise

**Waveform switching**: Immediate jump to the new waveform's output at its current phase — no transition smoothing in the engine.

---

### 2. Distortion Parameter Behavior

**Mechanism**: Additive LCG noise
`output = waveform_output + (distortion × lcg_noise_sample)`

- `distortion = 0` → bypass, clean waveform output (no effect)
- `distortion = 1` → waveform buried under full-amplitude LCG noise
- Output can exceed ±1 slightly at high distortion values

**RNG**: Shares the same single LCG seed as S&H and Random waveforms (one LCG chain per `LfoEngine` instance)

**Applied to**: All 7 waveforms **except Random** (Random is already pure noise — applying distortion to it is a no-op)

---

### 3. Beat-Sync Subdivisions

**Rate range in sync mode**: 1/32 note (fastest) → loopLengthBars × barDuration (slowest)

The slowest sync rate equals one full LFO cycle per looper loop. The loop length is **passed as a parameter** to `LfoEngine::process()` — the engine is not coupled directly to LooperEngine or APVTS.

**Subdivision list** (all that the engine must support internally):
- **Straight**: 1/32, 1/16, 1/8, 1/4, 1/2, 1/1, 2/1, 4/1, and up to loopLengthBars/1
- **Dotted**: 1/32., 1/16., 1/8., 1/4., 1/2.
- **Triplet**: 1/32T, 1/16T, 1/8T, 1/4T, 1/2T

The looper bar count caps the slow end dynamically — if the looper is set to 6 bars, the maximum sync subdivision = 6/1.

---

### 4. Free-Mode Rate Range

| Property | Value |
|----------|-------|
| Minimum  | 0.01 Hz (one cycle ≈ 100 seconds) |
| Maximum  | 20 Hz (upper edge of audio rate) |
| Scale    | **Logarithmic** — equal knob travel per decade |

At 20 Hz the LFO enters audible territory, enabling FM-like pitch effects. The Phase 14 slider must use a log taper.

---

## Deferred Ideas (Phase 14 requirement)

- **Joystick UI visual tracking**: When either LFO is active, the `JoystickPad` dot should visually move to reflect the LFO-modulated X/Y position, not just the physical joystick position. This is a Phase 14 rendering requirement — the LFO output values must be accessible from the editor repaint path.

---

## Summary for Researcher / Planner

Phase 12 builds `LfoEngine` as a standalone class with no APVTS or UI dependencies. Key constraints:

1. **7 waveforms**: Sine, Triangle, Saw Up, Saw Down, Square, S&H, Random
2. **Random** = per-sample LCG noise, rate-agnostic
3. **Distortion** = additive LCG noise post-waveform (not applied to Random)
4. **One shared LCG** per instance — no `std::rand()`, no CRT mutex
5. **Sync mode**: phase derived from `ppqPos`; accepts `loopLengthBars` as a `process()` parameter for max-slow subdivision
6. **Free mode**: 0.01–20 Hz, log scale
7. **Subdivisions**: straight + dotted + triplet from 1/32 to loopLengthBars
8. **Thread safety**: LFO output is a local float — no write-back to shared atomics
9. **Tests**: Catch2 unit tests for all waveforms, S&H boundary behavior, distortion bypass, sync phase reset
