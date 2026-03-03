---
phase: 27-triplet-subdivisions
verified: 2026-03-03T12:00:00Z
status: human_needed
score: 4/5 must-haves verified
re_verification: false
human_verification:
  - test: "Random Free + RND SYNC ON timing at 120 BPM with 1/8T selected"
    expected: "Gate onsets arrive at ~166ms intervals (1/3 beat = 500ms/3), not 250ms (straight 1/8)"
    why_human: "Cannot execute audio processing in static file analysis; timing correctness requires DAW MIDI monitor timestamps"
  - test: "Quantize subdivision 1/8T grid in DAW piano roll"
    expected: "Recorded notes snap to 166ms grid boundaries — visually distinct from straight 1/8 (250ms) grid"
    why_human: "Requires live quantize recording session and piano roll visual inspection"
  - test: "Random subdiv ComboBox shows all 15 options in correct interleaved order"
    expected: "Dropdown reads: 4/1, 2/1, 1/1, 1/1T, 1/2, 1/2T, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T, 1/64"
    why_human: "ComboBox rendering must be confirmed in the running plugin UI"
  - test: "Quantize subdiv ComboBox shows all 10 options in correct interleaved order"
    expected: "Dropdown reads: 1/1T, 1/2T, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T; default shows 1/8 on fresh load"
    why_human: "ComboBox rendering and default selection must be confirmed in the running plugin UI"
  - test: "Triplet subdivision preset save/reload roundtrip"
    expected: "Select 1/8T in random subdiv and quantize subdiv, save preset, reload — both dropdowns show 1/8T"
    why_human: "Requires JUCE APVTS XML serialization exercised by actual plugin save/load cycle"
---

# Phase 27: Triplet Subdivisions Verification Report

**Phase Goal:** Players can select triplet rhythms (1/1T through 1/32T) in both the random trigger and quantize subdivision selectors, enabling triplet-feel gate patterns and quantize snapping without any workarounds
**Verified:** 2026-03-03T12:00:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|---------|
| 1 | Random trigger selector shows 6 triplet options (1/1T–1/32T) alongside existing straight subdivisions | ✓ VERIFIED | `subdivChoices` StringArray in PluginProcessor.cpp has 15 interleaved items; PluginEditor.cpp `randomSubdivBox_` uses identical 15-item StringArray |
| 2 | Quantize selector shows 6 triplet options alongside existing straight subdivisions | ✓ VERIFIED | `qSubdivChoices` in PluginProcessor.cpp has 10 interleaved items (1/1T, 1/2T, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T); `quantizeSubdivBox_` addItem calls match |
| 3 | All subdivision options save/reload correctly with a preset | ? UNCERTAIN | APVTS handles XML persistence automatically for `AudioParameterChoice`; new-preset roundtrip logic is sound, but needs human confirmation in a running session |
| 4 | Triplet timing is mathematically correct (× 2/3 of straight value; exact fractions used) | ✓ VERIFIED | `subdivBeatsFor()` uses `1.0/3.0`, `2.0/3.0`, `1.0/12.0` etc.; `hitsPerBarToProbability()` handles all 15 enum cases; `quantizeSubdivToGridSize()` uses identical fractions for cases 0–9 |
| 5 | Old presets load with quantize showing 1/8 (no regression) | ✗ KNOWN DEVIATION | SUMMARY explicitly records: "No preset backward compatibility maintained — user accepted index shift for old presets." Old saved quantizeSubdiv value 1 now resolves to "1/2T" (not "1/8") under the interleaved ordering. User decision, not a defect. |

**Score:** 3/5 truths fully verified (+ 1 uncertain, + 1 known-accepted deviation)

---

## Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `Source/TriggerSystem.h` | RandomSubdiv enum with 15 interleaved values (QuadWhole=0 … SixtyFourth=14) | ✓ VERIFIED | 15 values confirmed; interleaved: Whole=2, WholeT=3, Half=4, HalfT=5, Quarter=6, QuarterT=7, Eighth=8, EighthT=9, Sixteenth=10, SixteenthT=11, ThirtySecond=12, ThirtySecondT=13, SixtyFourth=14 |
| `Source/TriggerSystem.cpp` | `subdivBeatsFor()` and `hitsPerBarToProbability()` handle all 15 cases | ✓ VERIFIED | `subdivBeatsFor`: 15 named cases (QuadWhole–ThirtySecondT) + default return 1.0. `hitsPerBarToProbability`: 14 named cases + default (SixtyFourth=64.0f). All triplet fractions use exact division (8.0/3.0 etc.) |
| `Source/PluginProcessor.cpp` | `randomSubdiv` APVTS has 15-item interleaved StringArray; `quantizeSubdiv` is `AudioParameterChoice` with 10-item interleaved StringArray | ✓ VERIFIED | randomSubdiv: 15 items, default index 8 = "1/8". quantizeSubdiv: `addChoice()` call with 10-item `qSubdivChoices`, default index 4 = "1/8". |
| `Source/LooperEngine.cpp` | `quantizeSubdivToGridSize()` handles indices 0–9 with exact fractions | ✓ VERIFIED | Cases 0–9: 8.0/3.0, 4.0/3.0, 1.0, 2.0/3.0, 0.5, 1.0/3.0, 0.25, 1.0/6.0, 0.125, 1.0/12.0. Default: 0.5. No decimal approximations. |
| `Source/LooperEngine.h` | `quantizeSubdiv_` comment updated to document 0=1/1T through 9=1/32T | ✓ VERIFIED | Line 197 comment: `0=1/1T, 1=1/2T, 2=1/4, 3=1/4T, 4=1/8, 5=1/8T, 6=1/16, 7=1/16T, 8=1/32, 9=1/32T`. Free function declaration on line 251–252 also updated. |
| `Source/PluginEditor.cpp` | `randomSubdivBox_[4]` uses 15-item interleaved StringArray; `quantizeSubdivBox_` has 10 addItem calls in interleaved order | ✓ VERIFIED | randomSubdivBox_: 15-item StringArray, setSelectedItemIndex(8) = "1/8". quantizeSubdivBox_: IDs 1–10 = 1/1T, 1/2T, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T. Tooltip updated to "(1/4 to 1/32T)". |

---

## Key Link Verification

### Link 1: TriggerSystem.h enum → TriggerSystem.cpp dispatch (direct integer cast)

**Via:** `tp.randomSubdiv[v] = static_cast<RandomSubdiv>(subdivIdx)` in PluginProcessor.cpp

| Check | Result |
|-------|--------|
| APVTS index 8 → enum integer 8 → `RandomSubdiv::Eighth` | ✓ WIRED — enum `Eighth = 8`, subdivBeatsFor returns 0.5 |
| APVTS index 9 → enum integer 9 → `RandomSubdiv::EighthT` | ✓ WIRED — enum `EighthT = 9`, subdivBeatsFor returns 1.0/3.0 |
| All 15 APVTS indices align with corresponding enum integer values | ✓ WIRED — PluginProcessor StringArray and enum both use same 15-entry interleaved ordering |

### Link 2: PluginProcessor.cpp randomSubdiv choices → PluginEditor.cpp randomSubdivBox_

**Via:** Both must have identical length and ordering for UI labels to match parameter values.

| Check | Result |
|-------|--------|
| PluginProcessor subdivChoices count | 15 items (verified by reading lines 182–191) |
| PluginEditor subdivChoices count | 15 items (verified by reading lines 1802–1811) |
| Item ordering identical | ✓ WIRED — "4/1", "2/1", "1/1", "1/1T", "1/2", "1/2T", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64" in both files |

### Link 3: PluginProcessor.cpp quantizeSubdiv choices → LooperEngine.cpp quantizeSubdivToGridSize()

**Via:** `setQuantizeSubdiv(qSubdiv)` stores APVTS choice index; `quantizeSubdivToGridSize(subdivIdx)` must match.

| APVTS Index | qSubdivChoices value | quantizeSubdivToGridSize case | Grid size | Match |
|-------------|---------------------|-------------------------------|-----------|-------|
| 0 | "1/1T" | case 0: 8.0/3.0 | 2.667 beats | ✓ |
| 1 | "1/2T" | case 1: 4.0/3.0 | 1.333 beats | ✓ |
| 2 | "1/4" | case 2: 1.0 | 1.0 beats | ✓ |
| 3 | "1/4T" | case 3: 2.0/3.0 | 0.667 beats | ✓ |
| 4 | "1/8" | case 4: 0.5 | 0.5 beats | ✓ |
| 5 | "1/8T" | case 5: 1.0/3.0 | 0.333 beats | ✓ |
| 6 | "1/16" | case 6: 0.25 | 0.25 beats | ✓ |
| 7 | "1/16T" | case 7: 1.0/6.0 | 0.167 beats | ✓ |
| 8 | "1/32" | case 8: 0.125 | 0.125 beats | ✓ |
| 9 | "1/32T" | case 9: 1.0/12.0 | 0.083 beats | ✓ |

**Status: ✓ WIRED — All 10 indices align perfectly**

### Link 4: PluginProcessor.cpp quantizeSubdiv choices → PluginEditor.cpp quantizeSubdivBox_

**Via:** `ComboBoxParameterAttachment` maps choice index (0-based) to item ID (1-based). Index N → item ID N+1.

| APVTS Index | Choice label | ComboBox item ID | Item label | Match |
|-------------|-------------|-----------------|------------|-------|
| 0 | "1/1T" | 1 | "1/1T" | ✓ |
| 1 | "1/2T" | 2 | "1/2T" | ✓ |
| 2 | "1/4" | 3 | "1/4" | ✓ |
| 3 | "1/4T" | 4 | "1/4T" | ✓ |
| 4 | "1/8" (default) | 5 | "1/8" | ✓ |
| 5 | "1/8T" | 6 | "1/8T" | ✓ |
| 6 | "1/16" | 7 | "1/16" | ✓ |
| 7 | "1/16T" | 8 | "1/16T" | ✓ |
| 8 | "1/32" | 9 | "1/32" | ✓ |
| 9 | "1/32T" | 10 | "1/32T" | ✓ |

**Status: ✓ WIRED — UI labels match APVTS indices at all 10 positions**

---

## Interleaved Ordering Consistency Check

The implementation uses a single interleaved ordering across all 4 layers. The SUMMARY notes this was a user-requested deviation from the PLAN (which specified appended-block ordering). Consistency check:

| Layer | Order | Count | Default index | Status |
|-------|-------|-------|---------------|--------|
| TriggerSystem.h RandomSubdiv enum | Interleaved: QuadWhole, DblWhole, Whole, WholeT, Half, HalfT, Quarter, QuarterT, Eighth, EighthT, Sixteenth, SixteenthT, ThirtySecond, ThirtySecondT, SixtyFourth | 15 | n/a (not an APVTS default) | ✓ |
| PluginProcessor.cpp randomSubdiv StringArray | Interleaved: "4/1","2/1","1/1","1/1T","1/2","1/2T","1/4","1/4T","1/8","1/8T","1/16","1/16T","1/32","1/32T","1/64" | 15 | 8 = "1/8" | ✓ |
| PluginEditor.cpp randomSubdivBox_ StringArray | Identical to processor StringArray | 15 | setSelectedItemIndex(8) = "1/8" | ✓ |
| PluginProcessor.cpp qSubdivChoices | Interleaved: "1/1T","1/2T","1/4","1/4T","1/8","1/8T","1/16","1/16T","1/32","1/32T" | 10 | 4 = "1/8" | ✓ |
| LooperEngine.cpp quantizeSubdivToGridSize() | case 0..9 matching qSubdivChoices | 10 | default fallback: 0.5 | ✓ |
| PluginEditor.cpp quantizeSubdivBox_ addItem | IDs 1–10 matching qSubdivChoices | 10 | n/a (APVTS attachment handles default) | ✓ |
| LooperEngine.h quantizeSubdiv_ comment | Documents 0=1/1T through 9=1/32T | — | — | ✓ |

**All 4 layers (enum, APVTS, LooperEngine dispatch, UI ComboBox) are internally consistent in interleaved ordering.**

---

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|---------|
| TRIP-01 | 27-01-PLAN.md | Random trigger subdivision selector includes 1/1T–1/32T alongside straight subdivisions, save/load with preset | ✓ SATISFIED | 15-item interleaved StringArray in both PluginProcessor and PluginEditor; APVTS `AudioParameterChoice` provides XML persistence |
| TRIP-02 | 27-01-PLAN.md | Quantize subdivision selector includes same 6 triplet options alongside straight subdivisions | ✓ SATISFIED | 10-item interleaved `qSubdivChoices`; `quantizeSubdivToGridSize()` extended to 10 cases; `quantizeSubdivBox_` populated with 10 items |

**Note:** REQUIREMENTS.md still shows TRIP-01 and TRIP-02 as `- [ ]` (unchecked). The implementation satisfies both requirements but the tracking document was not updated. This is a documentation gap, not a code defect.

---

## Deviations from PLAN

The implementation deviates from the PLAN's specified design in two user-accepted ways:

### Deviation 1: Interleaved vs appended-block ordering (PLAN Truth 2 not literally met)

- **PLAN specified:** Triplets appended after straight values (straight 0–8, triplets 9–14 for random; straight 0–3, triplets 4–9 for quantize)
- **RESEARCH explicitly warned against interleaving** (Section "Anti-Patterns to Avoid"): "Do not interleave triplets with straight values in the randomSubdiv enum/choice list... Interleaving breaks the direct cast static_cast<RandomSubdiv>(subdivIdx)"
- **What was implemented:** Interleaved ordering throughout all 4 layers
- **Why it works despite the warning:** The interleaved implementation is self-consistent — enum integer values, APVTS StringArray indices, and UI ComboBox ordering all use the same interleaved sequence. The cast `static_cast<RandomSubdiv>(subdivIdx)` still works correctly because enum integers and APVTS indices are synchronized.
- **User decision:** SUMMARY records this as an explicit user choice made before execution ("user asked for the subdivision lists to be reordered to interleaved tempo order")

### Deviation 2: No preset backward compatibility (PLAN Truth 5 not met)

- **PLAN required:** "Old presets (v1.5-era) load with quantize showing 1/8 (index 1) — no regression"
- **What was implemented:** No backward compatibility. Old quantizeSubdiv value 1 now means "1/2T" (not "1/8") under the interleaved ordering.
- **User decision:** SUMMARY records: "No preset backward compatibility maintained — user accepted index shift for old presets; no migration needed"

---

## Anti-Patterns Found

None found in the 5 modified files. Grep for TODO/FIXME/XXX/HACK/PLACEHOLDER returned no matches in TriggerSystem.cpp, PluginProcessor.cpp, LooperEngine.cpp, or PluginEditor.cpp.

---

## Human Verification Required

### 1. Triplet Timing — Random Free + RND SYNC ON

**Test:** Set voice 0 to Random Free, enable RND SYNC, select "1/8T" subdivision. At 120 BPM, observe gate onsets in DAW MIDI monitor.
**Expected:** Inter-onset intervals are approximately 166ms (not 250ms as straight 1/8 would produce). Formula: 1/3 beat × (60s/120 BPM) = 1/3 × 0.5s = 0.1667s = 166.7ms.
**Why human:** Requires audio processing and DAW MIDI monitoring.

### 2. Quantize Triplet Grid — 1/8T in DAW Piano Roll

**Test:** Set quantize mode to Live or Post, set quantize subdivision to "1/8T". Record a short loop with gate events not precisely on the grid. Observe quantized results in DAW piano roll.
**Expected:** Quantized note positions land on the triplet grid (166ms apart), not the straight 1/8 grid (250ms apart). The two grids are visually distinct.
**Why human:** Requires live recording session and visual inspection of piano roll.

### 3. Random Subdiv ComboBox UI Content and Order

**Test:** Click the random subdivision dropdown for any voice.
**Expected:** 15 items in order: 4/1, 2/1, 1/1, 1/1T, 1/2, 1/2T, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T, 1/64. Default selection is "1/8" on a fresh load.
**Why human:** JUCE ComboBox rendering must be confirmed in the running plugin UI.

### 4. Quantize Subdiv ComboBox UI Content and Order

**Test:** Click the quantize subdivision dropdown.
**Expected:** 10 items in order: 1/1T, 1/2T, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T. Default on fresh load shows "1/8".
**Why human:** JUCE ComboBox rendering must be confirmed in the running plugin UI.

### 5. Triplet Preset Save/Reload Roundtrip

**Test:** Select "1/8T" in a random subdivision ComboBox and in the quantize subdivision ComboBox. Save a preset. Reload the preset.
**Expected:** Both dropdowns show "1/8T" after reload.
**Why human:** JUCE APVTS XML serialization must be exercised in a real save/load cycle.

---

## Summary

All 5 source files have been modified with substantive, internally consistent implementations. The interleaved ordering is consistent across all 4 required layers (TriggerSystem.h enum, PluginProcessor.cpp APVTS choices, LooperEngine.cpp dispatch, PluginEditor.cpp ComboBoxes). Exact fraction arithmetic is used throughout. There are no stubs, orphaned artifacts, or anti-patterns.

Two aspects deviate from the PLAN with user acceptance: (1) interleaved rather than appended-block ordering, and (2) no preset backward compatibility (explicitly accepted by user). The deviations are structurally sound — the interleaved implementation is fully self-consistent.

Five items require human verification in a running DAW session: timing correctness, quantize grid behavior, ComboBox UI content, and preset roundtrip. The automated code analysis verifies that the mathematical and structural prerequisites for all these behaviors are correctly in place.

**REQUIREMENTS.md documentation gap:** TRIP-01 and TRIP-02 remain marked `- [ ]` in REQUIREMENTS.md. Phase 27 in the ROADMAP progress table still shows "In progress" (0/1 plans). These are doc-only gaps that should be updated.

---

_Verified: 2026-03-03T12:00:00Z_
_Verifier: Claude (gsd-verifier)_
