---
phase: 28-random-free-redesign
plan: 01
subsystem: audio
tags: [cpp, juce, midi, trigger, random, poisson, burst]

# Dependency graph
requires:
  - phase: 27-triplet-subdivisions
    provides: RandomSubdiv enum with triplet values (WholeT..ThirtySecondT) that burst drain reads via subdivBeatsFor()
provides:
  - Poisson exponential countdown clock for RND SYNC OFF (truly stochastic inter-onset intervals)
  - Three-branch sync matrix: Poisson / DAW-ppq / internal-free-tempo
  - Burst semantics for RandomFree and RandomHold: N = round(probability * 64) notes per trigger event
  - burstNotesRemaining_ and burstPhase_ state arrays with full reset coverage
affects: [29-looper-perimeter-bar, 30-distribution]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Poisson exponential wait draw inlined at fire site using LCG nextRandom() (inverse-CDF method)
    - Burst drain pattern: set burstNotesRemaining_ + burstPhase_=0 on trigger; decrement per block via burstPhase_ countdown
    - Three-branch clock: !randomClockSync (Poisson) / isDawPlaying+ppq>=0 (grid) / else (internal counter)
    - Reset burst state on: transport-stop, mode-switch, resetAllGates(), pad-release in RandomHold

key-files:
  created: []
  modified:
    - Source/TriggerSystem.h
    - Source/TriggerSystem.cpp

key-decisions:
  - "Probability (0-1.0) maps to burst size N = round(probability * 64): 0%=silence, 100%=64 notes per burst event"
  - "Population is Poisson rate lambda for SYNC OFF; slot-selection probability via hitsPerBarToProbability only for SYNC ON"
  - "Poisson wait inlined at call site rather than extracted to static helper — simpler, no signature complexity"
  - "burstPhase_ counts DOWN toward 0 in SYNC OFF Poisson path (same pattern used for burst drain spacing)"
  - "effectiveBpm = randomFreeTempo when !randomClockSync OR !isDawPlaying — ensures internal-clock third branch uses free tempo"

patterns-established:
  - "Burst drain: burstPhase_[v] -= blockSize; fire when <= 0; reschedule at samplesPerSubdiv spacing"
  - "Poisson draw: u = max(1e-6, nextRandom()); wait = -mean * log(u); clamp to 10ms floor"

requirements-completed: [RND-08, RND-09, RND-10]

# Metrics
duration: 2min
completed: 2026-03-03
---

# Phase 28 Plan 01: Random Free Redesign Summary

**Poisson exponential clock + 64-note burst model replaces periodic grid in RND SYNC OFF, three-branch sync matrix governs all random timing modes**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-03T01:05:03Z
- **Completed:** 2026-03-03T01:07:12Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- TriggerSystem.h gains `burstNotesRemaining_` and `burstPhase_` arrays with full reset coverage (resetAllGates, transport-stop, mode-switch)
- Clock block restructured to three branches: Poisson countdown (SYNC OFF), DAW ppq grid (SYNC ON + playing), internal free-tempo counter (SYNC ON + stopped)
- RandomFree and RandomHold branches rewritten: burst of N notes (N = round(probability * 64)) per trigger event, drained one per block via burstPhase_ countdown
- Build succeeds with 0 errors; VST3 installed to `%COMMONPROGRAMFILES%\VST3`

## Task Commits

Each task was committed atomically:

1. **Task 1: Add burst state arrays to TriggerSystem.h and update resetAllGates()** - `191193a` (feat)
2. **Task 2: Restructure clock block and rewrite RandomFree/RandomHold branches** - `a3627d6` (feat)

**Plan metadata:** (docs commit — see below)

## Files Created/Modified
- `Source/TriggerSystem.h` - Added `burstNotesRemaining_` and `burstPhase_` private arrays after `randomGateRemaining_`
- `Source/TriggerSystem.cpp` - Three-branch clock block, rewritten RandomFree/RandomHold branches, reset logic for burst state

## Decisions Made
- Probability (0–1.0) maps to burst size N = round(probability * 64): 0% = silence, 100% = up to 64 rapid re-triggers per burst event
- Population is the Poisson rate lambda (events/bar) for SYNC OFF; `hitsPerBarToProbability()` is only called for SYNC ON slot-selection gating
- Poisson wait inlined at fire site rather than extracted to a static free function — avoids signature complexity with the private `rng_` member
- `burstPhase_` counts DOWN toward 0 in both the Poisson path and burst drain path (consistent direction)
- `effectiveBpm` formula updated: use `randomFreeTempo` whenever `!randomClockSync || !isDawPlaying` — ensures the internal-clock third branch uses free tempo, not stale DAW BPM

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None. Build succeeded on first attempt with 0 compiler errors or warnings introduced by these changes.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Phase 28 Plan 01 complete. RandomFree/RandomHold now use Poisson timing and burst semantics.
- Phase 29 (Looper Perimeter Bar) is independent of Phase 28 and can start now.
- Phase 28 has no further plans — phase complete after this metadata commit.

---
*Phase: 28-random-free-redesign*
*Completed: 2026-03-03*
