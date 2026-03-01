---
phase: 19-sub-octave-per-voice
plan: 01
subsystem: midi
tags: [sub-octave, midi, apvts, notecount, looper, gamepad]

# Dependency graph
requires:
  - phase: 18-single-channel-routing
    provides: sentChannel_ snapshots, noteCount_ dedup, resetNoteCount() pattern, effectiveChannel()
provides:
  - subOct0..3 APVTS bool parameters (false by default)
  - subHeldPitch_[4] — pitch snapshotted at gate-open (prevents stuck notes on joystick move)
  - looperActiveSubPitch_[4] — sub pitch snapshotted at looper gate-on (live param at emission time)
  - subOctSounding_[4] — atomic bool per voice tracking whether sub note is actively on
  - Mid-note SUB8 toggle detection loop in processBlock before TriggerSystem::processBlock
  - Looper gate-on/off sub-octave emission using live subOct param
  - resetNoteCount() extended to reset sub arrays
  - R3 gamepad combo changed: no longer calls triggerPanic(); now toggles subOctN for held pads
affects:
  - 19-02 (UI plan depends on these APVTS params existing)
  - Any future plan using sub-octave state

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Sub-pitch snapshot at gate-open to prevent stuck notes on joystick move
    - noteCount_ dedup applied to sub-octave notes (same pattern as main note dedup)
    - Mid-note toggle loop before TriggerSystem::processBlock for instant enable/disable response
    - Looper reads live APVTS param at gate-on emission, not baked into loop events

key-files:
  created: []
  modified:
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp

key-decisions:
  - "R3 alone on gamepad: no action (panic removed). R3 + held voice pad (L1/L2/R1/R2): toggles subOctN APVTS bool for that voice"
  - "Sub pitch snapshot at note-on ensures note-off always matches, even when joystick moves mid-hold"
  - "looperActiveSubPitch_ uses live param state at emission time — sub-octave not baked into recorded loop events"
  - "resetNoteCount() extended to also reset subHeldPitch_, looperActiveSubPitch_, subOctSounding_ to prevent double note-on after allNotesOff"

patterns-established:
  - "Sub-octave emission: snapshot subPitch at gate-open using noteCount_ dedup, store in subHeldPitch_[v]"
  - "Mid-note toggle detection: compare subWanted vs subSounding vs gateOpen, emit immediate note-on or note-off"

requirements-completed:
  - SUBOCT-02
  - SUBOCT-03
  - SUBOCT-04

# Metrics
duration: ~20min
completed: 2026-03-01
---

# Phase 19 Plan 01: Sub Octave Backend Summary

**Sub-octave MIDI backend: APVTS params, pitch snapshot arrays, note-on/off with noteCount_ dedup, mid-note toggle detection, looper gate sub path, and R3 gamepad combo for voice SUB8 toggle**

## Performance

- **Duration:** ~20 min
- **Started:** 2026-03-01
- **Completed:** 2026-03-01
- **Tasks:** 3
- **Files modified:** 2

## Accomplishments
- Registered subOct0..3 APVTS AudioParameterBool params (default false, saved with preset)
- Added subHeldPitch_[4], looperActiveSubPitch_[4], subOctSounding_[4] member arrays to PluginProcessor.h
- tp.onNote lambda: isOn emits sub note-on (snapshot subPitch, noteCount_ dedup, set subOctSounding_); gate-close emits sub note-off from snapshot (not live pitch-12)
- Mid-note toggle loop in processBlock before trigger_.processBlock(): detect SUB8 on/off while gate open, emit immediate note-on or note-off, no stuck notes
- Looper gateOn/gateOff: live subOct param read at emission time, looperActiveSubPitch_ snapshot for matching note-off
- resetNoteCount() extended to also reset sub arrays
- R3 gamepad: removed triggerPanic(); replaced with per-voice SUB8 toggle when held pad detected, R3 alone is silent

## Task Commits

1. **Task 1: APVTS bool params + header declarations** -  (feat)
2. **Task 2: Note-on/off sub emission, mid-note toggle, looper gate sub path, flush reset** -  (feat)
3. **Task 3: R3 gamepad combo + Release build** - included in  (feat)

## Files Created/Modified
-  - Added subHeldPitch_[4], looperActiveSubPitch_[4], std::atomic<bool> subOctSounding_[4]
-  - APVTS params, tp.onNote sub emission, mid-note toggle loop, looper gate path, resetNoteCount extension, R3 combo

## Decisions Made
- R3 alone = no-op (panic is UI-button only); R3 + held pad = subOctN toggle
- Sub pitch snapshot at gate-open ensures note-off matches even after joystick movement
- looperActiveSubPitch_ uses live param state at emission (not baked into loop)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## Next Phase Readiness
- All sub-octave APVTS parameters registered and accessible from PluginEditor via proc_.apvts
- subOctSounding_[v] readable from processor for UI brightness indication
- isGateOpen(v) already public for UI timer coloring
- 19-02 UI plan can wire ButtonParameterAttachment directly to subOct0..3

---
*Phase: 19-sub-octave-per-voice*
*Completed: 2026-03-01*
