---
phase: 39-knob-ux-velocity-drag
verified: 2026-03-09T12:00:00Z
status: human_needed
score: 7/7 automated must-haves verified
re_verification: false
human_verification:
  - test: "Drag a VelocityKnob slowly then quickly; confirm slow drag feels precise (300 px full sweep at 1x) and fast drag sweeps ~3x faster"
    expected: "SC-1: velocity-sensitive drag — slow drag precise, fast drag ~3x multiplier, Shift bypasses to fine-tune"
    why_human: "EMA multiplier ramp and feel cannot be verified by static code inspection; requires runtime interaction"
  - test: "Drag the LFO rate fader and a rotary knob (e.g. Probability) quickly vs slowly; observe value display"
    expected: "SC-2: EMA smoothing — no stepping or sudden speed jumps; acceleration/deceleration feels smooth"
    why_human: "EMA smoothness is a subjective feel criterion; static analysis cannot confirm absence of visible stepping"
  - test: "Move mouse over any rotary knob; observe ring; move mouse away"
    expected: "SC-3: faint pink-red hover ring appears on hover, disappears on mouse-out"
    why_human: "isMouseOver() triggers repaint at runtime; hover ring presence requires live plugin rendering"
  - test: "Open plugin; inspect Transpose, Third/Fifth/Tension Interval, Root/Third/Fifth/Tension Octave knobs"
    expected: "SC-4: 12 evenly-spaced dots around each arc, bright dot on current value, no fill arc; dot updates when value changes"
    why_human: "Visual rendering of 12 dots vs fill arc requires plugin rendered in DAW — static grep only shows code path, not paint output"
---

# Phase 39: Knob UX / Velocity Drag Verification Report

**Phase Goal:** Add velocity-sensitive drag to all in-scope knobs and sliders, with EMA smoothing, hover ring, and 12-dot snap indicators for 0..11 range knobs.
**Verified:** 2026-03-09T12:00:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| #  | Truth                                                                     | Status     | Evidence                                                              |
|----|---------------------------------------------------------------------------|------------|-----------------------------------------------------------------------|
| 1  | VelocityKnob class exists with full EMA velocity drag logic               | VERIFIED  | PluginEditor.h lines 255-302: mouseDown/mouseDrag/mouseUp, kAlpha=0.25, kBasePx=300, mult jlimit(1,3) |
| 2  | VelocitySlider class exists with same EMA logic for horizontal sliders    | VERIFIED  | PluginEditor.h lines 307-354: mirrors VelocityKnob, uses getDistanceFromDragStartX |
| 3  | ScaleSnapSlider inherits VelocityKnob (not juce::Slider)                  | VERIFIED  | PluginEditor.h line 376: `class ScaleSnapSlider : public VelocityKnob` |
| 4  | drawRotarySlider: 12-dot mode for 0..11 range knobs, fill arc suppressed  | VERIFIED  | PluginEditor.cpp lines 76-133: isDotMode check, 12-dot loop in else branch, fill arc wrapped in `if (!isDotMode)` |
| 5  | drawRotarySlider: hover ring drawn last at 1.5px, 35% alpha               | VERIFIED  | PluginEditor.cpp lines 177-183: after centre cap, `slider.isMouseOver()`, hoverR=trackR+2.5f, alpha=0.35f, stroke=1.5f |
| 6  | All in-scope rotary members changed to VelocityKnob (15 members)          | VERIFIED  | PluginEditor.h lines 411-561: joyXAttenKnob_/joyYAttenKnob_, transposeKnob_, rootOctKnob_/thirdOctKnob_/fifthOctKnob_/tensionOctKnob_, randomDensityKnob_, randomProbabilityKnob_, randomFreeTempoKnob_, filterXAttenKnob_/filterYAttenKnob_, filterXOffsetKnob_/filterYOffsetKnob_, arpGateTimeKnob_ |
| 7  | All 10 in-scope linear LFO sliders changed to VelocitySlider              | VERIFIED  | PluginEditor.h lines 582-587: lfoXSisterAttenSlider_/lfoYSisterAttenSlider_, lfoXRateSlider_/lfoYRateSlider_, lfoXPhaseSlider_/lfoYPhaseSlider_, lfoXLevelSlider_/lfoYLevelSlider_, lfoXDistSlider_/lfoYDistSlider_ |

**Score:** 7/7 truths verified

### Required Artifacts

| Artifact                | Expected                                          | Status    | Details                                                                       |
|-------------------------|---------------------------------------------------|-----------|-------------------------------------------------------------------------------|
| `Source/PluginEditor.h` | VelocityKnob, VelocitySlider classes + type changes | VERIFIED | Classes defined at lines 250-354; ScaleSnapSlider rebased at line 376; all member type changes present |
| `Source/PluginEditor.cpp` | drawRotarySlider: isDotMode + 12 dots + hover ring | VERIFIED | isDotMode check lines 76-81; dot loop lines 122-132; hover ring lines 177-183 |

### Key Link Verification

| From                       | To                          | Via                                              | Status   | Details                                                                                     |
|----------------------------|-----------------------------|--------------------------------------------------|----------|---------------------------------------------------------------------------------------------|
| VelocityKnob               | juce::Slider                | public inheritance, mouseDown/Drag/Up override   | WIRED   | Overrides call juce::Slider base; EMA state in private members                              |
| VelocitySlider             | juce::Slider                | public inheritance, mouseDown/Drag/Up override   | WIRED   | Same pattern; uses getDistanceFromDragStartX (horizontal axis)                              |
| ScaleSnapSlider            | VelocityKnob                | public inheritance                               | WIRED   | Line 376; inherits all EMA drag without any additional changes needed                       |
| All rotary members         | VelocityKnob type           | member declaration                               | WIRED   | 15 direct VelocityKnob members confirmed; 3 ScaleSnapSlider members also inherit VelocityKnob |
| All LFO linear members     | VelocitySlider type          | member declaration                               | WIRED   | 10 VelocitySlider members confirmed                                                         |
| isDotMode                  | slider.getMinimum/Maximum/Interval | runtime query in drawRotarySlider          | WIRED   | Detection at lines 79-81; APVTS attachments set range/interval in PluginEditor ctor         |
| hover ring                 | slider.isMouseOver()         | runtime query in drawRotarySlider                | WIRED   | Line 178; JUCE triggers repaint on mouse-enter/exit automatically                           |

### Requirements Coverage

No requirement IDs were declared for this phase (requirements-completed: [] in both plan summaries). Phase 39 is a UX enhancement with no formal requirements mapping.

### Anti-Patterns Found

| File                    | Line | Pattern | Severity | Impact |
|-------------------------|------|---------|----------|--------|
| Source/PluginEditor.h   | 466  | `juce::Slider thresholdSlider_` | Info | Intentionally out of scope per 39-01 plan; not listed in Task 4 |
| Source/PluginEditor.h   | 537  | `juce::Slider loopLengthKnob_`  | Info | Intentionally out of scope per 39-01 plan; not listed in Task 4 |

No TODO/FIXME/placeholder comments found in modified files. No empty implementations found.

### Commit Verification

Both implementation commits are verified present in git history:

| Commit    | Message                                                     | Files changed                |
|-----------|-------------------------------------------------------------|------------------------------|
| `09f0ac8` | feat(39-01): VelocityKnob + VelocitySlider classes, ScaleSnapSlider rebased, member type changes | Source/PluginEditor.h (+121/-15) |
| `792a521` | feat(39-01): hover ring + 12-dot snap indicators in drawRotarySlider | Source/PluginEditor.cpp (+58/-26) |
| `afb1cb3` | docs(39-01): complete VelocityKnob plan — SUMMARY.md, STATE.md, ROADMAP.md updated | docs only |
| `790a244` | docs(39-02): complete Phase 39 knob UX — UAT approved, SUMMARY STATE ROADMAP updated | docs only |

### Human Verification Required

All four runtime success criteria require plugin execution in a DAW. The 39-02 SUMMARY records human tester approval of all four on 2026-03-09, but the verification agent cannot confirm runtime behavior programmatically.

#### 1. SC-1: Velocity-Sensitive Drag

**Test:** Drag any in-scope rotary knob (e.g. transposeKnob_, randomProbabilityKnob_) very slowly — confirm roughly 300 px for a full sweep. Drag same knob quickly in one stroke — confirm approximately 3x faster sweep. Change direction mid-drag — confirm no jump. Hold Shift — confirm fine-tune mode (slower than normal slow drag).
**Expected:** EMA-ramp multiplier 1x-3x, Shift=bypass to JUCE built-in fine-tune
**Why human:** Runtime drag behavior; static analysis confirms EMA formula but not feel or multiplier scaling in practice

#### 2. SC-2: EMA Smoothing

**Test:** Drag lfoXRateSlider_ (linear) slowly then quickly. Observe value display on randomProbabilityKnob_ and randomFreeTempoKnob_ during fast drag.
**Expected:** No discrete steps or sudden speed jumps; acceleration/deceleration smoothly ramped
**Why human:** EMA smoothness is subjective; cannot verify absence of visual stepping without rendering

#### 3. SC-3: Hover Highlight Ring

**Test:** Move mouse over any rotary knob; move mouse away.
**Expected:** Faint pink-red ring appears on hover (trackR+2.5f, 35% alpha), disappears on mouse-out
**Why human:** Requires live paint call with isMouseOver()=true; cannot confirm visual output from source alone

#### 4. SC-4: 12-Dot Indicators

**Test:** Look at Transpose, Third/Fifth/Tension Interval, Root/Third/Fifth/Tension Octave knobs (all have range 0..11, interval=1). Change a value.
**Expected:** 12 evenly-spaced dots on arc, no fill arc; active dot is highlight colour, inactive dots are dim accent; dot tracks value change
**Why human:** Requires plugin rendering in DAW to confirm visual output

### Gaps Summary

No automated gaps found. All 7 must-haves pass at levels 1, 2, and 3 (exists, substantive, wired). The two remaining `juce::Slider` members (`thresholdSlider_`, `loopLengthKnob_`) are confirmed out-of-scope by their absence from the 39-01 plan's task list.

The phase depends on 4 runtime UX criteria that the 39-02 SUMMARY records as human-approved. These items are flagged as `human_needed` rather than `gaps_found` because the code implements the correct logic paths — only runtime confirmation is outstanding.

---

_Verified: 2026-03-09T12:00:00Z_
_Verifier: Claude (gsd-verifier)_
