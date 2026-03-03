---
phase: 27-triplet-subdivisions
plan: 01
subsystem: ui
tags: [juce, apvts, triggerSystem, looperEngine, combobox, subdivisions, triplets]

# Dependency graph
requires:
  - phase: 26-defaults-and-bug-fix
    provides: stable APVTS enum base and no noteCount_ clamp before triplet logic added
provides:
  - triplet subdivisions (1/1T through 1/32T) in RandomSubdiv enum
  - interleaved straight+triplet ordering in all four subdivision selectors
  - updated quantizeSubdivToGridSize() with new index mapping
affects:
  - phase-28-random-free-redesign (reads RandomSubdiv enum values)
  - phase-29-looper-perimeter-bar (independent but shares LooperEngine)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Interleaved enum ordering: straight and triplet subdivisions paired by musical duration, slowest to fastest"
    - "Single-source-of-truth index mapping: APVTS StringArray indices, quantizeSubdivToGridSize() case numbers, and ComboBox item IDs all align"

key-files:
  created: []
  modified:
    - Source/TriggerSystem.h
    - Source/PluginProcessor.cpp
    - Source/LooperEngine.cpp
    - Source/LooperEngine.h
    - Source/PluginEditor.cpp

key-decisions:
  - "Interleaved ordering chosen over appended-block: each triplet sits immediately after its straight counterpart (same duration x 2/3), making the dropdown readable as a musically ordered list"
  - "Default index for randomSubdiv changed from 5 (old 1/8) to 8 (new 1/8) in both APVTS and ComboBox initial selection"
  - "Default index for quantizeSubdiv changed from 1 (old 1/8) to 4 (new 1/8) in both APVTS and ComboBox"
  - "No preset backward compatibility maintained — user accepted index shift for old presets; no migration needed"
  - "TriggerSystem.cpp subdivBeatsFor() and hitsPerBarToProbability() use enum names (not integers), so no cpp changes required after enum reorder"

patterns-established:
  - "Subdivision index alignment: whenever adding new subdivisions, keep APVTS StringArray, quantizeSubdivToGridSize() cases, and PluginEditor ComboBox addItem() in exact same order"

requirements-completed: [TRIP-01, TRIP-02]

# Metrics
duration: 25min
completed: 2026-03-03
---

# Phase 27 Plan 01: Triplet Subdivisions Summary

**Triplet subdivisions (1/1T through 1/32T) added to random trigger and quantize selectors, reordered to interleaved straight+triplet pairs sorted slowest to fastest**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-03-03T09:00:00Z
- **Completed:** 2026-03-03T09:25:00Z
- **Tasks:** 3 (Task 1 + Task 2 previously committed; Task 3 = 4 reordering change groups)
- **Files modified:** 5

## Accomplishments

- Added 6 triplet values (WholeT, HalfT, QuarterT, EighthT, SixteenthT, ThirtySecondT) to the RandomSubdiv enum with interleaved ordering
- Reordered all 4 subdivision selectors (randomSubdiv0..3 + quantizeSubdiv) in APVTS, LooperEngine, and PluginEditor to interleaved tempo order
- Updated quantizeSubdivToGridSize() case numbers to match new interleaved qSubdivChoices indices
- Build verified after each change group — all 4 VST3 builds passed

## Task Commits

All commits from this plan (including previously-done Tasks 1 and 2):

1. **Task 1: Backend enum + dispatch + APVTS + LooperEngine** - `771a606` (feat)
2. **Task 2: PluginEditor UI ComboBoxes** - `c392dc9` (feat)
3. **Change Group A: Reorder RandomSubdiv enum** - `0e364cb` (feat)
4. **Change Group B: Reorder APVTS choices + defaults** - `8226a3c` (feat)
5. **Change Group C: Reorder quantizeSubdivToGridSize()** - `acf8988` (feat)
6. **Change Group D: Reorder PluginEditor ComboBoxes** - `6be7ae2` (feat)

## Files Created/Modified

- `Source/TriggerSystem.h` - RandomSubdiv enum reordered to interleaved tempo order (integers 0-14 reassigned)
- `Source/PluginProcessor.cpp` - randomSubdiv StringArray + default (5→8) + qSubdivChoices + default (1→4) reordered
- `Source/LooperEngine.cpp` - quantizeSubdivToGridSize() cases rewritten for new interleaved index mapping
- `Source/LooperEngine.h` - quantizeSubdiv_ comment updated to reflect new 0=1/1T, 1=1/2T, 2=1/4 ... mapping
- `Source/PluginEditor.cpp` - randomSubdivBox_ StringArray + setSelectedItemIndex(5→8) + quantizeSubdivBox_ addItem order

## Decisions Made

- **Interleaved vs appended:** Chose interleaved (triplet immediately after straight peer) over appending triplets as a separate block. Musical reason: a player scanning the dropdown sees "1/4, 1/4T, 1/8, 1/8T..." rather than "..., 1/4, 1/8, ..., 1/4T, 1/8T..." — easier to find the right groove subdivision at a glance.
- **No preset migration:** Old saved preset values for randomSubdiv indices 3-14 and all quantizeSubdiv values will map to different subdivisions after this change. User explicitly accepted this (no backward compatibility needed).
- **TriggerSystem.cpp untouched:** subdivBeatsFor() and hitsPerBarToProbability() switch on enum names (e.g., RandomSubdiv::WholeT), not integer values, so reordering the enum integers did not require any switch-case renumbering in the .cpp file.

## Deviations from Plan

The original task 3 (human-verify checkpoint) was modified by the user before execution: instead of proceeding to the verify checkpoint, the user asked for the subdivision lists to be reordered to interleaved tempo order. The 4 change groups (A/B/C/D) replaced what would have been a single verify checkpoint task.

None of the auto-fix deviation rules (1-3) were triggered. No Rule 4 architectural changes were needed.

**Total deviations:** 0 auto-fixed rules triggered. Plan scope was expanded by user instruction before execution.
**Impact on plan:** Reordering is entirely correct behavior — all 4 layers (enum, APVTS, LooperEngine math, PluginEditor UI) are now in sync.

## Issues Encountered

None. All 4 intermediate builds passed. The known bash shell `/c/Users/Milosh: Is a directory` warning appeared on every commit but is a known spurious error that does not affect execution.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 27 complete. RandomSubdiv enum now has all 15 values (0-14) in interleaved order.
- Phase 28 (Random Free Redesign) can safely read RandomSubdiv enum values — enum integers are now stable.
- Phase 29 (Looper Perimeter Bar) is independent and can also proceed.
- Ready for `/gsd:plan-phase 28` or `/gsd:plan-phase 29`.

---
*Phase: 27-triplet-subdivisions*
*Completed: 2026-03-03*
