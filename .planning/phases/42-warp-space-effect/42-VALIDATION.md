---
phase: 42
slug: warp-space-effect
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-09
---

# Phase 42 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest (CMake) + ChordJoystickTests (Catch2) |
| **Config file** | `build/CTestTestfile.cmake` |
| **Quick run command** | `cmake --build "C:/Users/Milosh Xhavid/get-shit-done/build" --config Release --target ChordJoystickTests` |
| **Full suite command** | `ctest --test-dir "C:/Users/Milosh Xhavid/get-shit-done/build" -C Release` |
| **Estimated runtime** | ~30 seconds |

---

## Sampling Rate

- **After every task commit:** Run quick build to confirm no compile errors
- **After every plan wave:** Run full suite + manual DAW smoke test
- **Before `/gsd:verify-work`:** Full suite green + visual UAT in DAW
- **Max feedback latency:** 60 seconds (build) + manual visual check

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | Status |
|---------|------|------|-------------|-----------|-------------------|--------|
| 42-01-* | 01 | 1 | warp ramp + warpStars_ data | build-only | quick build | ⬜ pending |
| 42-02-* | 02 | 2 | paint pass + visual effect | manual UAT | DAW load + play looper | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

None — paint-only phase. Existing infrastructure covers all phase requirements. No new test files required.

---

## Manual-Only Verifications

| Behavior | Why Manual | Test Instructions |
|----------|------------|-------------------|
| Warp ramps to full over 4000ms on looper play | JUCE Component — no unit test harness | Load in DAW, press looper record, then play; watch pad over ~4s |
| Stars spawn at vanishing point, accelerate outward | Visual only | Observe streaks emanating from cursor during warp |
| Blue-white center / warm-white edge color gradient | Visual only | Confirm color temperature shift during warp |
| Looper stop → ramp down over 4000ms | Visual only | Press looper stop, watch warp fade |
| Cursor and UI panels remain on top | Layer order | Verify no warp star draws over knobs or chord display |
| warpT = smoothstep(warpRamp_) correct math | Trivial formula | Confirmed by code review of implementation |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 60s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
