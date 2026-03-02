---
phase: 26-defaults-and-bug-fix
verified: 2026-03-03T00:00:00Z
status: human_needed
score: 4/5 must-haves verified
human_verification:
  - test: "Open fresh plugin instance (no saved state) in DAW and check all four octave knobs and Scale Preset dropdown"
    expected: "Root Octave=2, Third Octave=4, Fifth Octave=4, Tension Octave=3, Scale Preset=Natural Minor"
    why_human: "APVTS defaults set in createParameterLayout() are only observable at plugin instantiation in a live DAW — cannot be verified from code grep alone"
  - test: "Load a v1.5 preset saved before this phase and confirm knob values reflect saved state, not new defaults"
    expected: "fifthOctave and scalePreset knobs show the values stored in the preset file, not 4 / Natural Minor"
    why_human: "Preset regression requires loading actual preset state through APVTS setStateInformation() in a live host"
  - test: "Enable Single Channel mode, manoeuvre joystick so two voices produce the same MIDI pitch, press both pads, release both"
    expected: "MIDI monitor shows one note-on (deduplicated), one note-off on full release — no active notes remaining afterward"
    why_human: "Real-time MIDI output and stuck-note behaviour require a live DAW + MIDI monitor; cannot be inferred from static code inspection"
---

# Phase 26: Defaults and Bug Fix — Verification Report

**Phase Goal:** The plugin loads with musically sensible default octave values and Natural Minor scale on a fresh install, and the noteCount_ reference-counting clamp is removed so that single-channel mode no longer produces stuck notes when two voices land on the same pitch.
**Verified:** 2026-03-03
**Status:** human_needed — all automated checks PASSED; three items require DAW confirmation
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|---------|
| 1 | Fresh plugin shows Fifth Octave knob = 4 | ? HUMAN | `addInt(ParamID::fifthOctave, "Fifth Octave", 0, 12, 4)` at line 144 — correct in code; live DAW instantiation needed to confirm display |
| 2 | Fresh plugin shows Scale Preset = Natural Minor | ? HUMAN | `addChoice(ParamID::scalePreset, "Scale Preset", scaleNames, 1)` at line 160; `ScalePreset::NaturalMinor = 1` confirmed in ScaleQuantizer.h; live DAW needed to confirm dropdown label |
| 3 | Third Octave=4 and Tension Octave=3 already correct (DEF-01/DEF-03) | ✓ VERIFIED | Line 143: `addInt(ParamID::thirdOctave, ..., 0, 12, 4)`; line 145: `addInt(ParamID::tensionOctave, ..., 0, 12, 3)` — both unchanged and correct |
| 4 | No else-noteCount_ clamp branches remain anywhere in processBlock | ✓ VERIFIED | `grep "else\s*noteCount_"` returns zero matches across entire PluginProcessor.cpp |
| 5 | Existing v1.5 presets load without regression | ? HUMAN | APVTS architecture confirmed correct (defaults only apply at createParameterLayout time; setStateInformation restores saved values); live preset load needed to confirm no regression |

**Score:** 2/5 truths fully verified programmatically; 3/5 require human DAW testing (all automated preconditions satisfied)

---

## Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `Source/PluginProcessor.cpp` | APVTS defaults corrected + all else-clamps removed | ✓ VERIFIED | File exists and is substantive; contains both required patterns; zero else-clamp branches remain |

### Artifact — Three-Level Check

**Level 1 — Exists:** `Source/PluginProcessor.cpp` exists and is the sole modified file for this phase.

**Level 2 — Substantive:** File contains actual implementation — 1555+ lines of real C++ code, no stubs, no TODOs/FIXMEs found.

**Level 3 — Wired:**
- `createParameterLayout()` values are consumed by `buildChordParams()` at lines 427-433 via `apvts.getRawParameterValue()` — wired correctly.
- `noteCount_` is used throughout processBlock with correct guard-only pattern (no else branches) — wired correctly.
- The legitimate `std::fill` allNotesOff reset at lines 1062-1063 (channel-switch flush) was intentionally preserved — confirmed intact.

---

## Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `createParameterLayout()` fifthOctave | APVTS initial value = 4 | Fourth argument to `addInt()` | ✓ WIRED | Line 144: `addInt(ParamID::fifthOctave, "Fifth Octave", 0, 12, 4)` — exact pattern match |
| `createParameterLayout()` scalePreset | APVTS initial value = 1 (NaturalMinor) | Fourth argument to `addChoice()` | ✓ WIRED | Line 160: `addChoice(ParamID::scalePreset, "Scale Preset", scaleNames, 1)` — exact pattern match |
| `processBlock()` note-off paths | MIDI noteOff emission | `noteCount_[ch-1][pitch] > 0 && --noteCount_[ch-1][pitch] == 0` guard only, no else | ✓ WIRED | 13 decremented note-off sites all follow guard-only pattern; zero `else noteCount_` occurrences found |

---

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|---------|
| DEF-01 | 26-01-PLAN.md | Third octave knob displays "4" by default on fresh install | ✓ SATISFIED | Line 143: `addInt(ParamID::thirdOctave, ..., 0, 12, 4)` — already correct, unchanged |
| DEF-02 | 26-01-PLAN.md | Fifth octave knob displays "4" by default on fresh install | ✓ SATISFIED (code) / ? HUMAN (display) | Line 144: `addInt(ParamID::fifthOctave, ..., 0, 12, 4)` — changed from 3 to 4; commit `ffad2b8` |
| DEF-03 | 26-01-PLAN.md | Tension octave knob displays "3" by default on fresh install | ✓ SATISFIED | Line 145: `addInt(ParamID::tensionOctave, ..., 0, 12, 3)` — already correct, unchanged |
| DEF-04 | 26-01-PLAN.md | Scale preset defaults to Natural Minor on fresh install | ✓ SATISFIED (code) / ? HUMAN (display) | Line 160: `addChoice(..., scaleNames, 1)`; NaturalMinor=1 confirmed in ScaleQuantizer.h enum; commit `ffad2b8` |
| BUG-03 | 26-01-PLAN.md | Remove else noteCount_ clamp from all note-off paths — eliminates stuck notes in Single Channel mode | ✓ SATISFIED (code) / ? HUMAN (runtime) | Zero `else noteCount_` occurrences in PluginProcessor.cpp; all 13 targeted branches removed; commit `2bdff1a` |

All five requirement IDs declared in PLAN frontmatter are accounted for. REQUIREMENTS.md marks all five as `[x]` Complete with Phase 26 assignment. No orphaned requirements detected.

---

## Commit Verification

Both commits claimed in SUMMARY.md are present in the git log and their messages match the tasks:

| Commit | Message | Status |
|--------|---------|--------|
| `ffad2b8` | `feat(26-01): fix APVTS defaults — fifthOctave 3→4, scalePreset Major→NaturalMinor` | ✓ EXISTS |
| `2bdff1a` | `fix(26-01): remove all 13 else-noteCount_=0 clamp branches (BUG-03)` | ✓ EXISTS |

---

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| — | — | None found | — | — |

No TODOs, FIXMEs, placeholders, empty implementations, or stubs detected in `Source/PluginProcessor.cpp`.

---

## Human Verification Required

### 1. Fresh-Instance Default Values

**Test:** Open DAW (Ableton or Reaper). Insert the freshly built VST3 from `build/ChordJoystick_artefacts/Release/VST3/`. Do NOT load any preset — let the plugin initialize with defaults.
**Expected:** Root Octave=2, Third Octave=4, Fifth Octave=4, Tension Octave=3, Scale Preset dropdown shows "Natural Minor".
**Why human:** APVTS defaults are set at createParameterLayout() time and are only visible when the plugin first instantiates in a live host. Code grep confirms the correct values are set; the displayed label and knob position require a live DAW.

### 2. Preset Regression Check

**Test:** Load any v1.5 preset file saved before this session. Inspect the Fifth Octave knob and Scale Preset dropdown.
**Expected:** Both show the values stored in the preset (e.g., Fifth Octave=3, Scale=Major if that was what was saved), not the new defaults (4 / Natural Minor). The new defaults must not overwrite explicit saved values.
**Why human:** Requires loading a real preset through APVTS setStateInformation() in a live host to confirm saved values take precedence over defaults.

### 3. Single Channel Stuck-Note Regression (BUG-03)

**Test:** Enable Single Channel mode (Routing dropdown). Adjust joystick position or intervals so that two voices produce the same MIDI pitch. Press both corresponding pads simultaneously; observe MIDI monitor for note-on events. Release both pads; observe MIDI monitor for note-off events. Then press a different chord and confirm all prior notes are off.
**Expected:** MIDI monitor shows exactly one note-on (deduplicated by noteCount_ increment guard); on release of both pads, exactly one note-off; no stuck notes on subsequent chord presses.
**Why human:** Real-time MIDI output and stuck-note detection require a live DAW MIDI monitor. The code change (zero else-clamp branches) is verified, but the runtime correction can only be confirmed by observing live MIDI output.

---

## Gaps Summary

No gaps found. All automated checks pass:

- `addInt(ParamID::fifthOctave, ..., 0, 12, 4)` — present at line 144.
- `addInt(ParamID::thirdOctave, ..., 0, 12, 4)` — present at line 143 (DEF-01 already satisfied).
- `addInt(ParamID::tensionOctave, ..., 0, 12, 3)` — present at line 145 (DEF-03 already satisfied).
- `addChoice(ParamID::scalePreset, "Scale Preset", scaleNames, 1)` — present at line 160.
- `ScalePreset::NaturalMinor = 1` — confirmed in ScaleQuantizer.h (Major=0, NaturalMinor implicitly 1).
- Zero `else noteCount_` occurrences remaining in PluginProcessor.cpp.
- The `std::fill` channel-switch flush at lines 1062-1063 is intact (not a clamp — correct behavior).
- Both commits (`ffad2b8`, `2bdff1a`) exist in git history with correct messages.
- REQUIREMENTS.md marks all five requirement IDs as `[x]` Complete under Phase 26.
- No anti-patterns (TODO/FIXME/placeholder/stubs) detected.

The three human-verification items are all runtime-observable behaviors that the code correctly supports. They are checkpoints, not gaps.

---

_Verified: 2026-03-03_
_Verifier: Claude (gsd-verifier)_
