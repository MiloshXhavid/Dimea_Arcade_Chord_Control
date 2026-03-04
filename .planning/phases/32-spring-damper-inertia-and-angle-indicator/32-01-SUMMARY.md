---
phase: 32-spring-damper-inertia-and-angle-indicator
plan: 01
subsystem: UI / Visual
tags: [spring-damper, inertia, arc, compass, joystick, paint, timerCallback]
dependency_graph:
  requires: [Phase 31 — space visual foundation, glow ring]
  provides: [spring-smoothed cursor dot, perimeter arc, note-label compass]
  affects: [JoystickPad::timerCallback, JoystickPad::paint]
tech_stack:
  added: []
  patterns:
    - Spring-damper integration at 60 Hz (kSpring=0.18, kDamping=0.72)
    - First-frame snap guard using all-zero state check
    - Raw vs. smoothed position split: raw cx/cy for particles/arc, displayCx_/Cy_ for cursor dot
    - atan2(-rawY, rawX) convention for JUCE Y-flipped polar angle
key_files:
  created: []
  modified:
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp
decisions:
  - "Spring target (raw LFO-aware pixel) computed in timerCallback(); particles and arc always read raw, never smoothed"
  - "First-frame snap uses all-zero guard (springVelX_==0 && springVelY_==0 && displayCx_==0 && displayCy_==0) — avoids ramp from origin on plugin load"
  - "No clamping on displayCx_/Cy_ in timerCallback() — ±1-3 px overshoot is intentional for physical feel"
  - "Arc reads proc_.joystickX/Y directly so direction is always current, not lagged"
  - "Compass labels normalized with while-loop angular diff instead of fmod to handle full wrap correctly"
metrics:
  duration: ~25 min
  completed: 2026-03-04
  tasks_completed: 4
  tasks_total: 4
  files_changed: 2
---

# Phase 32 Plan 01: Spring-Damper Inertia and Angle Indicator Summary

**One-liner:** Spring-damper cursor inertia (kSpring=0.18, kDamping=0.72) with perimeter arc + Root/Third/Fifth/Tension compass on JoystickPad.

## What Was Built

Added visual inertia and direction feedback to the JoystickPad:

1. **Spring-damper (Task 1-2):** Four new private members (`displayCx_`, `displayCy_`, `springVelX_`, `springVelY_`) on `JoystickPad`. Each `timerCallback()` tick advances the spring toward the raw LFO-aware pixel target (`cx`/`cy`). The smoothed position is stored in `displayCx_`/`displayCy_` for use by `paint()`. Gold particles and the note burst still spawn from the raw `cx`/`cy` so they lead the dot slightly on fast moves.

2. **paint() cursor (Task 3A):** Removed the redundant LFO-branch + `jlimit` recomputation from `paint()`. The cursor dot now reads `displayCx_`/`displayCy_` directly — one fewer per-frame atomic load, cleaner code.

3. **Perimeter arc (Task 3B):** Layer 4b — a 40° cyan arc at the inscribed circle edge pointing in the raw joystick's polar direction. Reads `proc_.joystickX`/`Y` directly (not the smoothed dot). Hidden when joystick magnitude < 0.08.

4. **Note-label compass (Task 3C):** Layer 4c — Root (bottom), Third (top), Fifth (left), Tension (right) labels at the circle perimeter. Idle alpha 0.38; active label (within 50° of arc angle) brightens to 0.75.

## Commits

| Task | Hash | Message |
|------|------|---------|
| 1 | a345728 | feat(32-01): add spring-damper private members to JoystickPad |
| 2 | 299f1c2 | feat(32-01): apply spring-damper update in timerCallback() |
| 3 | 2b9319a | feat(32-01): paint() spring cursor + perimeter arc + note-label compass |

## Build Result

- 0 compiler errors, 0 new warnings
- VST3 artifact: `build/ChordJoystick_artefacts/Release/VST3/Arcade Chord Control (BETA-Test).vst3`
- Installed to: `C:\Program Files\Common Files\VST3\Arcade Chord Control (BETA-Test).vst3`

## Deviations from Plan

None — plan executed exactly as written. Used the all-zero first-frame guard form as specified (`springVelX_ == 0.0f && springVelY_ == 0.0f && displayCx_ == 0.0f && displayCy_ == 0.0f`), not the `prevCx_ < -999.0f` form.

## Self-Check: PASSED

- Source/PluginEditor.h: FOUND
- Source/PluginEditor.cpp: FOUND
- Commit a345728 (Task 1): FOUND
- Commit 299f1c2 (Task 2): FOUND
- Commit 2b9319a (Task 3): FOUND
- VST3 artifact: FOUND in build/ChordJoystick_artefacts/Release/VST3/
