---
phase: 10-trigger-quantization-infrastructure
plan: 02
subsystem: api
tags: [juce, apvts, quantize, midi, vst3, cpp17]

# Dependency graph
requires:
  - phase: 10-trigger-quantization-infrastructure
    provides: Phase 10 context and quantize architecture decisions from 10-RESEARCH.md

provides:
  - quantizeMode APVTS parameter (int 0-2, default 0=Off) persisted via DAW state
  - quantizeSubdiv APVTS parameter (int 0-3, default 1=1/8) persisted via DAW state
  - PluginProcessor::setQuantizeMode / getQuantizeMode passthrough API
  - PluginProcessor::setQuantizeSubdiv / getQuantizeSubdiv passthrough API
  - PluginProcessor::looperApplyQuantize / looperRevertQuantize / looperQuantizeIsActive stubs

affects:
  - 10-03 (LooperEngine quantize backend wires into these stubs)
  - 10-04 (PluginEditor UI uses setQuantizeMode/setQuantizeSubdiv/getQuantizeMode)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "APVTS int param registration pattern: addInt(ParamID::x, label, min, max, default)"
    - "Setter pattern: apvts.getParameter(id)->setValueNotifyingHost(p->convertTo0to1(float(val)))"
    - "Getter pattern: auto* v = apvts.getRawParameterValue(id); return v ? int(*v) : fallback"
    - "Stub methods in .h allow UI plan (10-04) to compile before LooperEngine plan (10-03) lands"

key-files:
  created: []
  modified:
    - Source/PluginProcessor.cpp
    - Source/PluginProcessor.h

key-decisions:
  - "quantizeMode string literal used in .h methods (not ParamID namespace) because namespace is file-scope in .cpp and not accessible from .h"
  - "Stub methods return false/no-op to let Plan 10-04 (UI) compile ahead of Plan 10-03 (LooperEngine backend)"
  - "quantizeMode default=0 (Off) — quantize is opt-in, never applied silently on first load"
  - "quantizeSubdiv default=1 (1/8) — most common subdivision for quantize in DAW workflows"

patterns-established:
  - "Stub pattern: leave commented looper_.method() in stub with Plan reference so future plan knows exactly where to wire it"
  - "Null guard on getRawParameterValue: always check pointer before dereference"

requirements-completed: [QUANT-01, QUANT-02, QUANT-03]

# Metrics
duration: 12min
completed: 2026-02-25
---

# Phase 10 Plan 02: APVTS Quantize Parameters and Processor API Summary

**Two APVTS parameters (quantizeMode 0-2, quantizeSubdiv 0-3) registered with persistence, plus 7 PluginProcessor passthrough methods enabling UI and LooperEngine integration in subsequent plans**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-02-25T00:00:00Z
- **Completed:** 2026-02-25T00:12:00Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Registered `quantizeMode` (int 0-2, default 0=Off) and `quantizeSubdiv` (int 0-3, default 1=1/8) in APVTS — DAW now persists both values across sessions automatically
- Added 4 passthrough methods: `setQuantizeMode`, `setQuantizeSubdiv`, `getQuantizeMode`, `getQuantizeSubdiv` with correct JUCE parameter value conversion and null guards
- Added 3 stub methods: `looperApplyQuantize`, `looperRevertQuantize`, `looperQuantizeIsActive` allowing Plan 10-04 (UI) to compile before Plan 10-03 (LooperEngine backend) lands
- Plugin builds cleanly (ChordJoystick Debug confirmed)

## Task Commits

Each task was committed atomically:

1. **Task 1: Register quantizeMode and quantizeSubdiv in APVTS parameter layout** - `4f1acf8` (feat)
2. **Task 2: Add quantize passthrough methods to PluginProcessor** - `742877e` (feat)

**Plan metadata:** (docs commit pending)

## Files Created/Modified

- `Source/PluginProcessor.cpp` - ParamID namespace extended with `quantizeMode` + `quantizeSubdiv`; `createParameterLayout()` extended with both `addInt()` calls after the Arpeggiator section
- `Source/PluginProcessor.h` - 7 new public methods added after `looperArmWait()`: 4 get/set methods + 3 looper stubs

## Decisions Made

- **String literals in .h, not ParamID namespace:** The `ParamID` namespace is file-scope inside `PluginProcessor.cpp` and is not accessible from the header. Methods in `.h` use string literals `"quantizeMode"` and `"quantizeSubdiv"` directly, consistent with how existing editor code already calls `apvts.getRawParameterValue("looperSubdiv")` etc.
- **Stub methods return safe defaults:** `looperQuantizeIsActive()` returns `false`, stubs are no-ops. This lets Plan 10-04 UI compile and test independently of Plan 10-03.
- **Setter uses `convertTo0to1`:** Integer parameter setters must convert the raw int value through `p->convertTo0to1()` before calling `setValueNotifyingHost()`, matching the JUCE API contract for normalized parameter values.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- `cmake --build build --target ChordJoystickTests` failed with a pre-existing SDL2 FetchContent tlog error (MSB4018 ReadCommandLines) unrelated to this plan's changes. The main `ChordJoystick` target built successfully. Test target issue is a build environment artifact present before this plan.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 10-03 (LooperEngine backend): `looper_.setQuantizeMode()`, `looper_.setQuantizeSubdiv()`, `looper_.applyQuantize()`, `looper_.revertQuantize()`, `looper_.quantizeIsActive()` methods need to be added to LooperEngine, then the stubs in PluginProcessor.h can be uncommented
- Plan 10-04 (UI): Can now call `processor.setQuantizeMode(n)`, `processor.setQuantizeSubdiv(n)`, `processor.getQuantizeMode()`, `processor.getQuantizeSubdiv()`, `processor.looperApplyQuantize()` etc. — all compile correctly
- APVTS persistence is already wired — DAW will save/restore quantize settings across sessions with no additional work

---
*Phase: 10-trigger-quantization-infrastructure*
*Completed: 2026-02-25*
