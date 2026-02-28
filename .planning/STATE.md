---
gsd_state_version: 1.0
milestone: v1.5
milestone_name: Routing + Expression
status: unknown
last_updated: "2026-02-28T20:09:53.938Z"
progress:
  total_phases: 10
  completed_phases: 9
  total_plans: 26
  completed_plans: 25
---

---
gsd_state_version: 1.0
milestone: v1.5
milestone_name: Routing + Expression
status: unknown
last_updated: "2026-02-28T19:25:47.688Z"
progress:
  total_phases: 10
  completed_phases: 9
  total_plans: 26
  completed_plans: 26
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-28)

**Core value:** XY joystick mapped to harmonic space — per-note trigger gates, scale quantization, gesture looper with trigger quantization, gamepad control — no competitor provides this as a unified instrument.
**Current focus:** v1.5 — Phase 18: Single-Channel Routing

## Current Position

Phase: 18 of 25 (Single-Channel Routing) — IN PROGRESS
Plan: 2 of 3 in Phase 18 complete; next is Plan 18-03 (Visual Verification)
Status: Phase 18 Plan 02 complete — Routing UI panel built and wired; Plan 03 (visual verify) next
Last activity: 2026-02-28 — Plan 18-02 complete (routingModeBox_, singleChanTargetBox_, voiceChBox_[4] with APVTS attachments; timerCallback visibility toggle)

```
v1.0 MVP    [██████████] SHIPPED 2026-02-23
v1.3 Polish [██████████] SHIPPED 2026-02-25
v1.4 LFO    [██████████] SHIPPED 2026-02-26
v1.5 Routing+Expression  [█         ] In progress
  Phase 17  [██████████]   Bug Fixes              COMPLETE 2026-02-28
  Phase 18  [██████    ]   Single-Channel Routing In progress (2/3 plans)
  Phase 19  [          ]   Sub Octave Per Voice   Not started
  Phase 20  [          ]   RND Trigger Extensions Not started
  Phase 21  [          ]   Left Joystick Targets  Not started
  Phase 22  [          ]   LFO Recording          Not started
  Phase 23  [          ]   Arpeggiator            Not started
  Phase 24  [          ]   Gamepad Option Mode 1  Not started
  Phase 25  [          ]   Distribution           Not started
```

## Performance Metrics

**Velocity:**
- Total plans completed: 36 (v1.0: 17, v1.3: 11, v1.4: 9, v1.5: 5 [Phase 17 complete + 18-01 + 18-02])
- Average duration: not tracked per plan
- Total execution time: not tracked

## Accumulated Context

### Decisions

See .planning/PROJECT.md Key Decisions table — full log with outcomes.

Key v1.5 design decisions (locked, do not re-open):
- LFO recording: pre-distortion samples stored; Distort applied live on playback
- Arp step counter resets to 0 on toggle-off
- Single Channel looper uses live channel setting (not record-time)
- Gate Length is unified across Arp + Random sources (one param, both systems)
- Random Hold mode: RND Sync applies (held pad + sync = gated synced bursts)
- Population range 1–64; Probability 0–100%; subdivision adds 1/64
- Phase 17 must precede Phase 22 (looper anchor bug corrupts LFO recording seams)
- Phase 18 must precede Phase 19 (sentChannel_ infrastructure shared)
- [Phase 17-bug-fixes]: pendingReopenTick_ is a plain bool (not atomic) — timerCallback() runs exclusively on JUCE message thread
- [Phase 17-bug-fixes]: BUG-02: deferred-open pattern + instance-ID guard eliminates PS4 BT double-open and wrong-handle-close crashes
- [Phase 17-01]: BUG-01 fix: loopStartPpq_ += loopLen (not p.ppqPosition - overshoot) — plan formula was wrong; advancing by loopLen is always correct and handles FP drift
- [Phase 17-01]: TC 13 uses ppq = 4.0 - 1e-6 to expose FP drift bug; exact ppq=4.0 would not demonstrate the regression
- [Phase 18-01]: allNotesOff flush paths (DAW stop, gamepad disconnect) now cover all 16 channels — not just voiceChs[v] — to ensure Single Channel mode correctness
- [Phase 18-01]: processBlockBypassed uses sentChannel_ snapshots and calls resetNoteCount() on bypass activation
- [Phase 18-02]: Used full juce::APVTS::ComboBoxAttachment type in header (not ComboAtt alias) — alias declared later in same class, causing MSVC C2923
- [Phase 18-02]: singleChanTargetBox_ and voiceChBox_ grid share same vertical band in resized(); timerCallback setVisible() toggles exclusivity

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-02-28
Stopped at: Completed 18-02-PLAN.md (Routing UI panel — routingModeBox_, singleChanTargetBox_, voiceChBox_[4]; timerCallback visibility toggle)
Next step: Execute Phase 18 Plan 03 (Visual Verification)
