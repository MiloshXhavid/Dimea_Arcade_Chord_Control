---
phase: 17-bug-fixes
plan: 01
subsystem: testing
tags: [looper, daw-sync, anchor-drift, catch2, tdd, bug-fix]

# Dependency graph
requires:
  - phase: 10-trigger-quantization-infrastructure
    provides: LooperEngine with DAW sync anchor logic and processBlock beat tracking
provides:
  - TC 13 regression test for BUG-01 (looper anchor drift after auto-stop in DAW sync mode)
  - Fixed auto-stop block in LooperEngine::process() — loopStartPpq_ advanced by loopLen instead of reset to -1.0
affects:
  - 22-lfo-recording  # looper anchor correctness is prerequisite for LFO seam accuracy

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Advance loopStartPpq_ by loopLen on auto-stop (not sentinel reset) to preserve bar-aligned anchor across cycles"
    - "TDD RED/GREEN cycle: failing test committed before fix, green committed after"

key-files:
  created: []
  modified:
    - Tests/LooperEngineTests.cpp  # TC 13 appended after TC 12
    - Source/LooperEngine.cpp      # auto-stop block lines ~756-766 fixed

key-decisions:
  - "Used loopStartPpq_ += loopLen instead of plan's p.ppqPosition - overshoot formula — plan formula only works if ppqPosition equals recording end, but code uses block start; advancing by loopLen is always correct"
  - "TC 13 uses ppqPosition = 4.0 - 1e-6 to expose FP drift scenario where anchorToBar floors to wrong bar — zero-overshoot scenario with exact ppq=4.0 would not demonstrate the bug"

patterns-established:
  - "Do not use sentinel -1.0 to signal re-anchor after a known recording endpoint; advance the anchor explicitly"

requirements-completed:
  - BUG-01

# Metrics
duration: 45min
completed: 2026-02-28
---

# Phase 17 Plan 01: BUG-01 Looper Anchor Drift Fix Summary

**loopStartPpq_ += loopLen on auto-stop replaces -1.0 sentinel, eliminating DAW sync bar-boundary flooring on FP-drift ppqPosition**

## Performance

- **Duration:** ~45 min
- **Started:** 2026-02-28T00:00:00Z
- **Completed:** 2026-02-28
- **Tasks:** 2 (TDD: RED + GREEN)
- **Files modified:** 2

## Accomplishments
- TC 13 `[bug01]` added to Tests/LooperEngineTests.cpp: RED before fix, GREEN after fix
- BUG-01 root cause fixed in Source/LooperEngine.cpp auto-stop block: loopStartPpq_ now advances by loopLen instead of being reset to -1.0 sentinel
- No regressions in pre-existing passing tests (69 total: 64 passed, 5 pre-existing failures in TCs 4/5/6/10/11 unchanged)

## Task Commits

Each task was committed atomically:

1. **Task 1: Write failing TC 13 regression test for BUG-01** - `e34c48f` (test)
2. **Task 2: Fix LooperEngine.cpp auto-stop anchor — make TC 13 green** - `af1ddff` (fix)

_Note: TDD tasks have two commits (test RED → fix GREEN)_

## Files Created/Modified
- `Tests/LooperEngineTests.cpp` - TC 13 appended: demonstrates anchor drift via ppq = 4.0 - 1e-6 FP drift scenario
- `Source/LooperEngine.cpp` - auto-stop block (~line 756): replaced `loopStartPpq_ = -1.0` with `loopStartPpq_ = loopStartPpq_ + loopLen`

## Key Formula Change

**Buggy code:**
```cpp
loopStartPpq_  = -1.0;    // triggers wrong anchorToBar() on next block
```

**Fixed code:**
```cpp
const double overshoot = recordedBeats_ - loopLen;  // computed first, before clearing
loopStartPpq_  = loopStartPpq_ + loopLen;  // advance anchor by exactly one loop length
(void)overshoot;
```

The fix advances the anchor from its recording-start value (e.g., 0.0) to the start of the next playback cycle (e.g., 4.0). When the next block's ppqPosition has FP drift (e.g., 3.9999... instead of 4.0), elapsed = ppqPosition - loopStartPpq_ = negative → clamped to 0.0 → beat = 0.0 (correct). The buggy -1.0 sentinel caused anchorToBar to re-floor, picking the previous bar (0.0 instead of 4.0) → elapsed ≈ 4.0 → beat ≈ 4.0 (wrong).

## Decisions Made
- **Formula correction (Rule 1 - Bug):** Plan specified `loopStartPpq_ = p.ppqPosition - overshoot`. Analysis showed this formula uses block-START ppqPosition (3.0) rather than the end-of-recording ppq (4.0), giving a wrong anchor of 3.0. The correct formula is `loopStartPpq_ += loopLen` which is always equivalent to `loopStartPpq_at_recording_start + loopLen` regardless of block size or FP drift.
- **TC 13 scenario correction (Rule 1 - Bug):** Plan's test used ppq=4.0 for the first playback block. This exact value causes anchorToBar to produce the same result with both buggy and fixed code (no observable difference). Changed to ppq = 4.0 - 1e-6 to expose the actual FP drift scenario where the bug manifests.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Corrected fix formula from `p.ppqPosition - overshoot` to `loopStartPpq_ + loopLen`**
- **Found during:** Task 2 (Fix LooperEngine.cpp)
- **Issue:** Plan formula `p.ppqPosition - overshoot` uses the ppqPosition at the START of the auto-stop block (3.0 in the test), not at recording end (4.0). This gives loopStartPpq_=3.0, causing playback to show beat 1.0 (wrong) for the first playback block at ppq=4.0.
- **Fix:** Used `loopStartPpq_ = loopStartPpq_ + loopLen` — advances anchor by exactly one loop length from the original recording anchor, always correct regardless of block size or FP drift.
- **Files modified:** Source/LooperEngine.cpp
- **Verification:** TC 13 passes with fix; TC 13 fails without fix (returns 3.99999904... ≈ 4.0 instead of 0.0)
- **Committed in:** af1ddff (Task 2 commit)

**2. [Rule 1 - Bug] Corrected TC 13 first playback block ppq from 4.0 to 4.0 - 1e-6**
- **Found during:** Task 1 (Write failing TC 13)
- **Issue:** With ppq=4.0 (exact bar boundary), both the buggy and fixed code produce the same result (beat=0.0) — anchorToBar(4.0, 4.0)=4.0 which coincidentally equals the correct anchor. The test would PASS WITH THE BUG, making it useless as a regression guard.
- **Fix:** Used ppq = 4.0 - 1e-6 to simulate real-world FP drift. With the bug, anchorToBar(3.999..., 4.0) floors to 0.0 (wrong bar), giving elapsed ≈ 4.0 and beat ≈ 4.0 (clearly fails the Approx(0.0) check). With the fix, loopStartPpq_=4.0, elapsed=-1e-6→0.0, beat=0.0 (passes).
- **Files modified:** Tests/LooperEngineTests.cpp
- **Verification:** TC 13 is RED before fix (returns 3.99999...) and GREEN after fix (returns 0.0)
- **Committed in:** e34c48f (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (both Rule 1 — Bug)
**Impact on plan:** Both fixes essential for the test to correctly demonstrate and guard against BUG-01. No scope creep.

## Issues Encountered
- Plan's test scenario and fix formula were internally inconsistent: the scenario with exact ppq=4.0 doesn't expose the bug, and the formula `p.ppqPosition - overshoot` doesn't produce the expected value. Required analysis of the DAW sync timing model to identify the correct formula.

## Pre-existing Test Failures (out of scope)
TC 4, 5, 6, 10, 11 were already failing before this plan's changes. These are unrelated looper tests from Phase 10 that have not been fixed. Their failure count did not change (5 before, 5 after).

## Next Phase Readiness
- BUG-01 is resolved. Phase 22 (LFO Recording) can proceed without looper anchor drift corrupting LFO seam alignment
- No blockers

## Self-Check: PASSED

- Tests/LooperEngineTests.cpp: FOUND
- Source/LooperEngine.cpp: FOUND
- 17-01-SUMMARY.md: FOUND
- Commit e34c48f (TC 13 RED): FOUND
- Commit af1ddff (BUG-01 fix): FOUND

---
*Phase: 17-bug-fixes*
*Completed: 2026-02-28*
