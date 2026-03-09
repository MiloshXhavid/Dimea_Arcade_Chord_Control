---
plan: 24-02
phase: 24-gamepad-option-mode-1
status: complete
completed: "2026-03-01"
---

# Plan 24-02 Summary: PluginProcessor Mode 1 Dispatch + PluginEditor Label/Highlight

## What Was Built

Wired the four new GamepadInput consume methods into PluginProcessor.cpp's Mode 1 dispatch block, gated the looper face-button consume calls to `optMode != 1`, changed the Mode 1 UI label from "OCTAVE" to "ARP", and added per-mode control highlighting in PluginEditor.cpp's `timerCallback()`.

## Tasks Completed

### Task 1: Add Mode 1 face-button dispatch and gate looper consume in PluginProcessor.cpp

**Change 1 — Move optMode declaration and gate looper consume:**
- Moved `const int optMode = gamepad_.getOptionMode()` to before the looper consume calls (line 505)
- Wrapped the four looper consume calls in `if (optMode != 1) { ... }` so that face buttons do not fire looper commands while Mode 1 is active
- Mode 0 and Mode 2 retain full looper face-button control

**Change 2 — Mode 1 face-button arp dispatch (inside `if (optMode == 1)` block after the D-pad octave lines):**
- `consumeArpCircle()` → toggles `arpEnabled` param; if turning ON with looper stopped (non-DAW-sync): calls `looper_.reset()` then `looper_.startStop()`
- `consumeArpRateDelta()` → `stepWrappingParam(apvts, "arpSubdiv", 0, 5, d)`
- `consumeArpOrderDelta()` → `stepWrappingParam(apvts, "arpOrder", 0, 6, d)`
- `consumeRndSyncToggle()` → toggles `randomClockSync` bool param

### Task 2: Change Mode 1 UI label to "ARP" and add per-mode control highlight in PluginEditor.cpp

**Change 1 — Label string:** `"OCTAVE"` → `"ARP"` at timerCallback OPTION indicator update site (line 2893)

**Change 2 — Per-mode control highlight:**
- Added `int lastHighlightMode_ { -1 };` to PluginEditor.h private section
- Added highlight block in timerCallback inside the OPTION indicator update scope
- Change-check guard (`if (optMode != lastHighlightMode_)`) prevents redundant 30Hz repaints
- Mode 1: green tint (`Clr::gateOn` at 18% alpha) on `arpSubdivLabel_`, `arpOrderLabel_`, `rootOctLabel_`, `thirdOctLabel_`, `fifthOctLabel_`, `tensionOctLabel_`
- Mode 2: muted blue tint on `transposeLabel_`, `thirdIntLabel_`, `fifthIntLabel_`, `tensionIntLabel_`
- Mode 0: transparent (no tint) on all controls

## Key Files

- `Source/PluginProcessor.cpp` — Mode 1 arp dispatch + looper gate
- `Source/PluginEditor.cpp` — "ARP" label + per-mode highlight
- `Source/PluginEditor.h` — `lastHighlightMode_` member

## Verification

```
grep consumeArpCircle|consumeArpRateDelta|consumeArpOrderDelta|consumeRndSyncToggle → 4 lines ✓
grep "optMode != 1" → 1 line (looper consume gate) ✓
grep '"ARP"' PluginEditor.cpp → 1 line at label site ✓
grep "lastHighlightMode_" PluginEditor.cpp → 2 lines (guard + assignment) ✓
Build: 0 errors ✓
```

OPT1-05/06/07 regression:
- R3 + pad sub-oct toggle logic is unchanged (lines 515-533)
- Mode 2 D-pad INTRVL block unchanged
- Looper consume gated only in Mode 1 (Mode 0 and Mode 2 fully intact)

## Commits

- `feat(24-02): wire Mode 1 arp dispatch in PluginProcessor + UI label/highlight` (d699669)

## Self-Check: PASSED

- [x] consumeArpCircle/consumeArpRateDelta/consumeArpOrderDelta/consumeRndSyncToggle in optMode==1 block
- [x] Looper consume guarded by optMode != 1
- [x] arpEnabled toggle includes looper reset+start for stopped-looper case
- [x] "ARP" label at timerCallback OPTION indicator
- [x] lastHighlightMode_ declared in PluginEditor.h private
- [x] Per-mode highlight with change-check guard
- [x] Build succeeds with 0 errors
- [x] R3 and Mode 2 behavior unchanged
