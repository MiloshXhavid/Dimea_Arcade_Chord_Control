---
phase: 06-sdl2-gamepad-integration
plan: "01"
subsystem: gamepad
tags: [sdl2, gamepad, atomic, debounce, sample-and-hold, c++17]

# Dependency graph
requires:
  - phase: 05-looperengine-hardening-and-daw-sync
    provides: LooperEngine, GamepadInput (pre-refactor baseline)
provides:
  - SdlContext process-level SDL2 singleton (acquire/release ref-count)
  - GamepadInput dead zone atomic (setDeadZone feeds joystickThreshold APVTS)
  - GamepadInput sample-and-hold for right and left sticks
  - GamepadInput 20ms ButtonState debounce on all 8 buttons
  - Dual onConnectionChange / onConnectionChangeUI callback slots
affects:
  - 06-02 (PluginProcessor must call setDeadZone from processBlock)
  - 06-03 (PluginEditor uses onConnectionChangeUI slot)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Process-level singleton via atomic ref-count (acquire/release) — safe for multi-instance DAW plugins"
    - "Sample-and-hold: only update held value when normalised axis != 0.0f (in dead zone)"
    - "ButtonState struct with SDL_GetTicks() timestamp for 20ms debounce — prevents double-fire"
    - "Dual callback slots prevent later assignment stomping earlier assignment"

key-files:
  created:
    - Source/SdlContext.h
    - Source/SdlContext.cpp
  modified:
    - Source/GamepadInput.h
    - Source/GamepadInput.cpp
    - CMakeLists.txt

key-decisions:
  - "DBG() not available in SdlContext.cpp (no JUCE headers); replaced with (void)SDL_GetError() — SDL error readable in debugger without macro dependency"
  - "SdlContext holds gRefCount and gAvailable as file-scope atomics — no singleton class instance needed"
  - "setDeadZone() writes deadZone_ with memory_order_relaxed — 60 Hz timer reads same atomics, full ordering not required"

patterns-established:
  - "SdlContext acquire/release: SDL_Init on first acquire, SDL_Quit on last release — copy this pattern for any other process-global resource"
  - "ButtonState: {bool prev, uint32_t lastMs} with kDebounceMsBtn threshold — use for any periodic-poll button debounce"

requirements-completed: [SDL-01, SDL-02]

# Metrics
duration: 4min
completed: 2026-02-23
---

# Phase 6 Plan 01: SDL2 Singleton and GamepadInput Hardening Summary

**Process-level SdlContext singleton with atomic ref-count eliminates SDL_Init/SDL_Quit multi-instance DAW crash; GamepadInput gains dead zone atomic, sample-and-hold on both sticks, 20ms button debounce, and dual connection callback slots.**

## Performance

- **Duration:** ~4 min
- **Started:** 2026-02-23T00:35:52Z
- **Completed:** 2026-02-23T00:40:13Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Created SdlContext.h/cpp: atomic gRefCount ensures SDL_Init fires exactly once per process and SDL_Quit fires only when the last plugin instance releases — eliminates the multi-instance DAW crash
- Refactored GamepadInput ctor/dtor to use SdlContext::acquire/release instead of raw SDL_Init/SDL_Quit; SDL hints moved into SdlContext::acquire (called only once)
- Added deadZone_ atomic and setDeadZone() — joystickThreshold APVTS param can now be fed in from processBlock (plan 06-02), removing the hard-coded kDeadzone constant
- Sample-and-hold on right stick (pitchX/Y) and left stick (filterX/Y): held value only updates when normalised axis is outside the dead zone, so pitch/filter freeze at last position instead of snapping to zero when controller released
- ButtonState struct with 20ms debounce on all 8 button inputs prevents double-fire on rapid presses
- Added onConnectionChangeUI slot alongside the existing onConnectionChange so PluginEditor (plan 06-03) can register a UI update handler without overwriting the PluginProcessor disconnect handler
- Both callback slots fired in tryOpenController() and closeController()

## Task Commits

Each task was committed atomically:

1. **Task 1: Create SdlContext process-level SDL singleton** - `0779877` (feat)
2. **Task 2: Refactor GamepadInput — SdlContext, dead zone, sample-and-hold, debounce, dual callbacks** - `cb35c78` (feat)

**Plan metadata:** (created in final commit below)

## Files Created/Modified
- `Source/SdlContext.h` - Process-level SDL2 lifecycle interface (acquire/release/isAvailable)
- `Source/SdlContext.cpp` - Atomic ref-count implementation; SDL_Init on first acquire, SDL_Quit on last release
- `Source/GamepadInput.h` - Added setDeadZone(), deadZone_ atomic, lastPitchX/Y/filterX/Y sample-and-hold fields, ButtonState debounce struct, onConnectionChangeUI slot; removed prev* bool fields
- `Source/GamepadInput.cpp` - Uses SdlContext::acquire/release; normaliseAxis reads deadZone_.load(); sample-and-hold in stick sections; debounce lambda replaces edge-detection booleans; dual callbacks fired
- `CMakeLists.txt` - Added Source/SdlContext.cpp to target_sources(ChordJoystick PRIVATE ...)

## Decisions Made
- DBG() is not available in SdlContext.cpp because the file has no JUCE header include (SDL headers only). The plan anticipated this and specified replacing DBG with `(void)SDL_GetError()` — applied as Rule 1 auto-fix during build.
- SdlContext uses file-scope static atomics (gRefCount, gAvailable) rather than a Meyers singleton or static member — avoids JUCE header dependency in a minimal SDL-only translation unit.
- setDeadZone() uses memory_order_relaxed on both store and load — the 60 Hz timer thread reads it once per tick; full sequential consistency is unnecessary overhead for this use case.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] DBG macro unavailable in SdlContext.cpp**
- **Found during:** Task 2 (build step)
- **Issue:** SdlContext.cpp does not include JUCE headers (by design — it only needs SDL.h). The DBG() macro requires juce_core. MSVC reported C3861: identifier 'DBG' not found.
- **Fix:** Replaced `DBG("SdlContext: SDL_Init failed: " << SDL_GetError())` with `(void)SDL_GetError()` and a comment explaining the error is accessible in the debugger. This is exactly the fallback the plan specified in its Note section.
- **Files modified:** Source/SdlContext.cpp
- **Verification:** Build succeeded after fix; ChordJoystick.vst3 installed to `C:\Program Files\Common Files\VST3\`
- **Committed in:** cb35c78 (Task 2 commit, SdlContext.cpp was re-staged)

---

**Total deviations:** 1 auto-fixed (Rule 1 — Bug, anticipated by plan)
**Impact on plan:** No scope change. The fix was explicitly described in the plan's Note. DBG dependency would have required pulling in JUCE headers, defeating the purpose of the SDL-only translation unit.

## Issues Encountered
None beyond the DBG macro issue documented above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- SdlContext singleton is live — any additional GamepadInput instances in the same DAW process will share a single SDL_Init
- setDeadZone() public API is ready for plan 06-02 (PluginProcessor feeds joystickThreshold)
- onConnectionChangeUI slot is ready for plan 06-03 (PluginEditor connection indicator)
- Plugin builds clean and is installed to system VST3 directory

---
*Phase: 06-sdl2-gamepad-integration*
*Completed: 2026-02-23*

## Self-Check: PASSED

- FOUND: Source/SdlContext.h
- FOUND: Source/SdlContext.cpp
- FOUND: Source/GamepadInput.h
- FOUND: Source/GamepadInput.cpp
- FOUND: .planning/phases/06-sdl2-gamepad-integration/06-01-SUMMARY.md
- FOUND commit 0779877 (Task 1: SdlContext files)
- FOUND commit cb35c78 (Task 2: GamepadInput refactor + CMakeLists)
