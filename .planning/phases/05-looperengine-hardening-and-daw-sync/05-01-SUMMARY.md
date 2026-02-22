---
phase: 05-looperengine-hardening-and-daw-sync
plan: "01"
subsystem: audio-engine
tags: [juce, abstractfifo, lock-free, catch2, spsc, punch-in, daw-sync, tsan]

# Dependency graph
requires:
  - phase: 02-core-engine-validation
    provides: Catch2 FetchContent infrastructure and ChordJoystickTests CMake target

provides:
  - Lock-free LooperEngine with AbstractFifo SPSC double-buffer design
  - Punch-in merge algorithm preserving events at untouched beat positions
  - 11 new Catch2 tests covering default state, loop length, record/playback, reset, delete, FIFO stress, DAW sync anchor, punch-in correctness, and loop-wrap boundary sweep (96 combos)
  - ENABLE_TSAN CMake option gated on Clang/GNU with MSVC warning
  - ASSERT_AUDIO_THREAD() debug macro

affects: [05-02-looperengine-daw-wiring, 05-03-looperengine-verification]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "AbstractFifo SPSC double-buffer: eventBuf_+fifo_ for recording, playbackStore_ for playback"
    - "ScopedRead/ScopedWrite must go out of scope before fifo_.reset() to prevent validStart advance"
    - "Scratch buffers as class members to avoid ~49KB stack arrays in finaliseRecording()"
    - "Destructive ops (reset, delete) via atomic request flags serviced at top of process()"
    - "Recording gate/joystick via fifo_.write(1).forEach on audio thread — no lock"
    - "finaliseRecording() threading invariant: called ONLY when playing_=false AND recording_=false"

key-files:
  created:
    - Tests/LooperEngineTests.cpp
  modified:
    - Source/LooperEngine.h
    - Source/LooperEngine.cpp
    - CMakeLists.txt

key-decisions:
  - "Double-buffer over snapshot-scan-reinsert: separate FIFO (write side) and playbackStore_ (read side) avoids needing a stable snapshot during audio-thread scan"
  - "Punch-in touch radius = loopLen/64: replaces old events within that beat window, preserves all others; wrap-around distance handled via min(dist, loopLen-dist)"
  - "ScopedRead must destruct before fifo_.reset(): JUCE AbstractFifo reset() sets validEnd=0 then validStart=0, but ScopedRead dtor advances validStart by numRead; if reset() runs first the dtor leaves validStart=numRead with validEnd=0, causing getNumReady() to return bufferSize-numRead=2047"
  - "Scratch buffers scratchNew_ and scratchMerged_ as class members: avoids 2x 49KB stack allocation in finaliseRecording() that caused MSVC debug-mode stack corruption"
  - "TC 7 stress test validates atomic-flag no-deadlock only (not SPSC data correctness): concurrent writer+reader violates SPSC invariant intentionally; TC 10 validates correctness sequentially"

patterns-established:
  - "ASSERT_AUDIO_THREAD() macro: fires in Debug builds if audio method called from message thread"
  - "Atomic request flag pattern: UI stores true into resetRequest_/deleteRequest_, audio thread exchanges false at top of process()"
  - "Loop-wrap boundary test pattern: place event at loopLen-epsilon, drive blocksPerLoop*2 process() calls, assert fireCount==2"

requirements-completed: [LOOPER-01, LOOPER-02, LOOPER-04]

# Metrics
duration: ~180min
completed: 2026-02-22
---

# Phase 05 Plan 01: LooperEngine Hardening Summary

**Lock-free LooperEngine rewrite: AbstractFifo SPSC double-buffer with punch-in merge, 26 Catch2 tests all passing, TSAN CMake option**

## Performance

- **Duration:** ~180 min (including extensive FIFO reset-order bug debugging)
- **Started:** 2026-02-22
- **Completed:** 2026-02-22
- **Tasks:** 3
- **Files modified:** 4

## Accomplishments

- Replaced std::mutex/std::vector in LooperEngine with fully lock-free AbstractFifo SPSC design — no blocking on audio thread
- Implemented punch-in merge algorithm: new recordings replace events at touched beat windows (within loopLen/64 radius), all other events preserved
- 26 Catch2 tests pass (15 existing + 11 new): stress test confirms no deadlock, loop-wrap sweep covers all 6 time signatures x 16 bar lengths (96 combinations)

## Task Commits

Each task was committed atomically:

1. **Task 1: Rewrite LooperEngine.h** - `72a9eeb` (feat)
2. **Task 2: Rewrite LooperEngine.cpp** - `3de3cec` (feat)
3. **Task 3: Catch2 tests + TSAN + ScopedRead bug fix** - `f27440b` (feat)

## Files Created/Modified

- `Source/LooperEngine.h` - Fully rewritten: no mutex/vector, AbstractFifo+std::array double-buffer, atomic flags (recJoy_, recGates_, syncToDaw_, capReached_, deleteRequest_, resetRequest_), ASSERT_AUDIO_THREAD macro, scratch buffer class members
- `Source/LooperEngine.cpp` - Fully rewritten: lock-free recordGate/recordJoystick via fifo_.write(1).forEach, process() with deleteRequest_/resetRequest_ exchange at entry, finaliseRecording() with Case0/Case1/Case2 punch-in merge, ScopedRead scope block fix
- `Tests/LooperEngineTests.cpp` - New: 11 TEST_CASEs (TC1-default, TC2-loop-length, TC3-no-play, TC4-record-playback, TC5-reset, TC6-delete, TC7-FIFO-stress, TC8-DAW-anchor, TC9-negative-ppq, TC10-punch-in, TC11-wrap-sweep)
- `CMakeLists.txt` - Added LooperEngine.cpp + LooperEngineTests.cpp to ChordJoystickTests sources; ENABLE_TSAN option with MSVC warning guard

## Decisions Made

- **Double-buffer over snapshot-scan-reinsert:** The FIFO (eventBuf_+fifo_) handles the write side during recording; playbackStore_ holds the stable read-side copy populated by finaliseRecording(). This avoids needing a mid-scan stable snapshot and keeps the audio thread free from contention.
- **Punch-in touch radius = loopLen/64:** Events at beat positions within this radius of any newly recorded event are replaced; all others preserved. Wrap-around distance uses min(dist, loopLen-dist) to handle the loop boundary.
- **Scratch buffers as class members:** scratchNew_ and scratchMerged_ (each 2048 x 24 bytes = ~49KB) moved from local arrays to class members to prevent MSVC debug-mode stack overflow in finaliseRecording().
- **recJoy_/recGates_ separate atomic flags:** Allows independent armed/unarmed state per event type so the UI can present distinct [REC JOY] and [REC GATES] toggles.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed finaliseRecording() Case 0 wiping existing content**
- **Found during:** Task 2 (LooperEngine.cpp implementation)
- **Issue:** Original plan combined Case 0 (newCount=0) and Case 1 (playbackCount=0) into one branch that always stored count=0 when newCount=0, erasing existing playbackStore_ content on stop-with-no-new-recording
- **Fix:** Separated into explicit Case 0 (early return when newCount=0) and Case 1 (drain only when playbackCount=0)
- **Files modified:** Source/LooperEngine.cpp
- **Verification:** TC 10 punch-in test: calling startStop() twice (stop then play) with no new events preserves existing content
- **Committed in:** 3de3cec (Task 2 commit)

**2. [Rule 1 - Bug] Moved scratch arrays to class members to fix MSVC stack overflow**
- **Found during:** Task 3 debugging (TC 10 failing, phantom 2047 FIFO events)
- **Issue:** Local arrays LooperEvent newEvents[2048] and LooperEvent merged[2048] on stack in finaliseRecording() (~98KB total) caused MSVC debug-mode stack corruption, manifesting as phantom FIFO state
- **Fix:** Moved to class members scratchNew_ and scratchMerged_ declared in LooperEngine.h
- **Files modified:** Source/LooperEngine.h, Source/LooperEngine.cpp
- **Verification:** TC 7 stress test no longer corrupts TC 10 FIFO state
- **Committed in:** f27440b (Task 3 commit)

**3. [Rule 1 - Bug] Fixed ScopedRead-before-reset ordering bug**
- **Found during:** Task 3 (TC 10 failing with fifoNumReady()==2047 after finaliseRecording())
- **Issue:** In finaliseRecording() Case 1, `fifo_.reset()` was called while the ScopedRead was still in scope. When ScopedRead destructs after reset(), its `finishedRead(newCount)` call advances validStart to newCount while validEnd is already 0 from reset(). Result: getNumReady() returns bufferSize-newCount = 2047 instead of 0.
- **Root cause:** JUCE AbstractFifo::reset() sets validEnd=0 then validStart=0 (two separate atomic stores). ScopedRead dtor calls finishedRead which does `validStart = validStart.get() + numRead`. After reset sets validStart=0, the dtor runs and sets validStart=numRead=1, leaving validEnd=0,validStart=1, so getNumReady()=bufferSize-(1-0)=2047.
- **Fix:** Wrapped the ScopedRead in a scope block so it destructs (finishedRead fires) BEFORE fifo_.reset() is called. Added comment explaining the invariant. Case 2 already had a scope block; only Case 1 was missing it.
- **Files modified:** Source/LooperEngine.cpp
- **Verification:** All 26 tests pass; fifoNumReady()==0 confirmed in TC 10 after first record disarm
- **Committed in:** f27440b (Task 3 commit)

**4. [Rule 1 - Bug] Added fifo_.reset() in record() arm branch**
- **Found during:** Task 3 debugging (TC 10, stale FIFO state from previous test cases)
- **Issue:** record() arm did not clear the FIFO before setting recording_=true, allowing stale events from previous stress tests to appear as new recordings
- **Fix:** Added `fifo_.reset()` before `recording_.store(true)` in the arm branch of record()
- **Files modified:** Source/LooperEngine.cpp
- **Verification:** TC 10 operates on a clean FIFO at start of each recording pass
- **Committed in:** f27440b (Task 3 commit)

---

**Total deviations:** 4 auto-fixed (all Rule 1 - Bug)
**Impact on plan:** All four fixes required for correctness. No scope creep. The ScopedRead ordering bug is a fundamental JUCE AbstractFifo usage pattern — any caller that calls reset() while a ScopedRead is alive will exhibit this behavior.

## Issues Encountered

- **Phantom 2047 FIFO events:** The hardest issue in this plan. TC 10's punch-in test consistently showed `fifoNumReady()==2047` immediately after `finaliseRecording()` returned. The root cause was two compounding bugs: (a) 49KB stack arrays causing MSVC debug-mode stack corruption, and (b) ScopedRead destructing after fifo_.reset() advancing validStart past 0. Once the stack arrays were moved to class members, the real JUCE AbstractFifo ordering issue became visible and fixable. Diagnosed via printf diagnostics in finaliseRecording() (same this/&fifo_ address throughout) and inspection of AbstractFifo source code.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- LooperEngine is now fully lock-free and correct
- 26 Catch2 tests pass (0 failures)
- Ready for 05-02: PluginProcessor DAW sync wiring (ppqPosition integration) and PluginEditor [REC JOY]/[REC GATES]/[SYNC] button wiring
- ASSERT_AUDIO_THREAD() macro available for future audio-thread-only method enforcement

---
*Phase: 05-looperengine-hardening-and-daw-sync*
*Completed: 2026-02-22*

## Self-Check: PASSED

- FOUND: Source/LooperEngine.h
- FOUND: Source/LooperEngine.cpp
- FOUND: Tests/LooperEngineTests.cpp
- FOUND: CMakeLists.txt
- FOUND: .planning/phases/05-looperengine-hardening-and-daw-sync/05-01-SUMMARY.md
- FOUND: commit 72a9eeb (Task 1)
- FOUND: commit 3de3cec (Task 2)
- FOUND: commit f27440b (Task 3)
- Tests: All 26 passed (655 assertions)
