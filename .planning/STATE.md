# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-26)

**Core value:** XY joystick mapped to harmonic space — per-note trigger gates, scale quantization, gesture looper with trigger quantization, gamepad control — no competitor provides this as a unified instrument.
**Current focus:** v1.4 LFO + Clock — Phase 12 Plan 02 complete

## Current Position

Phase: 12 of 15 (LFO Engine Core)
Plan: 2 of ? (Plan 02 complete)
Status: In progress
Last activity: 2026-02-26 — Phase 12 Plan 02 complete (LfoEngine Catch2 test suite: 15 TEST_CASEs, all passing)

```
v1.0 MVP    [██████████] SHIPPED 2026-02-23
v1.3 Polish [██████████] SHIPPED 2026-02-25
  Phase 09  [██████████]   MIDI Panic            Complete (2/2 plans)
  Phase 10  [██████████]   Trigger Quantization  Complete (5/5 plans)
  Phase 11  [██████████]   UI Polish + Installer Complete (4/4 plans)
v1.4 LFO    [████░░░░░░] ~20% — Phase 12 in progress
  Phase 12  [████░░░░░░]   LFO Engine Core       In progress (2/? plans done)
  Phase 13  [░░░░░░░░░░]   processBlock + APVTS  Not started
  Phase 14  [░░░░░░░░░░]   LFO UI + Beat Clock   Not started
  Phase 15  [░░░░░░░░░░]   Distribution          Not started
  Phase 16  [░░░░░░░░░░]   Gamepad Preset Ctrl   Not started
```

## Accumulated Context

### Decisions

See .planning/PROJECT.md Key Decisions table — full log with outcomes.

Recent decisions affecting v1.4:
- LFO output applied as additive offset inside buildChordParams() local scope — never written to joystickX/Y atomics (prevents looper conflict)
- S&H/Random waveforms use project LCG (`seed * 1664525u + 1013904223u`), not std::rand() — audio-thread safe on MSVC
- Sync mode derives phase from ppqPos when DAW is playing; uses sample counter only when DAW is stopped — prevents drift
- No second timer for LFO UI — all display reads in existing 30 Hz timerCallback
- LfoEngine is fully standalone (no JUCE/APVTS/LooperEngine headers) — tested in isolation before wiring into processBlock
- nextLcg() maps via static_cast<int32_t>(rng_) / 0x7FFFFFFF for bipolar [-1,+1] output (differs from TriggerSystem::nextRandom() which uses rng_>>1 for unipolar [0,1))
- evaluateWaveform() declared non-const (not const + const_cast) — Random waveform calls nextLcg() which mutates rng_
- Triangle test values match actual implementation (single-peak: trough at phi=0, peak at phi=0.5) — plan spec described a double-frequency shape that does not match LfoEngine.cpp formula
- LfoEngine.cpp requires #include <algorithm> for std::min; JUCE headers masked this in plugin build, standalone test build exposed it (auto-fixed)

### Pending Todos

None.

### Blockers/Concerns

- LFO UI layout decision needed before Phase 14 planning: where do two LFO panels fit in the space-constrained editor? Side panel vs collapsible vs tab. Decide at plan-phase 14 time.
- TriggerSystem delta behavior with LFO active: post-LFO chordP.joystickX/Y drives deltaX/deltaY — LFO motion will retrigger joystick-source voices. Confirm this is desired before Phase 13 finalizes.

## Session Continuity

Last session: 2026-02-26
Stopped at: Completed 12-02-PLAN.md (LfoEngine Catch2 tests: 15 TEST_CASEs, 2222 assertions pass, commits 3a53ec8 + e5dc795)
Next step: Continue Phase 12 or proceed to Phase 13 (processBlock + APVTS wiring)
