---
phase: 45-arpeggiator-step-patterns-ui-redesign-lfo-heart-preset
verified: 2026-03-08T22:30:00Z
status: human_needed
score: 9/9 must-haves verified
human_verification:
  - test: "8-step grid visual — enable ARP and open the plugin UI. Confirm 8 cells appear in 2 rows x 4 columns between the ARP ON button and the controls row."
    expected: "Cells are visible. Row A contains steps 1–4, Row B contains steps 5–8. The vertical separator line at mid-width is present."
    why_human: "Grid rendering depends on resized() and paint() executing with real bounds — cannot confirm layout geometry without a running instance."
  - test: "Click cycling — click any cell 3 times."
    expected: "State cycles ON (bright fill) → TIE (dim fill + horizontal bar centered on cell) → OFF (empty rect with border) → ON."
    why_human: "Requires visual confirmation that all three render conditions are distinguishable and the bar appears for TIE."
  - test: "arpLength dimming — change LEN combo to 4."
    expected: "Cells 5–8 render at ~40% alpha (visibly dimmer) while cells 1–4 remain full brightness. All 8 cells remain clickable."
    why_human: "Alpha blending and click-through behavior require running verification."
  - test: "RND SYNC cycling — click the RND SYNC button 3 times."
    expected: "Cycles FREE (gray, text 'FREE') → INT (accent green, text 'INT') → DAW (cornflower blue, text 'DAW') → FREE."
    why_human: "Color and text rendering of a TextButton require visual confirmation."
  - test: "TIE sustain in playback — set a pattern with step 1 ON and steps 2–3 TIE, enable ARP, hold a chord."
    expected: "The note from step 1 rings continuously through steps 2 and 3 without re-triggering. No gap/stutter at step boundaries."
    why_human: "TIE behavior is audio-domain — requires listening verification with a MIDI instrument."
  - test: "OFF step silence — set step 3 to OFF in an otherwise ON pattern."
    expected: "The note stops immediately when the sequencer reaches step 3. No note sounds during step 3."
    why_human: "Immediate noteOff behavior requires audio playback to verify timing."
  - test: "kArpH = 84px constraint — inspect the arp panel with adjacent panels visible."
    expected: "The arp box does not overlap the looper panel above or any panel below."
    why_human: "Panel overlap is a visual layout check requiring the running UI."
  - test: "Preset save/load round-trip — configure a non-default step pattern (some TIE/OFF cells, LEN=5), save preset, reload."
    expected: "All step states and the LEN value are restored exactly."
    why_human: "Preset serialization correctness requires loading in a real DAW host."
---

# Phase 45: Arpeggiator Step Patterns + UI Redesign — Verification Report

**Phase Goal:** Add an 8-step pattern grid to the arpeggiator (ON/TIE/OFF per step, variable length 1–8), redesign the arp panel to a 2-row control layout to accommodate the grid within the same box size, and polish the RND SYNC button into a 3-state cycling control.
**Verified:** 2026-03-08T22:30:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|---------|
| 1 | 10 new APVTS params registered (arpStepState0..7, arpLength, randomSyncMode) | VERIFIED | `PluginProcessor.cpp` lines 206–221: addChoice calls for all 10 params present and substantive |
| 2 | Old preset containing randomClockSync=1 loads without crash and maps to randomSyncMode INT | VERIFIED | `setStateInformation` lines 2703–2711: migration shim checks `getChildByAttribute("id","randomClockSync")`, removes old param, creates new PARAM child with value 1.0 for wasSync=true |
| 3 | ARP step loop respects arpLength — steps beyond length are skipped (arpStep_ wraps at arpLen) | VERIFIED | `PluginProcessor.cpp` lines 1916–1918: `arpLen = jlimit(1,8, getRawParameterValue("arpLength")+1)`, `patStep = arpStep_ % arpLen`; line 1969: `if (arpStep_ >= arpLen) arpStep_ = 0`; line 1972: `arpStep_ = (arpStep_+1) % arpLen` |
| 4 | TIE state suppresses step-boundary note-off and re-trigger; note rings continuously | VERIFIED | Lines 1923–1924: `isTie = (stepState==1) && (arpActivePitch_>=0)`; lines 1926–1937: noteOff block guarded by `!isTie`; post-UAT fix (commit 312ac04): gate-time noteOff also peeks at upcoming step state (lines 1844–1860), suppresses when nextState==1 |
| 5 | OFF state sends immediate noteOff and skips note-on | VERIFIED | Lines 1981–1993: `if (stepState==2)` — fires noteOff if active note exists, clears arpActivePitch_/Voice_, then `continue` |
| 6 | randomSyncMode 0=FREE / 1=INT / 2=DAW routes correctly in TriggerSystem | VERIFIED | `TriggerSystem.h` line 99: `int randomSyncMode = 0`; `TriggerSystem.cpp`: 7 branch sites updated — lines 95, 112 (`>=1`), 129 (==2 for DAW effectiveBpm), 136, 279, 305 (`==0` for FREE), 345 (`>=1`); no `randomClockSync` remains |
| 7 | 8-step grid rendered in paint() — 2 rows x 4 cols with ON/TIE/OFF visual states | VERIFIED | `PluginEditor.cpp` lines 4490–4550: full implementation — cellRect lambda, 8-cell loop with ON (fillRect gateOn), TIE (dim fill + 2px horizontal bar), OFF (gateOff fill + drawRect border), 40% alpha for inactive cells, active step white highlight, vertical separator |
| 8 | mouseDown() hit-test cycles arpStepState{i} AudioParameterChoice | VERIFIED | Lines 6003–6027: `PluginEditor::mouseDown` hit-tests `arpStepRowABounds_`/`arpStepRowBBounds_`, computes cellIndex, dynamic_cast to `AudioParameterChoice`, cycles `(idx+1)%3`, calls `setValueNotifyingHost` |
| 9 | RND SYNC button cycles FREE/INT/DAW with matching text and color; timerCallback keeps it in sync | VERIFIED | Lines 2484–2494: `onClick` lambda steps `randomSyncMode` via `AudioParameterChoice`, calls `updateRndSyncButtonAppearance()`; lines 6030–6052: helper sets button text and color for all 3 states; line 5987: timerCallback calls helper every tick |

**Score:** 9/9 truths verified

---

## Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `Source/PluginProcessor.cpp` | 10 new APVTS registrations, step-state loop, migration shim, gamepad cycling | VERIFIED | All 4 aspects confirmed: param registrations at lines 206–221, step loop at 1913–1994, migration shim at 2703–2711, gamepad cycling at lines 775–787 |
| `Source/TriggerSystem.h` | `randomClockSync bool` replaced by `randomSyncMode int` in ProcessParams | VERIFIED | Line 99: `int randomSyncMode = 0; // 0=FREE (Poisson), 1=INT (internal BPM grid), 2=DAW (DAW grid)` |
| `Source/TriggerSystem.cpp` | All randomClockSync branch sites updated to use randomSyncMode int | VERIFIED | 7 sites confirmed, no `randomClockSync` references remain |
| `Source/PluginProcessor.h` | `arpCurrentStep_` atomic declared | VERIFIED | Line 175: `std::atomic<int> arpCurrentStep_ { 0 }` |
| `Source/PluginEditor.h` | `arpLengthBox_`, `arpLengthAtt_`, `arpStepRowABounds_`, `arpStepRowBBounds_` declared; `randomSyncButtonAtt_` removed; `mouseDown()` and `updateRndSyncButtonAppearance()` declared | VERIFIED | Lines 460–463 (new members), line 300 (`updateRndSyncButtonAppearance`), no `randomSyncButtonAtt_` found in file |
| `Source/PluginEditor.cpp` | resized() arp block rewritten, paint() step grid added, mouseDown() hit-test, timerCallback RND SYNC update | VERIFIED | resized() at 4028–4039, paint() at 4490–4550, mouseDown() at 6003–6027, timerCallback at 5985–5998 |

---

## Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `PluginProcessor::createParameterLayout` | APVTS | `addChoice` for arpStepState0..7, arpLength, randomSyncMode | WIRED | Lines 206–221 confirmed |
| `PluginProcessor::setStateInformation` | XML shim | `getChildByAttribute("id","randomClockSync")` | WIRED | Lines 2703–2711 confirmed |
| `PluginProcessor::processBlock` arp loop | `arpStepState` / `arpLength` params | `getRawParameterValue` inside stepsToFire loop | WIRED | Lines 1916–1920 confirmed |
| `TriggerSystem::processBlock` | `ProcessParams.randomSyncMode` | `p.randomSyncMode` comparisons at 7 branch sites | WIRED | All 7 sites confirmed in TriggerSystem.cpp |
| `PluginEditor::resized()` arp block | `arpStepRowABounds_` / `arpStepRowBBounds_` | `removeFromTop(14) × 2` stored to member rects | WIRED | Lines 4031–4032 confirmed |
| `PluginEditor::paint()` | `arpStepState0..7` APVTS params | `getRawParameterValue` per cell in 8-cell loop | WIRED | Lines 4508–4509 confirmed |
| `PluginEditor::mouseDown()` | `arpStepState{i}` AudioParameterChoice | `dynamic_cast + setValueNotifyingHost + repaint` | WIRED | Lines 6020–6026 confirmed |
| `randomSyncButton_.onClick` | `randomSyncMode` AudioParameterChoice | `(getIndex()+1)%3 + setValueNotifyingHost` | WIRED | Lines 2484–2491 confirmed |
| `timerCallback` | `arpCurrentStep_` atomic + RND SYNC appearance | `lastStep` guard + `updateRndSyncButtonAppearance()` | WIRED | Lines 5985–5997 confirmed |

---

## Requirements Coverage

| Requirement | Source Plan | Description (from v1.7-REQUIREMENTS.md) | Status | Evidence |
|-------------|-------------|----------------------------------------|--------|---------|
| ARP-01 | 45-01, 45-02 | Arpeggiator steps through the current 4-voice chord at configurable Rate and Order | SATISFIED | Pre-existing; phase 45 extends the step loop (does not break existing Rate/Order logic) — switch cases at lines 1945–1965 preserved verbatim per SUMMARY deviation notes |
| ARP-02 | 45-01, 45-02 | Arp Rate options: 1/4, 1/8, 1/16, 1/32 | SATISFIED | `arpSubdivBox_` and `arpSubdivAtt_` unchanged; Rate combo retained as first column in new 4-column controls row (line 4035) |
| ARP-03 | 45-01, 45-02 | Arp Order options: Up, Down, Up-Down, Random | SATISFIED | `arpOrderBox_` and `arpOrderAtt_` unchanged; Order combo retained as second column in new 4-column controls row (line 4036) |

**Note on requirement IDs:** ARP-01, ARP-02, ARP-03 were originally satisfied in Phase 23. Phase 45 claims the same IDs as it extends and modifies the arp subsystem. The ROADMAP assigns these IDs to Phase 45, and both plans list all three IDs in `requirements`. The v1.7-REQUIREMENTS.md marks them complete from Phase 23. The phase 45 work extends ARP-01 (adds 8-step ON/TIE/OFF gating within the existing step loop) and adds new arp length behavior. No dedicated "ARP-04/ARP-05/ARP-06" IDs exist for the new step-pattern behavior — the phase uses the existing ARP IDs as anchors for the arp subsystem. This is consistent with how the ROADMAP associates them.

---

## Anti-Patterns Found

No blockers or stubs detected.

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `PluginEditor.cpp` | 4323 area (TIE bar comment) | Minor comment artifact referencing a logic simplification: `// Simpler: just draw the bar across the full cell width` — comment is accurate, not a TODO | Info | None — code is complete |

---

## Human Verification Required

### 1. Step Grid Visual Layout

**Test:** Install the VST3 (`do-reinstall.ps1` or manual copy), load in DAW, enable ARP. Inspect the arp panel.
**Expected:** 8 cells in 2 rows x 4 columns appear between the ARP ON button and the Rate/Order/LEN/Gate controls row. A faint vertical separator line appears at mid-width. The kArpH=84px box does not overlap adjacent panels.
**Why human:** Grid rendering and pixel layout require a running UI instance.

### 2. Cell Click Cycling and State Visuals

**Test:** Click any cell 3 times, observing each state.
**Expected:** ON = bright filled square (accent green), TIE = dim fill with a 2px horizontal bar centered vertically across the cell, OFF = empty/dark rect with a visible 1px border. States cycle ON → TIE → OFF → ON without delay.
**Why human:** Visual distinguishability of the three states requires human observation.

### 3. arpLength Grid Dimming

**Test:** Change the LEN combo from 8 to 4. Observe cells 5–8.
**Expected:** Cells 5–8 render at ~40% alpha (visibly dimmer) while cells 1–4 remain full brightness. Clicking cell 7 still cycles its state (clickable despite being inactive). When LEN is set back to 8, cells 5–8 restore their saved state.
**Why human:** Alpha rendering and click-through on inactive-range cells require visual and interaction confirmation.

### 4. RND SYNC 3-State Button

**Test:** Click the RND SYNC button 3 times.
**Expected:** FREE (gray background, text "FREE") → INT (accent green background, text "INT") → DAW (cornflower blue background, text "DAW") → FREE.
**Why human:** Button color and text are set via `setColour`/`setButtonText` — visual verification required.

### 5. TIE Sustain Behavior

**Test:** Set a pattern with step 1 ON, steps 2 and 3 TIE, step 4 ON. Enable ARP, hold a chord, listen.
**Expected:** Step 1 fires a note-on. The note sustains through steps 2 and 3 without retriggering (no audible gap or click). Step 4 fires a new note-on (may be a different voice depending on Order setting).
**Why human:** Audio-domain behavior — requires MIDI monitoring or listening with an instrument plugin.

### 6. OFF Step Silence

**Test:** Set step 3 to OFF in an otherwise ON pattern. Enable ARP, hold a chord.
**Expected:** The note from step 2 stops immediately when the sequencer reaches step 3. No note sounds during step 3. Step 4 resumes.
**Why human:** Timing of immediate noteOff requires audio playback verification.

### 7. Preset Round-Trip

**Test:** Configure cells: step 1=ON, step 2=TIE, step 3=OFF, step 4=ON (rest=ON), LEN=5, RND SYNC=INT. Save as preset. Reload the preset.
**Expected:** All step cell states, LEN value, and RND SYNC mode are restored exactly. No crash.
**Why human:** Preset serialization correctness requires loading in a real DAW host (APVTS XML round-trip).

---

## Gaps Summary

No gaps. All processor-side and editor-side artifacts exist, are substantive, and are wired end-to-end. Post-UAT fixes (arpStep wrap at arpLen rather than seqLen; gate-time noteOff peek at upcoming step for TIE) were applied before the final commits and are confirmed in the source.

Automated checks pass for all 9 must-have truths across both plans. Phase 45 goal is fully implemented at the code level. Human UAT (approved per SUMMARY) was performed; the items above are the standard final UAT checks that cannot be confirmed by static analysis.

---

_Verified: 2026-03-08T22:30:00Z_
_Verifier: Claude (gsd-verifier)_
