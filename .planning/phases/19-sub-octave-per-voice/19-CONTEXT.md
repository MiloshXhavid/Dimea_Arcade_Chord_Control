---
phase: 19-sub-octave-per-voice
created: 2026-03-01
status: ready-for-planning
---

# Phase 19: Sub Octave Per Voice — Context

## Phase Goal
Players can add a parallel bass note exactly one octave below any individual voice,
controllable per-pad from both the UI and the gamepad, enabling bass-doubling without
affecting other voices.

---

## Decision 1: SUB8 Button Layout

**Split the existing 18px HOLD strip 50/50 — HOLD on the left half, SUB8 on the right half.**

```
┌──────────────┬──────────────┐  ← top 18px of each pad (same strip as today)
│     HOLD     │     SUB8     │
├──────────────┴──────────────┤
│                             │
│        ROOT / 3RD / etc     │
│                             │
└─────────────────────────────┘
```

- Label: `"SUB8"` (not SUB, -8, OCT, or 8VB)
- Color when active: **orange/amber** — distinct from green HOLD, unused in current palette
- Color when inactive: same dim style as HOLD inactive
- When HOLD and SUB8 are both active: sub-octave note obeys hold behavior (stays on after
  release; press = note-off, release = note-on) — exactly the same hold-invert logic as the
  main note
- Always visible regardless of trigger source (Pad / Joystick / Random)

**Implementation note:** `padHoldBtn_[v]` is currently full-width. Resize it to left 50%.
Add `padSubOctBtn_[v]` as a new `TextButton` occupying the right 50% of the same strip.
Both are siblings on the PluginEditor (same overlay pattern as HOLD). Layout in `resized()`.

---

## Decision 2: Mid-Note Toggle Behavior

- **Enable SUB8 while gate is open → immediate note-on** for the sub-octave pitch
  (musical: live layering during a held note)
- **Disable SUB8 while sub-octave is sounding → immediate note-off**
- **Sub-octave pitch is locked to trigger-time** — always exactly -12 semitones from the
  main note's snapshot pitch, never from the live joystick position
- **Looper playback** uses the **live SUB8 state at emission time** (same pattern as
  single-channel routing — not baked into the loop)

---

## Decision 3: Gamepad R3 Behavior

- **R3 alone: PANIC REMOVED in this phase.** R3 standalone no longer triggers all-notes-off.
  Panic is UI-button only from Phase 19 onward. (This pre-empts Phase 24's planned removal —
  implement it here, Phase 24 doesn't need to touch R3-alone.)
- **Combo: hold any pad button (L1/L2/R1/R2) + press R3 → toggle SUB8 for that voice**
- **Works in any Option Mode** (Mode 0 and Mode 1)
- **Multiple pads held simultaneously + R3 → toggle SUB8 for all held voices**
- **No special feedback** for the combo — state updates via APVTS param, SUB8 button
  reflects new state on next timerCallback tick (30Hz)

**Implementation note:** In `GamepadInput.cpp`, remove the panic path from
`consumeRightStickTrigger()` / wherever R3-alone fires. In processBlock (or GamepadInput
poll), check `voiceHeld_[v]` for each voice — if any are true when R3 is consumed, toggle
the corresponding `subOctEnabled_[v]` APVTS bool instead.

---

## Decision 4: Visual Feedback

- **SUB8 button lit up (orange/amber) = sufficient.** No changes to the TouchPlate body
  (no stripe, tint, or label change on ROOT/3RD/5TH/TEN).
- **Brightening when sounding:** SUB8 button uses a brighter orange when the sub-octave is
  actively sounding. Condition: `SUB8 enabled AND gate open for that voice`.
  Read via: `*apvts.getRawParameterValue("subOct" + String(v)) > 0.5f && proc_.isGateOpen(v)`
  — **no dedicated `subOctActive_` atomic needed**, same pattern as how HOLD reads
  `padHold_[v]` in `timerCallback`.

---

## APVTS Parameters (new)

4 bool parameters, one per voice:
- `"subOct0"` — Sub Oct voice 0 (Root), default `false`
- `"subOct1"` — Sub Oct voice 1 (Third), default `false`
- `"subOct2"` — Sub Oct voice 2 (Fifth), default `false`
- `"subOct3"` — Sub Oct voice 3 (Tension), default `false`

Pattern: `addBool("subOct0", "Sub Oct Root", false)` — consistent with `arpEnabled`,
`lfoXEnabled` etc. (PluginProcessor.cpp:108–110).

---

## Code Context (for researcher/planner)

| Area | Key File | Key Location | Notes |
|------|----------|--------------|-------|
| HOLD button split | PluginEditor.cpp | resized() ~line 1796 | Currently full-width 18px strip; resize to 50%, add SUB8 at right 50% |
| Hold behavior | PluginProcessor.cpp | ~line 454–478 | `padHold_[v]` atomic drives note-on/off inversion; sub-oct follows same logic |
| Note-on snapshot | PluginProcessor.cpp | ~line 956–958 | `sentChannel_[v]` + `heldPitch_` pattern; sub pitch = `heldPitch_[v] - 12` |
| Note-off matching | PluginProcessor.cpp | ~line 967–969 | Must snapshot sub-pitch at gate-open (like `looperActivePitch_`) |
| Looper gate callback | PluginProcessor.cpp | ~line 848–855 | `looperActivePitch_[v]` snapshot; add `looperActiveSubPitch_[v]` |
| R3 panic removal | GamepadInput.cpp | ~line 209–213 | Remove panic from `rightStickTrig_` consumption path |
| R3 + voice combo | PluginProcessor.cpp | processBlock gamepad poll | Check `voiceHeld_[v]` + `consumeRightStickTrigger()` |
| SUB8 button color | PluginEditor.cpp | timerCallback / paintOverChildren | Orange when enabled; bright orange when enabled + gate open |
| Bool param pattern | PluginProcessor.cpp | line 108–110, 161, 238, 257 | `addBool` lambda + `ButtonAttachment` |

---

## Out of Scope (deferred)

- Sub-octave per voice in arpeggiator steps → Phase 23
- Sub-octave indicator in looper position bar → not requested
- Sub-octave CC/pitch-bend tracking → not requested
