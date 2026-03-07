---
phase: 37-looper-internalbeat-fix
plan: 01
subsystem: looper
tags: [looper, fmod, double-scan, regression-test, catch2]

# Dependency graph
requires:
  - phase: 29-looper-perimeter-bar
    provides: LooperEngine base with recording/playback and internalBeat_ logic
provides:
  - "Fix: internalBeat_ retains fmod overshoot after free-running recording auto-stop"
  - "Regression test TC 14 guards against double-scan at beat 0 boundary"
affects: [looper, LooperEngine, overdub, recording]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "fmod-absorbed overshoot must not be overwritten after recording auto-stop"
    - "TC regression tests named [bug-double-scan] for searchability"

key-files:
  created: []
  modified:
    - Source/LooperEngine.cpp
    - Tests/LooperEngineTests.cpp

key-decisions:
  - "Line 773 (internalBeat_ = 0.0) removed — fmod at line 758 already sets internalBeat_ = overshoot in free-running mode; no sentinel needed"
  - "Comment updated from 'free-running mode discards overshoot' to 'Both modes now absorb overshoot correctly'"

patterns-established:
  - "double-scan regression pattern: drive small blocks (512 samples) to guarantee non-integer overshoot at recording boundary, then assert beat > 1e-4 on first-playback block"

requirements-completed: []

# Metrics
duration: 8min
completed: 2026-03-07
---

# Phase 37 Plan 01: Looper internalBeat_ Fix + Regression Test Summary

**Removed one-line double-scan bug (internalBeat_=0 at recording auto-stop) and added TC 14 free-running regression guard — fmod already absorbs overshoot, no sentinel needed**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-07T~02:40:00Z
- **Completed:** 2026-03-07T~02:48:00Z
- **Tasks:** 2/3 (Task 3 is checkpoint:human-verify — awaiting user)
- **Files modified:** 2

## Accomplishments
- Removed `internalBeat_ = 0.0` (line 773) — the single-line root cause of duplicate beat-0 events after overdub recording auto-stops in free-running mode
- Updated comment from "free-running mode discards overshoot" to "Both modes now absorb overshoot correctly" with per-mode explanation
- Added TC 14 `[looper][bug-double-scan]` to LooperEngineTests.cpp: drives 512-sample blocks at 120 BPM, verifies `beatAfterStop > 1e-4` and `< blockBeats` on the first-playback block after auto-stop

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove internalBeat_=0 and update comment** - `3c54b2c` (fix)
2. **Task 2: Add TC 14 regression test** - `6c251fd` (test)
3. **Task 3: checkpoint:human-verify** - awaiting user build + DAW smoke test

## Files Created/Modified
- `Source/LooperEngine.cpp` - Removed line 773 (`internalBeat_ = 0.0`); updated comment block at lines 778-780
- `Tests/LooperEngineTests.cpp` - Appended TC 14 (lines 496-564)

## Decisions Made
- Line 773 removed entirely (not gated or conditioned) — the fmod at line 758 is the canonical absorb path for both modes; the sentinel was always wrong for free-running
- TC 14 uses 512-sample blocks deliberately: `4.0 / blockBeats ≈ 172.27` (non-integer), guaranteeing overshoot > 0 without special-casing

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

**Build and smoke test required.** Run in admin PowerShell:

```powershell
# Step 1 — Build + run Catch2 tests
& "C:\Users\Milosh Xhavid\get-shit-done\build32.ps1"
# Run test binary — confirm all 14 tests pass (TC 14 GREEN, TC 01-13 no regression)

# Step 2 — Install VST3
& "C:\Users\Milosh Xhavid\get-shit-done\do-reinstall.ps1"
```

Then DAW smoke test:
- SC1: Record a loop, let it play back — confirm no extra note-ons at loop start boundary
- SC2: Record an overdub, play back 2+ cycles — confirm no "resets to beginning" after overdub
- SC3: Multiple consecutive overdubs — confirm no phantom duplicates

## Next Phase Readiness
- Code fix and regression test are committed and ready to build
- All prior TCs (01-13) unaffected — no LooperEngine logic changed except removal of the one faulty line
- Awaiting: user build confirmation + DAW smoke test approval (checkpoint)

---
*Phase: 37-looper-internalbeat-fix*
*Completed: 2026-03-07 (pending checkpoint approval)*
