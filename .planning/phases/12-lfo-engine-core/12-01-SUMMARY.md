---
phase: 12-lfo-engine-core
plan: 01
subsystem: dsp
tags: [lfo, dsp, lcg, waveform, sync, free-mode, midi-effect, cpp17]

# Dependency graph
requires: []
provides:
  - LfoEngine class with 7 waveforms (Sine, Triangle, SawUp, SawDown, Square, S&H, Random)
  - Dual timing modes: DAW-sync (ppqPos-derived) and free (rateHz accumulator)
  - LCG random engine per-instance, maps to [-1, +1] via signed int32 cast
  - S&H boundary detection using prevCycle_ sentinel + integer cycle index
  - Additive LCG distortion post-waveform (bypassed for Random waveform)
  - ProcessParams struct for zero-allocation audio-thread calling convention
  - reset() for prepareToPlay() and transport-stop integration
affects:
  - Phase 13 (processBlock + APVTS wiring)
  - Phase 14 (LFO UI — ProcessParams fields map directly to APVTS parameters)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Standalone DSP class with no JUCE/APVTS dependency — tested in isolation before wiring
    - LCG via rng_*1664525u+1013904223u with signed int32 / 0x7FFFFFFF for bipolar output
    - ProcessParams by-value struct passed to audio-thread method (no shared mutable state)
    - prevCycle_ = -1 sentinel pattern for guaranteed first-block S&H initialization
    - totalCycles_ accumulator for free-mode cycle tracking without dividing by sample rate per block

key-files:
  created:
    - Source/LfoEngine.h
    - Source/LfoEngine.cpp
  modified: []

key-decisions:
  - "LfoEngine is fully standalone — no JUCE, no LooperEngine, no APVTS headers in .h or .cpp; kTwoPi defined locally"
  - "nextLcg() maps via static_cast<int32_t>(rng_) / 0x7FFFFFFF (bipolar), unlike TriggerSystem::nextRandom() which uses rng_>>1 / 0x7FFFFFFF (unipolar [0,1))"
  - "evaluateWaveform() is non-const because Random waveform calls nextLcg() which mutates rng_"
  - "freeModeCycleSamples computed but suppressed with (void) cast — left as documentation for future use"
  - "Branch B (sync+stopped) continues motion via sampleCount_ so LFO does not freeze when DAW is paused"

patterns-established:
  - "DSP isolation pattern: build engine class standalone in one phase, wire into processBlock in next phase"
  - "Phase branch structure for timing: sync+playing (ppqPos), sync+stopped (sampleCount_), free (phase_ accumulator)"
  - "S&H uses prevCycle_=-1 sentinel so first process() call always latches a non-zero value"

requirements-completed: [LFO-03, LFO-04, LFO-05, LFO-06, LFO-07, PERF-01, PERF-02, PERF-03]

# Metrics
duration: 12min
completed: 2026-02-26
---

# Phase 12 Plan 01: LFO Engine Core Summary

**Audio-thread-safe LfoEngine with 7 waveforms, dual timing (DAW-sync + free), per-instance LCG RNG, S&H boundary detection, and additive distortion — zero JUCE/APVTS dependencies**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-02-26T02:35:31Z
- **Completed:** 2026-02-26T02:47Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Created standalone LfoEngine.h with `enum class Waveform` (7 values), `struct ProcessParams` (all fields with defaults), and complete class declaration including inline `nextLcg()` with correct signed-int32 bipolar mapping
- Created LfoEngine.cpp with `process()` implementing three timing branches (sync+DAW-playing via ppqPos, sync+DAW-stopped via sampleCount_, free via phase_ accumulator), S&H boundary detection, phase shift, distortion, and level application
- Implemented all 7 waveforms in `evaluateWaveform()`: Sine (std::sin), Triangle (piecewise linear), SawUp, SawDown, Square (50% duty), S&H (shHeld_ constant-hold), Random (per-call LCG noise)
- Ensured Random waveform bypasses distortion noise (already pure noise per CONTEXT.md), and distortion=0 produces exact clean output

## Task Commits

Each task was committed atomically:

1. **Task 1: Create LfoEngine.h** - `50a1143` (feat)
2. **Task 2: Create LfoEngine.cpp** - `04be91f` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified

- `Source/LfoEngine.h` - Waveform enum, ProcessParams struct, LfoEngine class declaration with inline nextLcg()
- `Source/LfoEngine.cpp` - process(), reset(), evaluateWaveform() with all 7 waveforms

## Decisions Made

- Used `static constexpr float kTwoPi = 6.28318530718f` locally in .cpp instead of including any JUCE header, keeping the translation unit fully standalone for isolated testing
- Made `evaluateWaveform()` non-const (rather than `const_cast<LfoEngine*>(this)->nextLcg()` as suggested in the plan) — cleaner and avoids undefined behavior from casting away constness on a mutating method
- Kept `freeModeCycleSamples` computed but silenced with `(void)` cast — preserves intent documentation for future wiring while eliminating compiler warnings
- Transport restart detection placed before phase derivation branches so `sampleCount_` is correctly reset before Branch B reads it

## Deviations from Plan

None - plan executed exactly as written, with one minor improvement: `evaluateWaveform()` declared non-const directly (rather than using `const_cast` as the plan suggested as a workaround). The plan itself noted "simplest fix: make evaluateWaveform non-const" — this was the chosen path.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `LfoEngine.h` and `LfoEngine.cpp` ready for Phase 13 integration into `processBlock()`
- `ProcessParams` fields map directly to planned APVTS parameters (rateHz, subdivBeats, syncMode, waveform, phaseShift, distortion, level, maxCycleBeats)
- Phase 13 will need to construct `ProcessParams` from APVTS parameter state and pass `ppqPosition` / `isDawPlaying` from `AudioPlayHead`
- Concern from STATE.md still active: TriggerSystem delta behavior with LFO active — LFO-modulated joystickX/Y will drive deltaX/deltaY and may retrigger joystick-source voices; confirm intent before Phase 13 finalizes

---
*Phase: 12-lfo-engine-core*
*Completed: 2026-02-26*

## Self-Check: PASSED

- FOUND: Source/LfoEngine.h
- FOUND: Source/LfoEngine.cpp
- FOUND: .planning/phases/12-lfo-engine-core/12-01-SUMMARY.md
- FOUND commit 50a1143 (feat: LfoEngine.h)
- FOUND commit 04be91f (feat: LfoEngine.cpp)
