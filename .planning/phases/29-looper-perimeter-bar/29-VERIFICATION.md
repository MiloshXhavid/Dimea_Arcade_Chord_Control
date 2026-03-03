---
phase: 29-looper-perimeter-bar
verified: 2026-03-03T00:00:00Z
status: human_needed
score: 5/5 must-haves verified
human_verification:
  - test: "Visual — No horizontal strip during playback"
    expected: "No 4px horizontal bar renders below the Rec gate buttons at any time"
    why_human: "Pixel-level rendering cannot be verified by static code grep; requires live DAW session"
  - test: "Visual — Clockwise animated bar during loop playback"
    expected: "Green 2px bar travels right→down→left→up around the LOOPER section rectangle, completing exactly one circuit per loop cycle"
    why_human: "Animation path and timing synchrony require live DAW observation; approved by user 2026-03-03"
  - test: "Visual — Ghost ring at idle"
    expected: "Dim full-perimeter outline visible when looper is stopped or no loop loaded"
    why_human: "Rendering appearance cannot be confirmed statically; user approved 2026-03-03"
  - test: "Visual — LOOPER label legibility"
    expected: "The LOOPER label text remains fully readable at all bar positions; no green pixels paint over the characters"
    why_human: "Label exclusion effect is a pixel-level visual check; user approved 2026-03-03"
---

# Phase 29: Looper Perimeter Bar Verification Report

**Phase Goal:** Replace the 4px horizontal looper progress strip with a clockwise rectangular perimeter bar that circuits the outer edge of the LOOPER section box, animated at 30 Hz, with ghost ring when stopped.
**Verified:** 2026-03-03
**Status:** human_needed (automated checks PASSED; human visual approval already on record from Task 2 checkpoint, 2026-03-03)
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | No horizontal progress strip renders below the Rec gate buttons during looper playback or stopped state | VERIFIED | `looperPositionBarBounds_` appears exactly once in PluginEditor.cpp (line 2930: `= {}`). Old paint block (`if (!looperPositionBarBounds_.isEmpty()) { ... }`) is absent. Old `repaint(looperPositionBarBounds_)` in timerCallback is absent. |
| 2 | A 2px green bar travels clockwise around the outer edge of the LOOPER section box, completing exactly one lap per loop cycle at 30 Hz | VERIFIED | Perimeter bar block at lines 3549-3644. `repaint(looperPanelBounds_)` fires in timerCallback at lines 3786-3787. `juce::PathStrokeType stroke(2.0f, ...)` + `Clr::gateOn`. Fraction computed as `beat / len` against `getLooperPlaybackBeat() / getLooperLengthBeats()`. |
| 3 | Bar originates at top-left corner, travels right along top, down right, left along bottom, up left | VERIFIED | `perimPoint` lambda (lines 3561-3572): segment 1 = `b.getX() + dist, b.getY()` (top L→R); segment 2 = `b.getRight(), b.getY() + dist` (right T→B); segment 3 = `b.getRight() - dist, b.getBottom()` (bottom R→L); segment 4 = `b.getX(), b.getBottom() - dist` (left B→T). Distance starts at 0 = top-left. |
| 4 | The LOOPER section knockout label remains fully legible at all times — the bar passes alongside it, never painting over the characters | VERIFIED (code) / ? (visual) | `excludeClipRegion(labelExclude)` called inside `ScopedSaveState` in all three stroke paths: ghost ring (line 3598), tail (line 3624), head (line 3638). Label rectangle computed from drawLfoPanel constants: `12pt bold font, textX = b.getX()+6, textH=14, textY = b.getY()-7`. Human visual approval on record 2026-03-03. |
| 5 | When the looper is stopped or no loop is loaded, a dim ghost ring outlines the full perimeter as a visual affordance | VERIFIED (code) / ? (visual) | `if (!isActive)` branch (lines 3587-3601): 40-segment ghostPath, `closeSubPath()`, `Clr::gateOff.brighter(0.3f)` at 1px `ghostStroke`. `isActive = proc_.looperIsPlaying() \|\| proc_.looperIsRecording()`. Human visual approval on record 2026-03-03. |

**Score:** 5/5 truths verified

---

## Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `Source/PluginEditor.cpp` | Perimeter bar paint block (ghost ring + animated bar) replacing old strip block | VERIFIED | Lines 3549-3644. Contains `perimPoint` lambda, ghost ring branch, animated tail+head branch. |
| `Source/PluginEditor.cpp` | `timerCallback` repaint target updated from `looperPositionBarBounds_` to `looperPanelBounds_` | VERIFIED | Lines 3784-3787: comment "Partial repaint for looper perimeter bar", `if (!looperPanelBounds_.isEmpty()) repaint(looperPanelBounds_)`. No reference to `looperPositionBarBounds_` in timerCallback. |
| `Source/PluginEditor.cpp` | `resized()` `looperPositionBarBounds_` layout call removed, zeroed | VERIFIED | Line 2930: `looperPositionBarBounds_ = {};  // replaced by perimeter bar — no strip layout needed`. Line 2931: `section.removeFromTop(8);` preserves 8px spacing gap. |

**Wiring:**

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `PluginEditor::timerCallback()` | `looperPanelBounds_` | `repaint(looperPanelBounds_)` | WIRED | Line 3786-3787 confirmed |
| `PluginEditor::paint()` perimeter bar block | `proc_.looperIsPlaying() / looperIsRecording()` | `isActive` bool, state branch | WIRED | Line 3585: `const bool isActive = proc_.looperIsPlaying() \|\| proc_.looperIsRecording()` |
| `PluginEditor::paint()` perimeter bar block | `excludeClipRegion` label zone | `ScopedSaveState + excludeClipRegion` in all 3 paths | WIRED | 3 confirmed calls at lines 3598, 3624, 3638 |

---

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| LOOP-01 | 29-01-PLAN.md | Remove existing linear looper progress bar (strip rendered below Rec gates) | SATISFIED | `looperPositionBarBounds_` zeroed in resized(); old strip paint block absent; old timerCallback repaint call absent |
| LOOP-02 | 29-01-PLAN.md | Rectangular perimeter progress bar travels clockwise, one circuit per loop cycle, 30 Hz via timerCallback | SATISFIED | Perimeter bar block lines 3549-3644; `repaint(looperPanelBounds_)` in timerCallback |
| LOOP-03 | 29-01-PLAN.md | Bar origin at top-left, travels right → down right → left along bottom → up left | SATISFIED | `perimPoint` lambda maps 1D distance to 4 CW segments starting at (b.getX(), b.getY()) |
| LOOP-04 | 29-01-PLAN.md | "LOOPER" section label remains fully visible at all times | SATISFIED (code) / Human-approved | `excludeClipRegion(labelExclude)` inside `ScopedSaveState` in all 3 stroke calls; human approved 2026-03-03 |

**Note:** LOOP-01 through LOOP-04 are the only requirement IDs mapped to Phase 29 in REQUIREMENTS.md (lines 276-279). All are marked `[x]` Complete. No orphaned requirements.

---

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| — | — | — | — | None found in new perimeter bar block (lines 3549-3644) |

No TODO/FIXME/HACK/placeholder comments in the new code. No empty return stubs. No console.log-only implementations (C++ codebase, not applicable). Paint ordering is correct: `drawLfoPanel(looperPanelBounds_, "LOOPER")` at line 3396 runs before the perimeter bar block at line 3549, so the panel fill does not overdraw the bar.

---

## Human Verification Required

The following items require visual confirmation in a live DAW session. Per the SUMMARY, Task 2 (human checkpoint) was approved by the user on 2026-03-03.

### 1. No Horizontal Strip Visible

**Test:** Open the plugin in Ableton or Reaper. Look at the LOOPER section during playback and idle.
**Expected:** No 4px horizontal bar below the Rec gate buttons at any time.
**Why human:** Pixel-level rendering requires live plugin UI.

### 2. Clockwise Animated Bar

**Test:** Record a 4-beat loop, start playback, observe the LOOPER section border.
**Expected:** Green 2px bar travels right along the top, down the right, left along the bottom, up the left, completing one full circuit per loop cycle.
**Why human:** Animation path and DAW sync require live observation.

### 3. Ghost Ring at Idle

**Test:** Stop the looper or open the plugin with no loop loaded.
**Expected:** Dim full-perimeter outline (1px, gateOff-tinted) visible around the LOOPER section border.
**Why human:** Visual appearance of dim ring cannot be confirmed statically.

### 4. LOOPER Label Legibility

**Test:** Watch the animated bar cross the top-left corner (label area) at multiple points in the circuit.
**Expected:** "LOOPER" text characters remain fully readable; no green pixels over the label.
**Why human:** Pixel-level exclusion zone alignment requires visual inspection.

**Status of all four:** Approved by user during Task 2 checkpoint, 2026-03-03.

---

## Commit Verification

| Commit | Message | Status |
|--------|---------|--------|
| `e917a47` | `feat(29-01): replace looper strip with clockwise perimeter bar` | FOUND in git log |

---

## Summary

Phase 29 goal is achieved. All five observable truths pass automated code verification. The implementation in `Source/PluginEditor.cpp` matches the plan specification exactly:

- Old 4px horizontal strip removed (looperPositionBarBounds_ zeroed, old paint block gone, old timerCallback repaint removed)
- New `perimPoint` lambda maps 1D perimeter distance to clockwise 2D points around `looperPanelBounds_`
- Ghost ring (1px, dim) renders when `!isActive`
- Animated two-pass bar (40px tail at 0.28 alpha + 12px head at full alpha) renders when playing/recording
- `excludeClipRegion(labelExclude)` inside `ScopedSaveState` protects the LOOPER label in all three stroke calls
- `repaint(looperPanelBounds_)` drives 30 Hz animation from the existing timerCallback
- All four LOOP-0x requirements satisfied and marked complete in REQUIREMENTS.md
- Human visual checkpoint approved 2026-03-03

Status is `human_needed` per scoring rules (all automated checks pass; items flagged for human verification exist and were approved by the user during the Task 2 gate).

---

_Verified: 2026-03-03_
_Verifier: Claude (gsd-verifier)_
