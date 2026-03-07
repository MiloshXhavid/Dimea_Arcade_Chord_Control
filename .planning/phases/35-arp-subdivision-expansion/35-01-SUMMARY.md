# Phase 35 Plan 01 — Summary

**Status:** checkpoint:human-verify — awaiting build + DAW smoke test
**Commit:** 694185f
**Date:** 2026-03-07

## What Was Built

Expanded the arpeggiator subdivision selector from 6 items to 17 items, matching the full
RandomSubdiv ordering used by the Random Trigger system.

## Files Changed

- `Source/PluginProcessor.cpp` — 3 change sites
- `Source/PluginEditor.cpp` — 1 change site

## Exact Changes

### CHANGE SITE 1 — createParameterLayout (line 261)
- `arpSubdiv` APVTS param: 6-item StringArray → 17-item StringArray
- Old: `{ "1/4", "1/8T", "1/8", "1/16T", "1/16", "1/32" }, 2`
- New: 17-item list matching randomSubdivN exactly, default index **6** (= "1/4")
- Decision: default is 1/4 (not 1/8) — matches Random Trigger default

### CHANGE SITE 2 — gamepad dispatch (line 743)
- `stepWrappingParam(apvts, "arpSubdiv", 0, 5, d)` → `stepWrappingParam(apvts, "arpSubdiv", 0, 16, d)`

### CHANGE SITE 3 — processBlock kSubdivBeats (line ~1733)
- `kSubdivBeats[6]` with `jlimit(0, 5)` replaced by `kSubdivBeats[17]` with `jlimit(0, 16)`
- Beat values match `subdivBeatsFor()` in TriggerSystem.cpp exactly
- Default annotated at index 6 (1/4)

### CHANGE SITE 4 — PluginEditor.cpp (line 2876)
- 6 manual `addItem` calls replaced with `addItemList(arpSubdivChoices, 1)` (17 items)
- Tooltip updated: "ARP Rate  -  time between arpeggiator steps" (dropped old "(1/4 to 1/32)")

## Preset Backward Compatibility

Old presets encode `arpSubdiv` as normalised float = `index/5.0` (6-item set).
With 17 items, the same float denormalises to a different index — silently remapped, no crash.
Decision: accepted (pre-release, no users with old presets in the wild). `jlimit(0,16)` prevents
any out-of-bounds access. No migration guard added.

## Verification Checkpoint

Build + install required before verification:
1. Run `build32.ps1` in admin PowerShell — 0 errors required
2. Run `do-reinstall.ps1`
3. Open DAW, load plugin on MIDI track
4. ARP panel → RATE dropdown shows 17 items (4/1 … 4/1T), default = **1/4**
5. Select 1/8T + DAW sync → triplet arp steps in piano roll (2/3 of 1/8 slot)
6. Select 1/1 → noticeably slow (one step per beat)
7. Select 4/1 → very slow (one step per 4 beats)
8. Select 1/64 → very rapid steps
9. Gamepad Option Mode 1, Triangle up/down → cycles all 17 items, wraps at 4/1T → 4/1
10. Load old preset → no crash, arp plays at some rate (any rate = pass)
