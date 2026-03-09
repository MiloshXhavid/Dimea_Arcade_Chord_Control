---
plan: 24-01
phase: 24-gamepad-option-mode-1
status: complete
completed: "2026-03-01"
---

# Plan 24-01 Summary: GamepadInput Mode 1 Atomic Signals

## What Was Built

Added four new atomic signal channels to `GamepadInput` for Option Mode 1 face-button dispatch. These channels form the cross-thread signal plumbing that `PluginProcessor` consumes in Plan 02.

## Tasks Completed

### Task 1: Add Mode 1 atomic signals and consume methods to GamepadInput.h

Added to the public interface (after `consumeOptionDpadDelta`):
```cpp
int  consumeArpRateDelta()  { return pendingArpRateDelta_.exchange(0, std::memory_order_relaxed); }
int  consumeArpOrderDelta() { return pendingArpOrderDelta_.exchange(0, std::memory_order_relaxed); }
bool consumeArpCircle()     { return pendingArpCircle_.exchange(false, std::memory_order_relaxed); }
bool consumeRndSyncToggle() { return pendingRndSyncToggle_.exchange(false, std::memory_order_relaxed); }
```

Added to the private section (after `optDpadLastPressMs_`):
- `pendingArpRateDelta_` (atomic<int>), `pendingArpOrderDelta_` (atomic<int>)
- `pendingArpCircle_` (atomic<bool>), `pendingRndSyncToggle_` (atomic<bool>)
- `arpRateLastPressMs_` and `arpOrderLastPressMs_` timestamp fields
- `btnMode1Circle_`, `btnMode1Triangle_`, `btnMode1Square_`, `btnMode1Cross_` ButtonState trackers

### Task 2: Add Mode 1 face-button detection block to GamepadInput.cpp and build

Added Mode 1 face-button detection block inside the `!optionFrameFired_` guard, after the existing D-pad block. Key behaviors:
- Circle (SDL B) → rising edge sets `pendingArpCircle_`
- Triangle (SDL Y) → double-click pattern: first press +1, second within 300ms adds -2 to `pendingArpRateDelta_`
- Square (SDL X) → same double-click pattern for `pendingArpOrderDelta_`
- Cross (SDL A) → rising edge sets `pendingRndSyncToggle_`

The existing unconditional looper button block (~lines 195-207) was NOT modified — looper atoms continue firing in all modes from the separate block.

`now` variable was confirmed in-scope throughout `timerCallback()`, so no `now2` alias was needed.

## Key Files

- `Source/GamepadInput.h` — 4 new atomics, 2 timestamp fields, 4 ButtonState trackers, 4 public consume methods
- `Source/GamepadInput.cpp` — Mode 1 face-button detection block inside `!optionFrameFired_` guard

## Verification

- `grep` confirms: `pendingArpRateDelta_`, `consumeArpCircle`, `btnMode1Triangle_` all present in header
- Build: 0 errors, VST3 produced and deployed to `C:\Program Files\Common Files\VST3\`

## Commits

- `feat(24-01): add Mode 1 face-button atomic signals to GamepadInput` (491eb39)

## Self-Check: PASSED

All must-haves satisfied:
- [x] 4 new consume methods exposed on public interface
- [x] Mode 1 face-button block inside `!optionFrameFired_` guard
- [x] Circle and Cross are simple boolean toggles
- [x] Triangle and Square use kDpadDoubleClickMs double-click pattern
- [x] Separate ButtonState trackers (looper detection unchanged)
- [x] Build succeeds with 0 errors
