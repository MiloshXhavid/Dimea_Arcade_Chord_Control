---
phase: 13-processblock-integration-and-apvts
plan: 01
subsystem: dsp
tags: [lfo, apvts, processblock, juce, midi, vst3, chord-engine]

# Dependency graph
requires:
  - phase: 12-lfo-engine-core
    provides: LfoEngine.h + LfoEngine.cpp (standalone DSP, 15 Catch2 tests)
provides:
  - LfoEngine members lfoX_/lfoY_ wired into PluginProcessor
  - 16 APVTS LFO parameters (enabled, waveform, rate, level, phase, distortion, sync, subdiv for X and Y)
  - processBlock LFO injection: additive clamped offset on chordP.joystickX/Y with ~10ms ramp
  - Phase reset on prepareToPlay, DAW transport stop, and DAW transport start
affects:
  - phase-14-lfo-ui-beat-clock
  - any future preset loading (LFO defaults apply to v1.3 presets automatically)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - LFO bypass via ramp-to-zero (lfoXRampOut_/lfoYRampOut_ with rampCoeff = blockSize / (sampleRate * 0.01))
    - Non-const ChordEngine::Params allows post-buildChordParams() modification before pitch compute
    - LFO disabled path: phase does not advance when enabled=false or level=0
    - Skew factor computed manually as log(0.5)/log((mid-start)/(end-start)) — JUCE 8.0.4 NormalisableRange lacks getSkewFactorFromMidPoint()

key-files:
  created: []
  modified:
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp

key-decisions:
  - "chordP declared non-const so LFO additive offset can be applied to joystickX/Y after buildChordParams()"
  - "LFO output applied as additive offset to local chordP only — joystickX/Y atomics never modified (prevents looper conflict)"
  - "Auto helper variables used instead of immediately-invoked lambda — avoids MSVC lambda quirks in processBlock scope"
  - "Static constexpr kLfoSubdivBeats[6] array placed as static local in processBlock scope (not anonymous namespace) — cleaner scoping for audio-thread-only data"
  - "Skew factor 0.2306f hard-coded — JUCE 8.0.4 NormalisableRange does not expose getSkewFactorFromMidPoint(); value derived analytically: log(0.5)/log((1.0-0.01)/(20.0-0.01))"
  - "Joystick-source voice retriggering with LFO active is intentional — LFO varies pitch at next trigger boundary via post-LFO chordP values driving TriggerSystem deltaX/deltaY"

patterns-established:
  - "processBlock LFO injection order: buildChordParams() -> LFO block modifies chordP.joystickX/Y -> freshPitches computed from modified chordP"
  - "Transport reset pattern: both justStopped and dawJustStarted reset lfoX_/lfoY_ + zero ramp state"

requirements-completed: [LFO-01, LFO-02, LFO-08, LFO-09, LFO-10]

# Metrics
duration: 25min
completed: 2026-02-26
---

# Phase 13 Plan 01: processBlock Integration and APVTS Summary

**LfoEngine wired into PluginProcessor via 16 APVTS params + additive clamped joystick offset with 10ms ramp, transport phase-reset, and full backward compatibility with v1.3 presets**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-02-26T04:08Z
- **Completed:** 2026-02-26T04:33Z
- **Tasks:** 2/2
- **Files modified:** 2

## Accomplishments
- Added `LfoEngine lfoX_` and `LfoEngine lfoY_` members to `PluginProcessor` with per-axis ramp state (`lfoXRampOut_`, `lfoYRampOut_`)
- Defined all 16 LFO APVTS parameters in `createParameterLayout()`: enabled booleans, waveform choices (7 options), log-scale rate (0.01–20 Hz, skew 0.2306), level (0–2), phase (0–360°), distortion (0–1), sync toggle, subdivision choices (6 options)
- Injected LFO processing block in `processBlock()` after `buildChordParams()`: reads APVTS params per axis, calls `lfoX_.process()` / `lfoY_.process()`, applies ~10ms linear ramp, clamps additive offset to `chordP.joystickX/Y`
- Added `lfoX_.reset()` / `lfoY_.reset()` + ramp clear in `prepareToPlay()`, on DAW transport stop (`justStopped`), and on DAW transport start (`dawJustStarted`)
- 15/15 LfoEngine Catch2 tests pass (2222 assertions) — no regressions from LFO wiring

## Task Commits

Each task was committed atomically:

1. **Task 1: Add LFO members + 16 APVTS parameters** - `1867574` (feat)
2. **Task 2: Wire LFO into processBlock + transport resets** - `56f0108` (feat)

**Plan metadata:** (docs commit — see below)

## Files Created/Modified
- `Source/PluginProcessor.h` - Added `#include "LfoEngine.h"`, `lfoX_`/`lfoY_` members, `lfoXRampOut_`/`lfoYRampOut_` ramp state
- `Source/PluginProcessor.cpp` - Added 16 ParamID constants, LFO section in `createParameterLayout()`, LFO injection block in `processBlock()`, resets in `prepareToPlay()` and transport handlers

## Decisions Made

**chordP non-const:** Changed `const ChordEngine::Params chordP = buildChordParams()` to non-const so the LFO block can write to `chordP.joystickX` and `chordP.joystickY` before pitch computation. The modification is scoped to `processBlock()` and never propagated to the shared atomics, preventing looper conflicts.

**Auto helper variables vs immediately-invoked lambda:** The plan suggested an immediately-invoked lambda (`[&]() -> float { ... }()`). This was replaced with straightforward `if (enabled && level > 0) { ... xTarget = lfoX_.process(xp); }` blocks. Both are valid C++17; the if-block form is cleaner and avoids any MSVC lambda capture peculiarities in processBlock scope.

**Static constexpr placement:** `kLfoSubdivBeats[6]` declared as `static constexpr double` inside the processBlock body (before the LFO injection block). This is a static local — initialized once, stored in read-only memory, zero runtime overhead.

**Skew factor 0.2306f:** `juce::NormalisableRange<float>::getSkewFactorFromMidPoint()` does not exist in JUCE 8.0.4. The skew factor was computed analytically: `skew = log(0.5) / log((midpoint - start) / (end - start)) = log(0.5) / log((1.0 - 0.01) / (20.0 - 0.01)) ≈ 0.2306`. At normalized 0.5, this yields `0.01 + 19.99 * 0.5^(1/0.2306) ≈ 1.0 Hz` — exactly the intended slider midpoint.

**Joystick-source voice retriggering:** When LFO X/Y modulation is active, `chordP.joystickX/Y` varies per block. TriggerSystem reads `deltaX = chordP.joystickX - prevJoyX_` to detect joystick motion for JOY-trigger voices. This means LFO modulation will retrigger joystick-source voices at LFO rate. This is intentional design (LFO drives pitch movement which drives retrigger). Documented as known behavior.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] getSkewFactorFromMidPoint() does not exist in JUCE 8.0.4**
- **Found during:** Task 1 (build verification)
- **Issue:** Plan specified `juce::NormalisableRange<float>(0.01f, 20.0f).getSkewFactorFromMidPoint(1.0f)` to set log-scale rate slider. JUCE 8.0.4's `NormalisableRange<T>` does not expose this static helper — compiler error C2039.
- **Fix:** Computed the skew factor analytically (`0.2306f`) and passed it directly as the 4th constructor argument. Produces identical slider behavior (midpoint = 1 Hz).
- **Files modified:** `Source/PluginProcessor.cpp`
- **Verification:** Build succeeds with 0 C++ errors. Rate slider midpoint math verified analytically.
- **Committed in:** `1867574` (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 — API mismatch)
**Impact on plan:** Trivial API adaptation, no behavior change. Slider skew is mathematically equivalent to what getSkewFactorFromMidPoint(1.0f) would have returned.

## Issues Encountered
- Post-build copy step fails with MSB3073 (copying VST3 to `C:\Program Files\Common Files\VST3` requires admin). This is a pre-existing known issue (CLAUDE.md / fix-install.ps1). The compiled VST3 binary at `build/ChordJoystick_artefacts/Release/VST3/Dima_Chord_Joy_MK2.vst3/Contents/x86_64-win/Dima_Chord_Joy_MK2.vst3` is fully built and valid.

## Build Verification
- `cmake --build build --config Release` — 0 C++ compilation errors, 0 linker errors
- VST3 binary produced: `Dima_Chord_Joy_MK2.vst3` (4,968,448 bytes, Feb 26 04:10)
- LfoEngine tests: `ChordJoystickTests.exe [lfo]` — **15/15 pass, 2222 assertions**

## Next Phase Readiness
- LFO DSP is fully wired and parameterized — Phase 14 can add UI panels (sliders, waveform selector, sync button) and wire them to the 16 new APVTS parameters
- DAW automation lane will show all 16 parameters once loaded in a DAW (no DAW-side steps needed)
- Backward compatibility: loading a v1.3 preset (XML without lfo* keys) results in all LFO defaults (enabled=false, level=0) — zero modulation, plugin behaves identically to v1.3

---
*Phase: 13-processblock-integration-and-apvts*
*Completed: 2026-02-26*
