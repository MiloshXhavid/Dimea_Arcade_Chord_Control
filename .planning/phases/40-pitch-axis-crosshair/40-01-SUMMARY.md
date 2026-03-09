---
phase: 40-pitch-axis-crosshair
plan: 01
subsystem: ui
tags: [juce, crosshair, visualization, atomic, apvts, joystick, pitch-display]

# Dependency graph
requires:
  - phase: 38-quick-fixes-rec-lane-undo
    provides: stable processBlock LFO application and modulatedJoyX_/Y_ atomics pattern
  - phase: 32-spring-damper-inertia-and-angle-indicator
    provides: displayCx_/displayCy_ spring-smoothed cursor position in JoystickPad
provides:
  - 4 livePitch_ atomics (int) written every processBlock after LFO application
  - crosshairVisible APVTS bool param (default ON, automatable, preset-persisted)
  - JoystickPad crosshair: two half-segment lines from cursor to pad center axes
  - Four octave-qualified note name labels flanking the line midpoints
  - Right-click toggle with discoverability dot when off
  - Center suppression (|x|+|y| < 0.01 hides crosshair)
affects:
  - phase-41-smart-chord-display
  - phase-42-warp-space-effect

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Cross-thread pitch display: audio thread writes std::atomic<int> livePitch_, UI timer reads at 30 Hz — one-block latency (~6ms), inaudible for display"
    - "APVTS bool read in paint() via getRawParameterValue()->load() — lock-free from message thread"
    - "Right-click toggle via mouseUp e.mods.isRightButtonDown() — sets param value 0.0/1.0 via setValueNotifyingHost"
    - "Center suppression: abs(mx)+abs(my) < 0.01f skips all crosshair drawing"
    - "Note name format: octave = midiNote/12 - 1 (C4=MIDI60, not JUCE C5 convention)"

key-files:
  created: []
  modified:
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp

key-decisions:
  - "[Phase 40]: livePitch_ stores inserted after chordP.joystickX/Y clamp (post-LFO block) — same resolved position as actual note-ons"
  - "[Phase 40]: crosshairVisible APVTS bool follows same inline addBool pattern as stickSwap/stickInvert — no ParamID namespace entry needed"
  - "[Phase 40]: Note name uses manual octave formula (midiNote/12 - 1) rather than JUCE getMidiNoteName — matches C4=60 convention not JUCE's C5=60"
  - "[Phase 40]: Faint discoverability dot (Clr::accent at 0.15 alpha, 4px radius) at cursor position when crosshair is toggled OFF"
  - "[Phase 40]: kCollisionR=20px label suppression prevents label overlap with cursor sprite at close range"

patterns-established:
  - "Crosshair half-segments: drawLine from displayCx_/displayCy_ to padCx (horizontal) and padCy (vertical)"
  - "Label placement: horizontal segment root above / third below midpoint; vertical segment fifth left / tension right midpoint"

requirements-completed: [VIZ-01, VIZ-02, VIZ-03, VIZ-04]

# Metrics
duration: ~35min
completed: 2026-03-09
---

# Phase 40 Plan 01: Pitch Axis Crosshair Summary

**Real-time pitch crosshair on JoystickPad: 4 livePitch_ atomics written post-LFO every block, two half-segment lines with octave-qualified note name labels, right-click toggle persisted in APVTS**

## Performance

- **Duration:** ~35 min
- **Started:** 2026-03-09T00:30:00Z
- **Completed:** 2026-03-09T01:05:00Z
- **Tasks:** 2 auto + 1 checkpoint (UAT approved)
- **Files modified:** 4

## Accomplishments

- 4 `livePitch_` atomics added to PluginProcessor, written after the LFO clamp block so they reflect the same resolved joystick position used for actual note-ons
- `crosshairVisible` APVTS bool param (default ON) added — automatable by DAW, persists in presets
- JoystickPad: 30 Hz pitch cache update, right-click toggle, crosshair paint block with two half-segments, four octave-qualified note name labels, center suppression, and faint discoverability dot
- Human UAT: all 10 checks passed — real-time updates, correct pitches, center suppression, right-click toggle, preset round-trip, automation lane visibility, scale quantization reflected

## Task Commits

1. **Task 1: Processor atomics + APVTS param + processBlock write** - `a1e50ad` (feat)
2. **Task 2: JoystickPad pitch cache, right-click toggle, crosshair paint** - `4aa10a2` (feat)

## Files Created/Modified

- `Source/PluginProcessor.h` — Added `livePitch0_` through `livePitch3_` as public `std::atomic<int>` members
- `Source/PluginProcessor.cpp` — Added `crosshairVisible` bool in `createParameterLayout()`; added livePitch store loop after LFO clamp in `processBlock`
- `Source/PluginEditor.h` — Added `livePitch_[4]` cache in JoystickPad private section
- `Source/PluginEditor.cpp` — timerCallback pitch cache update; mouseUp right-click toggle; paint() crosshair drawing block

## Decisions Made

- livePitch_ stores placed after `chordP.joystickX/Y = std::clamp(...)` lines so they use the fully LFO-resolved position, identical to note-on pitch computation
- Manual octave formula `midiNote/12 - 1` used for note names — JUCE's `getMidiNoteName` uses C5=60 convention, manual gives C4=60 matching standard expectation
- `kCollisionR = 20px` label suppression radius prevents note name labels from overlapping the cursor sprite at close range
- Faint `Clr::accent` dot at 0.15 alpha rendered at cursor when crosshair is off — provides a subtle discoverability hint without cluttering the space visual

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- `livePitch_` atomics and `crosshairVisible` APVTS param available for Phase 41 (Smart Chord Display) which may read the same live pitch values
- No blockers

---
*Phase: 40-pitch-axis-crosshair*
*Completed: 2026-03-09*
