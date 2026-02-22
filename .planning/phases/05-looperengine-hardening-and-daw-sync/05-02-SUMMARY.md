---
phase: 05-looperengine-hardening-and-daw-sync
plan: "02"
subsystem: audio-engine
tags: [juce, playhead, daw-sync, juce8-api, looper, ui, buttons, midi]

# Dependency graph
requires:
  - phase: 05-01
    provides: Lock-free LooperEngine with setRecJoy/setRecGates/setSyncToDaw/isCapReached API

provides:
  - PluginProcessor exposes 7 new forwarding methods for LooperEngine mode control
  - processBlock uses JUCE 8 AudioPlayHead::getPosition() Optional API (deprecated getCurrentPosition removed)
  - Looper gate playback emits MIDI directly via midi.addEvent, bypassing TriggerSystem
  - PluginEditor has REC JOY, REC GATES, SYNC buttons wired to new forwarding methods
  - capReached flash indicator in timerCallback at ~5 Hz

affects: [05-03-looperengine-verification]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "JUCE 8 AudioPlayHead::getPosition() returns Optional<PositionInfo>; use .hasValue() + .getPpqPosition()/.getBpm()/.getIsPlaying() accessors"
    - "Looper gate playback bypasses TriggerSystem: midi.addEvent directly so live pad input coexists without conflict"
    - "capFlashCounter static int in timerCallback cycles 0-5 at 30Hz for ~5 Hz flash effect on capReached"
    - "New looper buttons use setClickingTogglesState(true) but state is mirrored from LooperEngine atomics in timerCallback, not from APVTS"

key-files:
  created: []
  modified:
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp

key-decisions:
  - "JUCE 8 getPosition() Optional API: getPosition() returns Optional<PositionInfo>; each field also Optional (ppqPosition, bpm); replaced all pos.ppqPosition/pos.isPlaying/pos.bpm references with ppqPos/isDawPlaying/dawBpm local variables"
  - "Looper gate playback bypasses TriggerSystem: loopOut.gateOn/gateOff emit midi.addEvent directly using heldPitch_[v] and voiceChs[]; TriggerSystem remains unaware of looper events so live pad input passes through independently"
  - "Gate recording in tp.onNote lambda: both note-on and note-off paths now call looper_.recordGate() with consistent beatPos logic (hasDaw && ppqPos >= 0.0) ? ppqPos : looper_.getPlaybackBeat()"
  - "Button layout: row 2 added below existing PLAY/REC/RST/DEL row; 3-column equal-width layout (bw3 = (width-4)/3) for REC JOY | REC GATES | SYNC"
  - "No APVTS attachment for new buttons: recJoy_/recGates_/syncToDaw_ are LooperEngine atomics, not APVTS parameters; timerCallback polls and mirrors state at 30 Hz"

patterns-established:
  - "JUCE 8 PlayHead pattern: if (auto* head = getPlayHead()) { if (auto posOpt = head->getPosition(); posOpt.hasValue()) { ... } }"
  - "Direct MIDI emission pattern for looper: avoids calling TriggerSystem for looper-originated events"

requirements-completed: [LOOPER-02, LOOPER-03]

# Metrics
duration: ~24min
completed: 2026-02-22
---

# Phase 05 Plan 02: LooperEngine DAW Wiring Summary

**JUCE 8 AudioPlayHead API wired into processBlock; 7 looper forwarding methods added to PluginProcessor; REC JOY/REC GATES/SYNC buttons with capReached flash added to PluginEditor**

## Performance

- **Duration:** ~24 min
- **Started:** 2026-02-22T21:47:08Z
- **Completed:** 2026-02-22T21:50:41Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- Replaced deprecated JUCE 7 `getCurrentPosition(CurrentPositionInfo&)` API with JUCE 8 `getPosition()` returning `Optional<PositionInfo>` — ppqPos/dawBpm/isDawPlaying extracted with null-safe accessors
- Added 7 forwarding methods to PluginProcessor: `looperSetRecJoy`, `looperSetRecGates`, `looperSetSyncToDaw`, `looperIsCapReached`, `looperIsSyncToDaw`, `looperIsRecJoy`, `looperIsRecGates`
- Fixed looper gate playback model: replaced `trigger_.setPadState()` calls (which caused looper to block live pad input) with direct `midi.addEvent()` calls using `heldPitch_[v]` — live pad input now coexists independently
- Updated `tp.onNote` lambda gate recording: both note-on and note-off branches use `ppqPos` (not the old `pos.ppqPosition`)
- Added [REC JOY], [REC GATES], [SYNC] buttons in PluginEditor with click lambdas, 30 Hz timerCallback state mirror, and ~5 Hz capReached flash indicator

## Task Commits

Each task was committed atomically:

1. **Task 1: Update PluginProcessor** - `290849c` (feat)
2. **Task 2: Update PluginEditor** - `fb1a71c` (feat)

## Files Created/Modified

- `Source/PluginProcessor.h` - Added 7 new looper forwarding methods in the "Looper transport" public section after existing looperIsRecording()
- `Source/PluginProcessor.cpp` - Replaced deprecated JUCE 7 PlayHead API with JUCE 8 Optional API; replaced looper gate override block (trigger_.setPadState) with direct MIDI emission bypassing TriggerSystem; updated tp.onNote lambda and looper joystick recording block to use ppqPos
- `Source/PluginEditor.h` - Declared loopRecJoyBtn_, loopRecGatesBtn_, loopSyncBtn_ in the Looper section
- `Source/PluginEditor.cpp` - Constructor: addAndMakeVisible + setButtonText + onClick lambdas for 3 new buttons; resized(): second looper button row (3-column layout); timerCallback(): toggle state mirror + capFlashCounter ~5 Hz flash

## Decisions Made

- **JUCE 8 getPosition() Optional API:** Each field (ppqPosition, bpm, isPlaying) is independently Optional. Code uses `posOpt->getIsPlaying()` (bool, no Optional), `posOpt->getPpqPosition()` (Optional<double>), `posOpt->getBpm()` (Optional<double>). All field optionals checked with `.hasValue()` before dereferencing.
- **Looper gate playback bypasses TriggerSystem:** `loopOut.gateOn[v]` / `loopOut.gateOff[v]` emit MIDI directly via `midi.addEvent()` using `heldPitch_[v]` and `voiceChs[v]-1`. TriggerSystem receives no notification of looper gate events, so live pad state flows independently. This satisfies the locked CONTEXT.md Section B decision.
- **Button layout — second row:** 3-column equal-width row (28px tall) directly below the existing 4-button row (36px tall) with 2px gap. Width computed as `(row.getWidth() - 4) / 3` with 2px gaps between columns.
- **No APVTS attachments for new buttons:** `recJoy_`, `recGates_`, `syncToDaw_` are LooperEngine atomics driven by `setRecJoy/setRecGates/setSyncToDaw`. They are not APVTS parameters. State is polled in `timerCallback()` via `proc_.looperIsRecJoy()` etc. and mirrored to button toggle state.

## Exact Gate Recording Location

Gate recording calls (`looper_.recordGate()`) are inside the `tp.onNote` lambda, called by `trigger_.processBlock(tp)`. The lambda fires on the audio thread for each gate state change. Both note-on and note-off branches call `recordGate()` before the corresponding `midi.addEvent()`. Beat position is computed as `(hasDaw && ppqPos >= 0.0) ? ppqPos : looper_.getPlaybackBeat()`.

## Live-Input Passthrough Design

- **Before this plan:** `loopOut.gateOn[v]` called `trigger_.setPadState(v, true)` — this would compete with live pad input and could suppress live notes during playback
- **After this plan:** `loopOut.gateOn[v]` calls `midi.addEvent(noteOn(voiceChs[v], heldPitch_[v], 100), 0)` directly — TriggerSystem is never informed of looper gate events, so live pad gates and looper playback gates coexist as independent MIDI streams on the same channels

## Button Layout Coordinates

The second looper button row is positioned by:
```cpp
auto row2 = section.removeFromTop(28);
const int bw3 = (row2.getWidth() - 4) / 3;
loopRecJoyBtn_  .setBounds(row2.removeFromLeft(bw3)); row2.removeFromLeft(2);
loopRecGatesBtn_.setBounds(row2.removeFromLeft(bw3)); row2.removeFromLeft(2);
loopSyncBtn_    .setBounds(row2);
```
This places the three buttons in equal thirds of the looper section width, with 2px gaps, immediately below the PLAY/REC/RST/DEL row.

## Build Status

- ChordJoystick.vst3: built and installed to `C:\Program Files\Common Files\VST3\ChordJoystick.vst3`
- 0 compiler errors, 0 compiler warnings (C/C++)
- All 26 Catch2 tests pass (655 assertions)

## Deviations from Plan

### Auto-fixed Issues

None — plan executed exactly as written.

No pre-existing deprecated API calls were hidden. The JUCE 8 Optional API change was a clean replacement with no edge cases not covered by the plan spec.

---

*Phase: 05-looperengine-hardening-and-daw-sync*
*Completed: 2026-02-22*

## Self-Check: PASSED

- FOUND: Source/PluginProcessor.h
- FOUND: Source/PluginProcessor.cpp
- FOUND: Source/PluginEditor.h
- FOUND: Source/PluginEditor.cpp
- FOUND: .planning/phases/05-looperengine-hardening-and-daw-sync/05-02-SUMMARY.md
- FOUND: commit 290849c (Task 1)
- FOUND: commit fb1a71c (Task 2)
- Tests: All 26 passed (655 assertions)
