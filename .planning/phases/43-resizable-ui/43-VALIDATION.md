---
phase: 43
slug: resizable-ui
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-09
---

# Phase 43 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest + Catch2 (FetchContent, project-standard) |
| **Config file** | `build/CMakeCache.txt` (BUILD_TESTS=ON required) |
| **Quick run command** | `cmake --build "C:/Users/Milosh Xhavid/get-shit-done/build" --config Release --target ChordJoystickTests && ctest --test-dir "C:/Users/Milosh Xhavid/get-shit-done/build" -C Release -R ChordJoystickTests` |
| **Full suite command** | same (single test binary) |
| **Estimated runtime** | ~30 seconds |

---

## Sampling Rate

- **After every task commit:** Build VST3, install, open in DAW, resize to 0.75x and 2.0x, verify layout
- **After every plan wave:** Full build + DAW open/resize/save/load cycle
- **Before `/gsd:verify-work`:** Full suite must be green + DAW UAT complete
- **Max feedback latency:** ~60 seconds (build + install + DAW open)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 43-01-01 | 01 | 1 | Persistence round-trip | unit | `ctest -R ChordJoystickTests` | ❌ Wave 0 | ⬜ pending |
| 43-01-02 | 01 | 1 | scaleFactor_ clamp 0.75..2.0 | unit | `ctest -R ChordJoystickTests` | ❌ Wave 0 | ⬜ pending |
| 43-01-03 | 01 | 1 | Constructor init before setSize | manual | Open plugin, verify no first-frame jump | manual-only | ⬜ pending |
| 43-02-01 | 02 | 2 | Aspect ratio locked on drag | manual | DAW resize, verify 4:3 maintained | manual-only | ⬜ pending |
| 43-02-02 | 02 | 2 | Controls clickable at 0.75x | manual | Open in DAW at 0.75x, click all controls | manual-only | ⬜ pending |
| 43-02-03 | 02 | 2 | No overflow at 2.0x | manual | Open in DAW at 2240×1680, verify layout | manual-only | ⬜ pending |
| 43-02-04 | 02 | 2 | No MIDI on resize | manual | MIDI monitor open, resize window freely | manual-only | ⬜ pending |
| 43-02-05 | 02 | 2 | Scale persists across save/load | manual | Save project, reload, verify scale restored | manual-only | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `Tests/ScaleFactorTests.cpp` — unit tests for uiScaleFactor XML round-trip and clamp (0.75..2.0). Optional but recommended for persistence logic coverage.
- [ ] Framework already installed via FetchContent — no new install step.

*Note: Core correctness criteria (layout correctness, clickability, no MIDI) are visual/interactive and require manual DAW testing. Unit tests cover the persistence and clamping logic only.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Aspect ratio locked during drag | Success Criterion 1 | No programmatic way to drive OS resize | Open in DAW, drag corner, verify width:height stays ~4:3 |
| Controls clickable at 0.75x | Success Criterion 2 | Pixel-level hit testing requires visual + interaction check | Open at 840×630, click each touchplate, knob, combo, verify response |
| No overflow at 2.0x | Success Criterion 2 | Layout correctness is visual | Open at 2240×1680, verify no clipped text or overlapping controls |
| No MIDI output on resize | Success Criterion 3 | Requires live MIDI monitor during resize | Open plugin with MIDI monitor, drag window, verify zero MIDI events |
| Scale persists across save/load | Success Criterion 4 | Requires full DAW save/reload cycle | Resize to non-1x, save project, close DAW, reopen, verify same size |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 60s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
