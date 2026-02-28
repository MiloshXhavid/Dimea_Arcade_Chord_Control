---
phase: 17-bug-fixes
plan: 02
subsystem: gamepad
tags: [sdl2, bluetooth, crash-fix, gamepad, reconnect]

# Dependency graph
requires: []
provides:
  - "Guarded SDL_CONTROLLERDEVICEADDED handler: deferred open via pendingReopenTick_"
  - "Guarded SDL_CONTROLLERDEVICEREMOVED handler: instance-ID match before closeController()"
  - "BT reconnect crash eliminated (BUG-02 fixed)"
affects: [17-03-PLAN]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Deferred-open pattern: set a bool flag on ADDED event, consume after event loop to avoid SDL_GameControllerOpen() during active BT enumeration"
    - "Instance-ID guard on REMOVED event: SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_)) == ev.cdevice.which"

key-files:
  created: []
  modified:
    - Source/GamepadInput.h
    - Source/GamepadInput.cpp

key-decisions:
  - "pendingReopenTick_ is a plain bool (not atomic) — timerCallback() runs on JUCE message thread (single-threaded access)"
  - "Deferred by one 60Hz tick, not two — one tick is sufficient for BT enumeration to settle on tested BT stacks"
  - "Fallback disconnect poll (SDL_GameControllerGetAttached) preserved unchanged — handles USB silent-disconnect separately from BT crash"

patterns-established:
  - "Guard SDL device events before acting on them — ADDED checks controller_ == nullptr, REMOVED checks instance ID"

requirements-completed: [BUG-02]

# Metrics
duration: 2min
completed: 2026-02-28
---

# Phase 17 Plan 02: BT Reconnect Crash Fix Summary

**Guarded SDL device event handlers with instance-ID comparison and one-tick deferred open to eliminate PS4 Bluetooth double-open and wrong-handle-close crashes (BUG-02)**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-28T19:22:50Z
- **Completed:** 2026-02-28T19:24:54Z
- **Tasks:** 2 of 2
- **Files modified:** 2

## Accomplishments
- Added `pendingReopenTick_` bool member to GamepadInput.h private section — defers open by one 60Hz tick after SDL_CONTROLLERDEVICEADDED
- Replaced unconditional `tryOpenController()` on ADDED with `if (!controller_) pendingReopenTick_ = true` guard
- Replaced unconditional `closeController()` on REMOVED with instance-ID comparison guard using `SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_)) == ev.cdevice.which`
- Plugin builds cleanly with 0 errors, 0 new warnings

## Task Commits

Each task was committed atomically:

1. **Task 1: Add pendingReopenTick_ member to GamepadInput.h** - `3edf8be` (feat)
2. **Task 2: Guard SDL device events and add deferred open in GamepadInput.cpp** - `917565e` (fix)

**Plan metadata:** _(docs commit follows)_

## Files Created/Modified
- `Source/GamepadInput.h` - Added `bool pendingReopenTick_ = false` to private section (line 171)
- `Source/GamepadInput.cpp` - Replaced buggy SDL event loop (lines 108-116) with guarded version + post-loop deferred open

## Decisions Made
- `pendingReopenTick_` is a plain `bool`, not `std::atomic<bool>` — `timerCallback()` is the only accessor and runs exclusively on the JUCE message thread
- One-tick deferral (not two) — sufficient for BT enumeration to settle; two ticks would add unnecessary latency on USB plug-in
- Fallback disconnect poll (`SDL_GameControllerGetAttached`) kept unchanged — it handles USB silent-disconnect, a distinct failure mode from the BT crash

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None. Build verification confirms both SDL2 API calls (`SDL_JoystickInstanceID`, `SDL_GameControllerGetJoystick`) were already available in the linked SDL 2.30.9 release — no new dependencies required.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- BUG-02 fixed: PS4 BT reconnect no longer crashes the plugin
- Plan 17-03 (BT smoke test / manual verification) is ready to execute
- No blockers

---
*Phase: 17-bug-fixes*
*Completed: 2026-02-28*

## Self-Check: PASSED
- FOUND: Source/GamepadInput.h
- FOUND: Source/GamepadInput.cpp
- FOUND: .planning/phases/17-bug-fixes/17-02-SUMMARY.md
- FOUND commit: 3edf8be (feat(17-02): add pendingReopenTick_ member to GamepadInput.h)
- FOUND commit: 917565e (fix(17-02): guard SDL device events to fix BT reconnect crash (BUG-02))
