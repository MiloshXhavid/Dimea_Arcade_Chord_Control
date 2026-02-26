---
phase: 15-gamepad-preset-control
plan: "02"
subsystem: processor+editor
tags: [program-change, midi, preset-scroll, gamepad, ui-indicator]

# Dependency graph
requires:
  - phase: 15-01
    provides: GamepadInput isPresetScrollActive() + consumePcDelta() API
provides:
  - MIDI Program Change routing in processBlock via programCounter_
  - " | OPTION" UI indicator in gamepadStatusLabel_ when preset-scroll mode active
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "juce::MidiMessage::programChange(1-based channel, 0-based program) at sample offset 0"
    - "Audio-thread-only int counter (no atomic needed) for program number tracking"
    - "Boundary-skip pattern: only send PC when counter actually changes (jlimit + compare)"
    - "30 Hz label poll: append/strip text suffix + setColour per mode state"

key-files:
  created: []
  modified:
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp
    - Source/PluginEditor.cpp

key-decisions:
  - "programCounter_ is audio-thread-only int (no atomic) — never read from message thread"
  - "juce::MidiMessage::programChange takes 1-based channel (1-16) and 0-based program (0-127) — filterMidiCh stores 1-based directly, no conversion needed"
  - "Boundary clamp uses jlimit + equality check: if newProgram == programCounter_ after clamp, skip the MIDI send — preserves must_have truth that pressing at boundary does nothing"
  - "D-pad Left/Right (looper toggles) remain outside the if/else branch — always active regardless of preset-scroll mode"
  - "OPTION indicator: text suffix pattern (base + ' | OPTION') strips cleanly without blink/animation — satisfies CONTEXT.md subtle constraint"

# Metrics
duration: 4min
completed: 2026-02-26
---

# Phase 15 Plan 02: Gamepad Preset Control — PC Routing + UI Indicator Summary

**MIDI Program Change routing in processBlock via programCounter_ (0-127 clamped, 1-based filterMidiCh) and " | OPTION" suffix indicator in gamepadStatusLabel_ polled at 30 Hz in timerCallback**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-26T06:30:30Z
- **Completed:** 2026-02-26T06:34:27Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- Added `int programCounter_ = 0` to PluginProcessor private section (audio-thread-only, not persisted)
- Replaced flat D-pad Up/Down block in processBlock with mode-branched version:
  - Preset-scroll active: `consumePcDelta()` → `juce::MidiMessage::programChange(fCh, programCounter_)` added to MidiBuffer at offset 0
  - Normal mode: `consumeDpadUp/Down()` → `pendingBpmDelta_` (unchanged behavior)
- Boundary clamp: `jlimit(0, 127, programCounter_ + pcDelta)` with equality check prevents send at 0 or 127 edges
- D-pad Left/Right (looper toggles) remain unconditional — not affected by preset-scroll mode
- Added OPTION indicator block in `timerCallback()`:
  - Polls `isPresetScrollActive()` every 30 Hz tick
  - Appends `" | OPTION"` suffix to existing controller name text, sets `Clr::highlight` color
  - Strips suffix cleanly on deactivation; restores `Clr::gateOn` (connected) or `Clr::textDim` (disconnected)
- Both tasks build with zero errors, zero new warnings

## Task Commits

Each task was committed atomically:

1. **Task 1: Add program counter and PC routing to PluginProcessor** - `9489ea9` (feat)
2. **Task 2: Add OPTION mode indicator to PluginEditor** - `1720a71` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified

- `Source/PluginProcessor.h` - Added `int programCounter_ = 0` after arpWaitingForPlay_/prevArpOn_ arp state members
- `Source/PluginProcessor.cpp` - Replaced D-pad BPM block with if/else: preset-scroll path calls consumePcDelta() and emits programChange; normal path preserves existing consumeDpadUp/Down + pendingBpmDelta_
- `Source/PluginEditor.cpp` - Added OPTION indicator block in timerCallback() after BPM display update, before filter CC section

## Decisions Made

- `programCounter_` is audio-thread-only int — no atomic needed since it is never read from message thread (only written and read in processBlock on audio thread)
- `juce::MidiMessage::programChange(channel, program)` uses 1-based channel (1-16) and 0-based program number (0-127) — `filterMidiCh` APVTS parameter stores 1-based values directly, so no conversion needed
- Boundary clamp: if `jlimit(0, 127, programCounter_ + pcDelta) == programCounter_` after clamping, skip the MIDI send — preserves the "pressing at boundary does nothing" requirement
- D-pad Left/Right left outside the if/else: they always affect looper record toggles regardless of preset-scroll mode
- OPTION indicator uses text suffix pattern instead of a separate widget — satisfies the "subtle, secondary visual weight, no blinking" constraint from CONTEXT.md

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 15 complete: GamepadInput (Plan 01) + PluginProcessor PC routing + PluginEditor OPTION indicator (Plan 02)
- All CTRL-01, CTRL-02, CTRL-03 requirements satisfied
- v1.5 Gamepad Preset Control milestone complete

---
*Phase: 15-gamepad-preset-control*
*Completed: 2026-02-26*
