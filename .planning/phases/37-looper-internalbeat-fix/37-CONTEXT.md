# Phase 37 Context: Looper internalBeat_ Fix

## Phase Summary

**Goal:** Fix the LooperEngine double-scan bug where `internalBeat_ = 0.0` at line 773 overwrites the correct fmod-absorbed overshoot value, causing beat-0 events to fire twice on the block after a recording ends.

**Scope:** One line removed + one comment updated in `Source/LooperEngine.cpp`. One Catch2 unit test added. One DAW manual smoke test.

---

## Bug Analysis

### Root Cause

In `LooperEngine::process()`, when recording ends (line 768–786):

1. **Line 756–758** — internal clock advances correctly:
   ```cpp
   internalBeat_ += blockBeats;
   if (internalBeat_ >= loopLen)
       internalBeat_ = std::fmod(internalBeat_, loopLen);  // overshoot absorbed
   ```

2. **Line 773 (THE BUG)** — overwrites the correct fmod result with 0.0:
   ```cpp
   internalBeat_ = 0.0;  // discards overshoot → next block starts at exactly 0.0
   ```

3. **Next block** — `beatAtBlockStart = fmod(0.0, loopLen) = 0.0`, so the scan window `[0, blockBeats)` fires beat-0 events **again**, duplicating the first notes of the loop.

### Fix

Remove line 773 entirely. The fmod at line 758 already absorbs overshoot correctly for free-running mode. DAW sync mode absorbs overshoot independently via the `loopStartPpq_ + loopLen` anchor advance on line 774.

---

## Decisions

### 1. Code Change Scope

**Decision: Remove line 773 + update line 780 comment.**

Line 780 currently reads:
> "free-running mode discards it (internalBeat_ = 0.0); DAW sync mode automatically absorbs it..."

After the fix, both modes absorb overshoot. The comment must be updated to:
> "Both modes absorb overshoot: free-running via fmod at line 758; DAW sync via loopStartPpq_ advance above."

The `(void)overshoot;` at line 783 can stay — overshoot is still computed but not explicitly used (both paths absorb it implicitly).

### 2. Verification Approach

**Decision: Both Catch2 unit test AND DAW manual test.**

- **Catch2 test** — drives `process()` in free-running mode through a boundary where recording ends mid-block. Asserts that no event at beat 0 fires on the block immediately following recording completion. Lives in `Tests/ChordEngineTests.cpp` or a new `Tests/LooperEngineTests.cpp`.
- **DAW manual test** — record an overdub loop, let it play back 2–3 cycles, confirm in the DAW MIDI monitor that no extra note-ons appear at the loop start boundary.

---

## Code Context

**File:** `Source/LooperEngine.cpp`

| Line | Content |
|------|---------|
| 756–758 | Internal clock advance + fmod overshoot absorption |
| 768–786 | Recording end block (auto-stop when recordedBeats_ >= loopLen) |
| **773** | **`internalBeat_ = 0.0;` — THE LINE TO REMOVE** |
| 774 | `loopStartPpq_ += loopLen;` — DAW anchor advance (keep) |
| 780 | Comment describing overshoot handling — **UPDATE** |
| 783 | `(void)overshoot;` — keep as-is |

**Test files to check:** `Tests/ChordEngineTests.cpp`, `Tests/ScaleQuantizerTests.cpp` — see existing Catch2 patterns for LooperEngine mock/harness style.

---

## What the Plan Must Do

1. Remove `Source/LooperEngine.cpp` line 773 (`internalBeat_ = 0.0;`)
2. Update the overshoot comment on line 780 to be accurate post-fix
3. Add a Catch2 unit test: drive process() through a record-end boundary, assert no double-fire at beat 0
4. Build and run unit tests — all pass
5. User builds and installs VST3 manually
6. DAW manual smoke test: record overdub, verify no phantom notes at loop boundary across 2+ cycles

## Out of Scope (deferred)

- Any other looper bugs
- Changes to DAW sync path (loopStartPpq_ logic is correct and unchanged)
- Changes to `wraps` scan logic
