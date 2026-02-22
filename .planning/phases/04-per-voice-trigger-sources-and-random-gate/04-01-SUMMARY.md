---
phase: 04-per-voice-trigger-sources-and-random-gate
plan: 01
subsystem: trigger
tags: [juce, midi, trigger, joystick, continuous-gate, pitch-bend, rpn, vst3, cpp]

# Dependency graph
requires:
  - phase: 03-core-midi-output-and-note-off-guarantee
    provides: TriggerSystem with resetAllGates(), processBlockBypassed(), note-off guarantee

provides:
  - joystickThreshold APVTS param (0.001..0.1, default 0.015) controlling gate sensitivity
  - Continuous joystick gate model in TriggerSystem (Chebyshev magnitude)
  - Quantized pitch bend JOY gate model: note held at base pitch, pitchWheel tracks scale degrees
  - joyBasePitch_[4]: MIDI note fired at gate open (bend reference, always matches note-on)
  - joyLastBendValue_[4]: last sent bend value, avoids redundant pitchWheel messages
  - joystickStillSamples_[4] debounce counter: fires note-off after 50ms of stillness below threshold
  - MidiCallback onMidi in ProcessParams for pitch bend / CC / RPN messages
  - RPN bend-range setup (+-24 semitones) sent on gate open before note-on
  - pitchWheel reset to 0 before note-off on gate close and processBlockBypassed
  - Horizontal THRESHOLD slider in PluginEditor right column with SliderParameterAttachment
  - TouchPlate dimming and mouseDown/mouseUp no-op guard in JOY and RND modes

affects:
  - 04-02 (random gate -- shares TriggerSystem per-voice infrastructure)
  - 06-sdl2-gamepad (notifyJoystickMoved stub may be revisited)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Chebyshev distance (max|dx|, |dy|) for joystick motion magnitude -- avoids diagonal bias
    - Quantized pitch bend: bend = (heldPitch - basePitch) / 24 * 8191, clamped to +-24 semitones
    - RPN sequence for bend range: CC101=0, CC100=0, CC6=24, CC38=0, CC101=127, CC100=127
    - MidiCallback onMidi in ProcessParams alongside NoteCallback onNote -- raw MidiMessage passthrough
    - getRawParameterValue() load() from paint() message thread -- safe for atomic<float>*

key-files:
  created: []
  modified:
    - Source/TriggerSystem.h
    - Source/TriggerSystem.cpp
    - Source/PluginProcessor.cpp
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp

key-decisions:
  - "Pitch bend over legato note tracking (final): bend requires RPN setup but avoids note-on/off polling and is more expressive; heldPitches already scale-quantized so bend snaps to in-scale degrees only"
  - "joyBasePitch_[v] is the MIDI note number sent at note-on time; synth tracks this for note-off even while bend moves the sounding pitch"
  - "bend range +-24 semitones: covers 2 octaves, sufficient for any quantizer zone jump on a standard 5-octave joystick range"
  - "MidiCallback onMidi added to ProcessParams: allows TriggerSystem to emit CC/pitchWheel/RPN without coupling to MidiBuffer"
  - "pitchWheel reset before note-off: ensures synth pitch bend state returns to centre after gate closes, preventing tuning drift on next note-on"
  - "processBlockBypassed emits pitchWheel(ch, 0) per voice before note-off: cleans up any lingering bend when plugin is bypassed"
  - "Gate closes after 50ms debounce below threshold -- joystickStillSamples_[v] accumulates blockSize each block"
  - "Chebyshev distance: max(|dx|, |dy|) over Manhattan -- avoids diagonal-motion double-triggering"

patterns-established:
  - "Quantized pitch bend pattern: fire note-on at base pitch, send pitchWheel for scale-degree offsets, reset bend before note-off"
  - "MidiCallback pattern: ProcessParams carries onNote (note-on/off) + onMidi (everything else) -- callers wire both to MidiBuffer"

requirements-completed: [TRIG-01, TRIG-02]

# Metrics
duration: ~60min
completed: 2026-02-22
---

# Phase 4 Plan 1: Joystick Continuous Gate + Quantized Pitch Bend Summary

**Chebyshev-magnitude continuous JOY gate model with quantized pitch bend (note held at base pitch, pitchWheel tracks scale-quantized degree offsets), RPN bend-range setup on gate open, bend reset before note-off, 50ms stillness debounce, APVTS joystickThreshold param, THRESHOLD slider in UI, TouchPlate dimming in JOY/RND modes**

## Performance

- **Duration:** ~60 min (across original plan + four iterative fixes)
- **Completed:** 2026-02-22
- **Tasks:** All tasks and fixes complete
- **Files modified:** 5 (TriggerSystem.h, TriggerSystem.cpp, PluginProcessor.cpp, PluginEditor.h, PluginEditor.cpp)

## Accomplishments

- Added `joystickThreshold` APVTS parameter (0.001..0.1, default 0.015) and horizontal THRESHOLD slider in right column
- Replaced one-shot delta-based joystick trigger with sustained continuous gate model (open on threshold-crossing)
- Implemented quantized pitch bend for JOY gate: gate opens with `noteOn(basePitch)` + RPN bend-range setup (±24 semitones); while open, `pitchWheel` messages track `heldPitches[v] - joyBasePitch_[v]` offset (converted to ±8191 MIDI range); since `heldPitches` is already scale-quantized by ChordEngine, pitch bend snaps to in-scale degrees only
- Gate closes automatically after 50ms of stillness below threshold: `joystickStillSamples_[v]` accumulates `blockSize` each block; when `>= sampleRate * 0.05`, resets bend to 0 then fires `noteOff(joyBasePitch_[v])` — note-off targets the original note-on pitch so synth releases the correct note regardless of current bend position
- `joyBasePitch_[v]` tracks the base MIDI note (fixed for lifetime of gate); `joyLastBendValue_[v]` avoids sending redundant `pitchWheel` messages when bend value is unchanged
- `MidiCallback onMidi` added to `ProcessParams` alongside `NoteCallback onNote` — TriggerSystem emits raw `juce::MidiMessage` objects for pitch bend and CC; `PluginProcessor` wires this directly to `MidiBuffer`
- `processBlockBypassed` sends `pitchWheel(ch, 0)` per voice before note-off to clean up lingering bend state when plugin is bypassed
- TouchPlate pads dim visually and become non-functional in JOY and RND trigger modes
- `sendBendRangeRPN()` helper in TriggerSystem: sends the 6-message RPN sequence (CC101=0, CC100=0, CC6=24, CC38=0, CC101=127, CC100=127) before each gate-open note-on
- Build clean, VST3 installed to `C:\Program Files\Common Files\VST3\ChordJoystick.vst3`

## Commit History

All work committed atomically:

| Commit | Type | Description |
|--------|------|-------------|
| `afc4ba0` | feat | joystickThreshold APVTS param + continuous gate model |
| `0a2e023` | feat | Threshold slider + TouchPlate dimming in PluginEditor |
| `a2cea2e` | fix | JOY legato note tracking (superseded -- replaced by pitch bend) |
| `3ccd1a2` | fix | Close JOY gate after 50ms of stillness below threshold |
| `f6e6d5f` | fix | JOY pitch tracking via quantized pitch bend (final model) |

## Files Created/Modified

- `Source/TriggerSystem.h` — Replaced `joyActivePitch_[4]` with `joyBasePitch_[4]` + `joyLastBendValue_[4]`; added `MidiCallback onMidi` to `ProcessParams`; declared `sendBendRangeRPN()` private helper; kept `joystickStillSamples_[4]`
- `Source/TriggerSystem.cpp` — Gate-open path: `sendBendRangeRPN`, `pitchWheel(ch, 0)`, `fireNoteOn(basePitch)`; while-open path: compute bend offset, convert to ±8191, send `pitchWheel` if changed; gate-close (50ms): `pitchWheel(ch, 0)` then `onNote(joyBasePitch_[v], false)`; `resetAllGates()`: clears `joyBasePitch_` and `joyLastBendValue_`
- `Source/PluginProcessor.cpp` — Added `tp.onMidi` lambda (routes `MidiMessage` directly to `MidiBuffer`); `processBlockBypassed()` now emits `pitchWheel(ch, 0)` per voice before note-off

## Decisions Made

- **Pitch bend as final model (over legato):** Legato note-on/off pairs require the receiving synth to support MIDI legato mode; not universally reliable. Pitch bend is a direct pitch control message understood by all hardware and software synths. Since `heldPitches` is already scale-quantized, bend values automatically align to in-scale degree boundaries — the joystick becomes a continuous but harmonically-locked pitch control.
- **Base pitch stays fixed:** The MIDI note number sent at gate-open never changes. Only `pitchWheel` moves during joystick motion. The synth's note-off tracking (for sustain, envelopes, polyphony) always references `joyBasePitch_[v]`, not the sounding frequency. This ensures note-off always releases the correct voice.
- **Bend range ±24 semitones:** Covers 2 full octaves, sufficient for any quantizer zone jump. The RPN sequence uses `CC6=24` (Data Entry MSB = 24 semitones); bend cents (LSB) are set to 0 for integer-semitone precision.
- **MidiCallback onMidi added to ProcessParams:** Keeps TriggerSystem decoupled from MidiBuffer; callers provide the routing. Consistent with existing NoteCallback onNote pattern.
- **pitchWheel reset before note-off:** If the synth's pitch bend register is non-zero when note-off arrives, some hardware retains that bend offset for the next note-on. Resetting to 0 before releasing ensures a clean state.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Legato note tracking replaced with quantized pitch bend**
- **Found during:** Fix 4 / this continuation (explicit instruction to replace legato with bend model)
- **Issue:** Legato note-on/note-off pairs from commit `a2cea2e` required synth legato mode support; not universally reliable. The pitch change only occurred when ChordEngine crossed a quantizer zone boundary, not smoothly.
- **Fix:** Replaced `joyActivePitch_[4]` with `joyBasePitch_[4]` + `joyLastBendValue_[4]`; gate open sends RPN setup + `pitchWheel(0)` + `noteOn(basePitch)`; while open sends `pitchWheel` for scale-degree offset; gate close resets bend then fires `noteOff(joyBasePitch_[v])`. No legato note-on/note-off pairs during movement.
- **Files modified:** Source/TriggerSystem.h, Source/TriggerSystem.cpp, Source/PluginProcessor.cpp
- **Commit:** f6e6d5f

## Self-Check: PASSED

- `Source/TriggerSystem.h` — present, contains `joyBasePitch_`, `joyLastBendValue_`, `MidiCallback onMidi`, `sendBendRangeRPN`
- `Source/TriggerSystem.cpp` — present, contains pitch bend logic, RPN sequence, 50ms debounce
- `Source/PluginProcessor.cpp` — present, contains `tp.onMidi` lambda, `pitchWheel` reset in `processBlockBypassed`
- Commit `f6e6d5f` — verified in git log

## Verification

Build output confirms clean compile and VST3 install:
- `ChordJoystick_SharedCode.lib` rebuilt
- `ChordJoystick.vst3` rebuilt and installed to `C:\Program Files\Common Files\VST3\`

## Next Steps for DAW Testing

Load `ChordJoystick.vst3` in Reaper with a MIDI monitor:

1. Set ROOT voice to JOY trigger source
2. Move joystick above threshold — observe RPN sequence (CC101/100/6/38) then `pitchWheel(0)` then `noteOn(basePitch)` in MIDI monitor
3. Move joystick to different quantizer zone — observe `pitchWheel` messages with values corresponding to semitone offsets from `basePitch`; NO additional note-on/note-off events
4. Verify pitch snaps to in-scale degrees only (bend values jump discretely as joystick crosses zone boundaries)
5. Release joystick to center — after ~50ms, observe `pitchWheel(0)` then `noteOff(basePitch)` in MIDI monitor

---
*Phase: 04-per-voice-trigger-sources-and-random-gate*
*Plan: 01*
*Completed: 2026-02-22*
