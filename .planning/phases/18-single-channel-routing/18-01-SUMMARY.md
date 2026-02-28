---
phase: 18-single-channel-routing
plan: "01"
subsystem: PluginProcessor / MIDI routing
tags: [single-channel, midi-routing, note-dedup, apvts, processor]
dependency_graph:
  requires: []
  provides: [singleChanMode APVTS param, singleChanTarget APVTS param, effectiveChannel lambda, noteCount_ dedup, sentChannel_ snapshots, looperActiveCh_ snapshots, arpActiveCh_ snapshot, resetNoteCount helper]
  affects: [PluginProcessor.cpp, PluginProcessor.h]
tech_stack:
  added: []
  patterns: [noteCount reference counting for MIDI note dedup, channel snapshot pattern for note-off correctness, flush-before-mute guard ordering]
key_files:
  created: []
  modified:
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp
decisions:
  - "DAW stop flush now sends allNotesOff on all 16 channels (not just voiceChs[v]) to handle Single Channel mode correctness"
  - "pendingAllNotesOff_ (gamepad disconnect) now sends all 16 channels and calls resetNoteCount()"
  - "looper stop/reset use looperActiveCh_[v] snapshot for note-offs (not current voiceChs[v])"
  - "processBlockBypassed uses sentChannel_ snapshots and resets noteCount_"
metrics:
  duration: "~4 minutes"
  completed: "2026-02-28T21:00:00Z"
  tasks_completed: 2
  files_changed: 2
---

# Phase 18 Plan 01: Single-Channel Routing Infrastructure Summary

Single-Channel Routing processor infrastructure: singleChanMode/singleChanTarget APVTS params, effectiveChannel() lambda used at all 7 MIDI emission sites, noteCount_[16][128] reference-count deduplication for same-pitch collisions, channel snapshots (sentChannel_, looperActiveCh_, arpActiveCh_) for stuck-note-free channel changes, and resetNoteCount() called at all allNotesOff flush paths.

## Tasks Completed

| # | Task | Commit | Files |
|---|------|--------|-------|
| 1 | Add new member declarations to PluginProcessor.h | 7b388d8 | Source/PluginProcessor.h |
| 2 | Add APVTS params, effectiveChannel logic, all emission sites | 7b388d8 | Source/PluginProcessor.cpp |

## What Was Built

### New Members in PluginProcessor.h (private section)
- `noteCount_[16][128]` — reference counter per (channel, pitch) pair; 0-initialized
- `sentChannel_[4]` — channel snapshot per voice at note-on; used at note-off
- `looperActiveCh_[4]` — channel snapshot per voice at looper gate-on; used at looper gate-off
- `arpActiveCh_` — channel snapshot at arp note-on; used at all arp note-offs
- `prevSingleChanMode_` / `prevSingleChanTarget_` — previous-block values for flush detection
- `void resetNoteCount() noexcept` — declaration

### New APVTS Parameters
- `singleChanMode` (Choice: "Multi-Channel" / "Single Channel", default 0)
- `singleChanTarget` (Int 1..16, default 1)

### New Infrastructure in PluginProcessor.cpp
- `resetNoteCount()` implementation — `std::fill` over 16*128 entries
- `effectiveChannel(v)` lambda — returns singleTarget in Single mode, voiceChs[v] in Multi
- `effectiveFilterChannel()` lambda — returns singleTarget in Single mode, filterMidiCh in Multi
- Mode/target change flush block before midiMuted_ guard:
  - Mode change: allNotesOff all 16 channels, resetAllGates, looperActivePitch_.fill(-1), resetNoteCount()
  - Target change (Single mode only): allNotesOff old target channel, clear that channel's noteCount_ row

### Emission Sites Updated (all 7)
1. **TriggerSystem note-on** — effectiveChannel(v), sentChannel_ snapshot, noteCount gate
2. **TriggerSystem note-off** — sentChannel_[v] snapshot read, noteCount gate
3. **Looper gateOn** — effectiveChannel(v), looperActiveCh_[v] + sentChannel_[v] snapshots, noteCount gate
4. **Looper gateOff** — looperActiveCh_[v] snapshot read, noteCount gate
5. **Arp note-on** — effectiveChannel(arpActiveVoice_), arpActiveCh_ + sentChannel_ snapshots, noteCount gate
6. **Arp note-off** (3 paths: kill-on-off, gate-time, step-boundary) — arpActiveCh_ read, noteCount gate
7. **Filter CC** — effectiveFilterChannel() replaces inline filterMidiCh read

### allNotesOff Flush Sites (all call resetNoteCount())
- DAW stop detection — now sends all 16 channels (was voiceChs[v] only)
- Looper stop detection — uses looperActiveCh_[v] for note-offs, then resetNoteCount()
- Looper reset block — uses looperActiveCh_[v] for note-offs, then resetNoteCount()
- Panic block — already sent all 16, now adds resetNoteCount()
- pendingAllNotesOff_ (gamepad disconnect) — now all 16 channels + resetNoteCount() (was voiceChs[v] only)
- Mode switch flush — all 16 channels + resetNoteCount()
- Target change flush — old target channel + clears that row in noteCount_

### processBlockBypassed
- Uses sentChannel_[v] snapshot for note-offs (was voiceChs[v])
- Calls resetNoteCount() after resetAllGates()

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] DAW stop flush was per-voice channels only**
- **Found during:** Task 2 Step F
- **Issue:** Original code sent allNotesOff only on voiceChs[v] (4 channels). In Single Channel mode this would leave notes stuck on the single target channel.
- **Fix:** Changed to loop over all 16 channels — matches the pattern already used in the panic block.
- **Files modified:** Source/PluginProcessor.cpp
- **Commit:** 7b388d8

**2. [Rule 1 - Bug] pendingAllNotesOff_ (gamepad disconnect) was per-voice channels only**
- **Found during:** Task 2 Step F
- **Issue:** Same issue as DAW stop — only sent allNotesOff on voiceChs[v]. In Single Channel mode would leave notes stuck.
- **Fix:** Changed to loop over all 16 channels + resetNoteCount().
- **Files modified:** Source/PluginProcessor.cpp
- **Commit:** 7b388d8

**3. [Rule 2 - Missing] processBlockBypassed missing resetNoteCount()**
- **Found during:** Task 2 Step O
- **Issue:** Plan Step O described updating channel to use sentChannel_, but processBlockBypassed also needs to reset noteCount_ on bypass (bypass sends note-offs that bypass the normal tracking).
- **Fix:** Added resetNoteCount() after resetAllGates() in processBlockBypassed.
- **Files modified:** Source/PluginProcessor.cpp
- **Commit:** 7b388d8

## Build Result

Release build: PASS — 0 errors, 0 warnings from plugin code. VST3 installed to `C:\Program Files\Common Files\VST3\`.

## Self-Check

### Files Exist
- Source/PluginProcessor.h: contains all 6 member declarations and resetNoteCount() declaration
- Source/PluginProcessor.cpp: contains effectiveChannel lambda, effectiveFilterChannel lambda, resetNoteCount() implementation, flush block before midiMuted_ guard, all 7 emission sites updated

### Commits Exist
- 7b388d8: feat(18-01): add Single-Channel Routing infrastructure to processor

## Self-Check: PASSED
