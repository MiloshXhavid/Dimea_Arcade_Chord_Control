---
phase: 08-post-v1.0-patch-verification
plan: 01
subsystem: MIDI output / UI
tags: [patch, midi, sustain, panic, cc64]
dependency_graph:
  requires: []
  provides: [cc64-sustain-injection, one-shot-panic-button]
  affects: [Source/PluginProcessor.cpp, Source/PluginEditor.cpp]
tech_stack:
  added: []
  patterns: [MIDI CC injection before noteOn, one-shot button pattern]
key_files:
  created: []
  modified:
    - Source/PluginProcessor.cpp
    - Source/PluginEditor.cpp
decisions:
  - "CC64=127 injected on the voice's own MIDI channel (ch0+1), not filterMidiCh — sustain must be per-voice"
  - "timerCallback R3 sync block (lines 1484-1502) left untouched — scope is PATCH-04 UI button only; R3 gamepad mute deferred to Phase 09"
metrics:
  duration: "77 seconds"
  completed: "2026-02-24"
  tasks_completed: 2
  files_modified: 2
requirements_satisfied: [PATCH-01, PATCH-04]
---

# Phase 08 Plan 01: PATCH-01 and PATCH-04 Source Fixes Summary

**One-liner:** CC64=127 sustain-on injected at both note-on emission sites in PluginProcessor.cpp; panicBtn_ rewired as one-shot action with setClickingTogglesState(false) and setMidiMuted() removed.

## Tasks Completed

| # | Task | Commit | Files |
|---|------|--------|-------|
| 1 | PATCH-01: inject CC64=127 before every note-on (2 sites) | 860f5f4 | Source/PluginProcessor.cpp |
| 2 | PATCH-04: replace toggle-mute panic with one-shot wiring | bb44e47 | Source/PluginEditor.cpp |

## What Was Built

### PATCH-01: CC64=127 sustain-on injection (PluginProcessor.cpp)

Two sites modified — both now emit `controllerEvent(ch0+1, 64, 127)` immediately before the corresponding `noteOn` event:

**Site 1 — Looper gate playback block (~line 490):**
```cpp
if (loopOut.gateOn[v])
{
    // PATCH-01: CC64=127 sustain-on before every note-on
    midi.addEvent(juce::MidiMessage::controllerEvent(ch0 + 1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::noteOn(ch0 + 1, heldPitch_[v], (uint8_t)100), 0);
}
```

**Site 2 — tp.onNote lambda, isOn branch (~line 529):**
```cpp
// PATCH-01: CC64=127 sustain-on before every note-on
midi.addEvent(juce::MidiMessage::controllerEvent(ch0 + 1, 64, 127), sampleOff);
midi.addEvent(juce::MidiMessage::noteOn(ch0 + 1, pitch, (uint8_t)100),
              sampleOff);
```

Both use `ch0 + 1` (the voice's configured MIDI channel, 1-based). No CC64 was added at note-off, filter CC, or TriggerSystem sites.

### PATCH-04: One-shot panic button wiring (PluginEditor.cpp)

The panicBtn_ setup block was replaced:
- `setClickingTogglesState(false)` — button never enters toggled-on state
- `setMidiMuted()` call removed entirely from onClick
- `buttonOnColourId` colour removed (no on-state)
- `getToggleState()` / "MUTED" text / `nowMuted` logic removed from setup block
- `setTriggeredOnMouseDown(true)` retained (fires on press not release)
- onClick now calls `proc_.triggerPanic()` directly with no state side-effects

```cpp
panicBtn_.setClickingTogglesState(false);   // PATCH-04: one-shot, not toggle
// ...
panicBtn_.onClick = [this]
{
    proc_.triggerPanic();   // PATCH-04: one-shot — fires allNotesOff this block, button stays pressable
};
```

The R3 gamepad toggle-mute path in processBlock (~line 379) and the timerCallback UI sync block (~line 1484) were left untouched — both are deferred to Phase 09.

## Verification Results

| Check | Command | Result |
|-------|---------|--------|
| PATCH-01: exactly 2 CC64=127 injection sites | `grep -n "controllerEvent.*64.*127" Source/PluginProcessor.cpp` | 2 matches (lines 493, 532) |
| PATCH-01: both use ch0+1 (no hardcoded channel) | `grep -n "controllerEvent.*64" Source/PluginProcessor.cpp` | Both reference `ch0 + 1` |
| PATCH-04: toggle removed | `grep -n "setClickingTogglesState" Source/PluginEditor.cpp` | Only `(false)` for panicBtn_ |
| PATCH-04: setMidiMuted removed | `grep -n "setMidiMuted" Source/PluginEditor.cpp` | 0 results |
| PATCH-04: triggerPanic in onClick | `grep -n "triggerPanic" Source/PluginEditor.cpp` | 1 functional call in onClick lambda |

## Deviations from Plan

None — plan executed exactly as written. The timerCallback R3 sync block at lines 1484-1502 was noted but confirmed out of scope per plan instruction: "R3 gamepad toggle-mute path remains UNCHANGED — deferred to Phase 09."

## Decisions Made

1. **CC64=127 on voice channel only** — Each injection uses `ch0 + 1` (the voice's own MIDI channel from `voiceChs[v]`), not `filterMidiCh`. Sustain must be sent per-voice so downstream synths on channels 1-4 each receive their own sustain pedal event.

2. **timerCallback R3 sync left untouched** — Lines 1484-1502 in `timerCallback` sync `panicBtn_` toggle state to `proc_.isMidiMuted()` (for R3 gamepad). This is pre-existing R3 functionality explicitly deferred to Phase 09. PATCH-04 only fixes the UI button's click wiring.

## Self-Check: PASSED

Files verified to exist:
- Source/PluginProcessor.cpp — modified, contains 2 CC64=127 injection sites
- Source/PluginEditor.cpp — modified, panicBtn_ uses setClickingTogglesState(false)

Commits verified:
- 860f5f4 — PATCH-01 fix
- bb44e47 — PATCH-04 fix
