---
phase: 20-random-trigger-system-extensions
plan: 01
subsystem: audio-engine
tags: [trigger-system, random-trigger, midi, gate-logic, cpp, juce]

# Dependency graph
requires:
  - phase: 19-sub-octave-per-voice
    provides: TriggerSystem infrastructure unchanged; Phase 19 established sentChannel_ and resetNoteCount() patterns

provides:
  - TriggerSource::RandomFree = 2 (renamed from Random, value preserved for DAW session compat)
  - TriggerSource::RandomHold = 3 (new: pad-held gated random firing)
  - RandomSubdiv::SixtyFourthNote = 4 (new 1/64 subdivision option)
  - ProcessParams: randomPopulation, randomProbability, gateLength (replaces randomDensity, randomGateTime)
  - Double-roll probability model in processBlock (popProb AND userProb per tick)
  - Manual open gate sentinel: randomGateRemaining_[v] = -1; countdown guard uses > 0
  - RandomHold pad-release cut: immediate note-off when pad released mid-note

affects:
  - 20-02 (PluginProcessor must forward new ProcessParams fields)
  - 20-03 (PluginEditor must add Probability knob, update combo boxes)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "-1 sentinel in randomGateRemaining_[v] for manual open gate; countdown guard uses > 0"
    - "Double-roll: two nextRandom() calls per tick — popProb roll then userProb roll — both must pass"
    - "RandomHold pad-release check before trigger evaluation — immediate note-off clears gate and resets counter"

key-files:
  created: []
  modified:
    - Source/TriggerSystem.h
    - Source/TriggerSystem.cpp

key-decisions:
  - "TriggerSource::Random renamed to RandomFree (value 2 preserved) — existing saved DAW sessions load RandomFree correctly"
  - "Double-roll model: two independent nextRandom() calls per subdivision tick; expected notes/bar = population * probability"
  - "Manual gate sentinel: gateLength <= 0.0f sets randomGateRemaining_[v] = -1; countdown guard > 0 skips sentinel so note sustains"
  - "RandomHold pad-release mid-note fires immediate note-off (overrides any active gate timer)"
  - "SixtyFourthNote = 0.0625 beats per subdivision; hitsPerBarToProbability uses 64 subdivs/bar for this case"

patterns-established:
  - "Sentinel pattern: -1 = open gate, 0 = no gate, > 0 = countdown; guard checks > 0 so -1 is transparent"
  - "RandomHold check order: pad-release note-off first, then tick+pad-held trigger — ensures note-off cannot race trigger"

requirements-completed: [RND-01, RND-02, RND-03, RND-04, RND-05, RND-06]

# Metrics
duration: 2min
completed: 2026-03-01
---

# Phase 20 Plan 01: Random Trigger System Extensions — TriggerSystem Backend Summary

**RandomHold mode, double-roll probability, 1/64 subdivision, and manual gate sentinel added to TriggerSystem.h/cpp audio-thread backend with zero UI dependencies**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-01T01:44:23Z
- **Completed:** 2026-03-01T01:46:22Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Renamed `TriggerSource::Random` to `RandomFree` (value 2 preserved — all existing DAW sessions load correctly)
- Added `TriggerSource::RandomHold = 3`: fires only while pad is held, with immediate note-off on pad release mid-note
- Added `RandomSubdiv::SixtyFourthNote = 4`: `subdivBeatsFor()` returns 0.0625, `hitsPerBarToProbability()` uses 64 subdivs/bar
- Replaced `randomDensity`/`randomGateTime` in `ProcessParams` with `randomPopulation`, `randomProbability`, `gateLength`
- Implemented double-roll model: two `nextRandom()` calls per tick in both RandomFree and RandomHold branches
- Manual gate sentinel: `gateLength <= 0.0f` sets `randomGateRemaining_[v] = -1`; countdown guard uses `> 0` so sentinel is skipped

## Task Commits

Each task was committed atomically:

1. **Task 1: Expand enums and ProcessParams in TriggerSystem.h** - `4ac2bcb` (feat)
2. **Task 2: Implement RandomHold, double-roll, sentinel, and 1/64 in TriggerSystem.cpp** - `f6cdb13` (feat)

**Plan metadata:** _(docs commit pending)_

## Files Created/Modified

- `Source/TriggerSystem.h` — Updated `TriggerSource` enum (RandomFree=2, RandomHold=3), `RandomSubdiv` enum (SixtyFourthNote=4), `ProcessParams` fields (randomPopulation, randomProbability, gateLength)
- `Source/TriggerSystem.cpp` — Updated `subdivBeatsFor`, `hitsPerBarToProbability`, `processBlock` with RandomFree/RandomHold branches, double-roll, sentinel, mode-switch clear guard

## Decisions Made

- **TriggerSource::Random renamed to RandomFree** — The integer value 2 is preserved so all existing DAW sessions loading `triggerSource=2` will still load RandomFree without migration
- **Double-roll model** — Two independent `nextRandom()` calls per tick; gate fires only if both population roll AND probability roll pass. Expected notes/bar = `population * probability`
- **-1 sentinel for manual gate** — `randomGateRemaining_[v] = -1` when `gateLength <= 0.0f`; the countdown guard `> 0` transparently skips the sentinel so notes sustain indefinitely until the next trigger's `fireNoteOff` at note-on
- **RandomHold pad-release cut ordering** — Pad-release check (`!padHeld && gateOpen_`) runs before the tick+roll evaluation; this ensures note-off fires on the same block as the release without waiting for the next subdivision tick

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

Build verification shows two expected downstream errors in `PluginProcessor.cpp` (lines 1150-1151) referencing old field names `randomDensity` and `randomGateTime` — these are intentional and will be resolved in Plan 02. `TriggerSystem.cpp` itself compiled without errors.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `TriggerSystem.h` and `TriggerSystem.cpp` are internally consistent and ready for Plan 02
- Plan 02 must update `PluginProcessor.cpp`: rename APVTS param IDs, forward `randomPopulation`/`randomProbability`/`gateLength` into `ProcessParams`, expand `triggerSource` choices to 4 options and `randomSubdiv` choices to 5 options
- Plan 03 (UI) can proceed after Plan 02 completes

---
*Phase: 20-random-trigger-system-extensions*
*Completed: 2026-03-01*
