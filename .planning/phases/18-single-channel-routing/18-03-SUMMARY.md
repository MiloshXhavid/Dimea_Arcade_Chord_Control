---
phase: 18-single-channel-routing
plan: 03
subsystem: deploy+verify
tags: [smoke-test, routing, single-channel, human-verified]

requirements-completed:
  - ROUT-01
  - ROUT-02
  - ROUT-03
  - ROUT-04
  - ROUT-05

# Metrics
duration: <5min
completed: 2026-03-01
---

# Phase 18 Plan 03: Deploy + Smoke Test Summary

**All 8 smoke tests passed. ROUT-01 through ROUT-05 verified. Phase 18 complete.**

## Performance

- **Completed:** 2026-03-01
- **Tasks:** 2 (deploy + human verify)

## Smoke Test Results

| Test | Requirement | Result |
|------|-------------|--------|
| 1 — UI layout | ROUT-01 | ✅ Routing panel visible; Multi/Single toggle works; per-voice dropdowns hide/show correctly |
| 2 — Multi-Channel routing | ROUT-01 | ✅ 4 pads → noteOns on ch 1/2/3/4 only |
| 3 — Single Channel routing | ROUT-01, ROUT-02 | ✅ All 4 voices on selected channel only |
| 4 — Filter CC routing | ROUT-04 | ✅ CC74 arrives on selected channel only in Single mode |
| 5 — Note deduplication | ROUT-03 | ✅ Same pitch from 2 voices → exactly 1 noteOn / 1 noteOff; no stuck notes |
| 6 — Looper live channel | ROUT-05 | ✅ Changing channel mid-playback updates looper emission channel |
| 7 — Mode-switch flush | ROUT-03 | ✅ No stuck notes when switching mode while pad held |
| 8 — Preset save/load | ROUT-01, ROUT-02 | ✅ Routing mode and channel target restored correctly on preset load |

## Additional Changes (session)

- Layout polish applied alongside this phase: right column pushed down 14px, 14px CUTOFF/RES label clearance, X/Y Range knobs moved under LFO panels, Gate% slider anchored to joystick pad bottom, chord display locked to padAll_ Y
- Default routing changed to Single Channel ch1 (`singleChanMode` default index 0→1)
- Committed: `55acc2b` — pushed to `plugin` remote

## Phase 18 Requirements Status

- **ROUT-01** ✅ Routing dropdown and conditional UI
- **ROUT-02** ✅ Single Channel target dropdown (1-16)
- **ROUT-03** ✅ Note deduplication confirmed
- **ROUT-04** ✅ Filter CCs on selected channel in Single mode
- **ROUT-05** ✅ Looper uses live channel at emission time

---
*Phase: 18-single-channel-routing*
*Completed: 2026-03-01*
