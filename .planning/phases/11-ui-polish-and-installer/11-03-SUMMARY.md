---
phase: 11-ui-polish-and-installer
plan: "03"
subsystem: PluginEditor / PluginProcessor / LooperEngine
tags: [ui, looper, progress-bar, repaint, partial-repaint]
dependency_graph:
  requires: ["11-01", "11-02"]
  provides: ["looper-position-bar"]
  affects: ["Source/PluginProcessor.h", "Source/PluginEditor.cpp"]
tech_stack:
  added: []
  patterns: ["partial repaint region", "atomic float read from message thread", "inline accessor delegation"]
key_files:
  created: []
  modified:
    - Source/PluginProcessor.h
    - Source/PluginEditor.cpp
decisions:
  - "Used getPlaybackBeat() and getLoopLengthBeats() — exact names confirmed in LooperEngine.h lines 145-146"
  - "Partial repaint via repaint(looperPositionBarBounds_) at top of timerCallback — avoids full editor repaint at 30 Hz"
  - "Division-by-zero guarded: fraction computed only when len > 0.0"
  - "No juce::ProgressBar used — plain paint() draw to avoid internal timer"
metrics:
  duration: "~10 minutes"
  completed: "2026-02-25"
  tasks_completed: 2
  files_modified: 2
---

# Phase 11 Plan 03: Looper Position Bar Summary

Looper playback position bar: 10px horizontal strip inside the LOOPER panel that sweeps left-to-right at 30 Hz showing current beat position relative to loop length, using atomic float read from message thread via two new thin PluginProcessor accessors.

## Tasks Completed

| Task | Description | Commit |
|------|-------------|--------|
| 1 | Add getLooperPlaybackBeat() + getLooperLengthBeats() to PluginProcessor | 344e803 |
| 2 | resized() bar allocation + paint() drawing + timerCallback() partial repaint | 91b6bdd |

## Files Modified

### Source/PluginProcessor.h
- Lines 71-72: Two new inline public accessor methods added after `looperIsCapReached()` (line 68)
  ```cpp
  double getLooperPlaybackBeat()  const { return looper_.getPlaybackBeat();    }
  double getLooperLengthBeats()   const { return looper_.getLoopLengthBeats(); }
  ```
- API names confirmed from LooperEngine.h: `getPlaybackBeat()` (line 145) and `getLoopLengthBeats()` (line 146)

### Source/PluginEditor.cpp
- **resized() — line 1669:** `looperPositionBarBounds_` set as 10px strip, followed by 4px gap, between mode buttons row and subdiv/length ctrlRow
  ```cpp
  looperPositionBarBounds_ = section.removeFromTop(10);
  section.removeFromTop(4);  // 4px gap after bar before subdiv row
  ```
- **paint() — lines 1834-1856:** Bar drawn after `drawSectionPanel(...)` calls. Background `Clr::gateOff`, foreground `Clr::gateOn` proportional fill, guarded by `len > 0.0` to prevent division-by-zero
- **timerCallback() — line 1968:** `repaint(looperPositionBarBounds_)` at the very top of function body (before `padRoot_.repaint()`)

## Bar Layout in resized()

Position within the looper section (counting from top of looper panel):
1. Buttons row 1 (PLAY/REC/RST/DEL): 36px
2. Gap: 2px
3. Buttons row 2 (REC GATES/REC JOY/REC WAIT/DAW SYNC): 28px
4. Gap: 4px
5. **Position bar: 10px** ← looperPositionBarBounds_
6. Gap after bar: 4px
7. Subdiv + length ctrlRow: 46px
8. Gap: 4px
9. Quantize row: 20px

## Verification Results

- `grep -n "getLooperPlaybackBeat\|getLooperLengthBeats" Source/PluginProcessor.h` — 2 results (lines 71, 72)
- `grep -n "looperPositionBarBounds_" Source/PluginEditor.cpp` — 4 results (lines 1669, 1834, 1836, 1967-1968)
- `grep -n "repaint(looperPositionBarBounds_)" Source/PluginEditor.cpp` — exactly 1 result (line 1968)
- `grep -n "juce::ProgressBar" Source/PluginEditor.cpp` — 0 results
- Build: CLEAN — VST3 compiled and installed to `C:\Program Files\Common Files\VST3\Dima_Chord_Joy_MK2.vst3`

## Build Status

PASS — zero compile errors. VST3 output:
`build/ChordJoystick_artefacts/Debug/VST3/Dima_Chord_Joy_MK2.vst3/Contents/x86_64-win/Dima_Chord_Joy_MK2.vst3`

## Deviations from Plan

None — plan executed exactly as written.

## Self-Check: PASSED
- Source/PluginProcessor.h — modified (getLooperPlaybackBeat/getLooperLengthBeats present at lines 71-72)
- Source/PluginEditor.cpp — modified (looperPositionBarBounds_ set in resized, drawn in paint, repaint in timerCallback)
- Commits 344e803 and 91b6bdd both exist
