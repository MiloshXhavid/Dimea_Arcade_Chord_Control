---
status: resolved
trigger: "Bug A: octave-interval-knob-no-drag / Bug B: rnd-sync-knob-style-mismatch"
created: 2026-03-09T00:00:00Z
updated: 2026-03-09T04:00:00Z
---

## Current Focus

hypothesis: CONFIRMED and FIXED (round 3) — human verification APPROVED
test: Reloaded plugin in DAW, cycled through all 3 RND SYNC states
expecting: RND SYNC color scheme matches the looper box buttons exactly
next_action: COMPLETE — session archived

## Symptoms

expected:
  - Bug A: Dragging octave/interval knobs changes their value
  - Bug B: RND SYNC button shows "FREE" / "INT" / "DAW" state text
actual:
  - Bug A: Mouse drag does nothing; only scroll wheel works
  - Bug B: RND SYNC shows as colored circle with no text
errors: none
reproduction:
  - Bug A: Open plugin UI, click+drag any octave or interval knob
  - Bug B: Look at RND SYNC button
started: After phase 39 (VelocityKnob introduction)

## Eliminated

- hypothesis: Mouse events intercepted by PluginEditor::mouseDown
  evidence: PluginEditor::mouseDown only handles arp step grid areas, returns early otherwise
  timestamp: 2026-03-09

- hypothesis: setInterceptsMouseClicks blocking events
  evidence: No setInterceptsMouseClicks calls anywhere in Source/
  timestamp: 2026-03-09

- hypothesis: drawRotarySlider changes caused drag regression
  evidence: drawRotarySlider is pure paint code, no mouse handling
  timestamp: 2026-03-09

- hypothesis: Phase 40 changes broke octave/interval knobs
  evidence: Phase 40 only modified JoystickPad internals, not octave/interval knob code
  timestamp: 2026-03-09

## Evidence

- timestamp: 2026-03-09
  checked: VelocityKnob::mouseDrag implementation
  found: Uses getValue() to compute next value; calls setValue(getValue()+change, sendNotificationAsync)
  implication: getValue() returns integer-snapped parameter value after async feedback fires

- timestamp: 2026-03-09
  checked: JUCE SliderParameterAttachment source
  found: sliderValueChanged fires asynchronously, feeds integer-snapped value back via slider.setValue(round, sendNotificationSync)
  implication: Between mouseDrag calls, AudioParameterInt feedback snaps slider back to integer

- timestamp: 2026-03-09
  checked: Math of delta accumulation for range=0..11, kBasePx=300
  found: Each pixel gives 0.037 change. Need 13.6px per drag event to cross 0.5 integer threshold. Mouse events fire every 1-5px. Each event's delta < 0.5, snapped back to same integer.
  implication: ROOT CAUSE Bug A — getValue() returns already-snapped integer; net progress = 0 for normal drag on int-range knobs

- timestamp: 2026-03-09
  checked: JUCE Slider::Pimpl::mouseDrag implementation
  found: Standard JUCE drag uses valueWhenLastDragged (private float accumulator), not getValue(). Float accumulates independently of parameter integer snapping.
  implication: VelocityKnob's use of getValue() instead of private accumulator is the design flaw

- timestamp: 2026-03-09
  checked: drawButtonBackground "round" section + drawButtonText
  found: drawButtonBackground draws ellipse only, no text. drawButtonText returns early for "round". Comment claiming "round draws own content in background" was incorrect.
  implication: ROOT CAUSE Bug B — button text set by updateRndSyncButtonAppearance() is never rendered

- timestamp: 2026-03-09 (round 2)
  checked: loopSyncBtn_ (DAW SYNC) vs randomSyncButton_ setup
  found: loopSyncBtn_ has no setName() → uses default rounded-rect style. randomSyncButton_ had setName("round") → circle rendering. Text was added in round 1 fix but user requires button to match DAW SYNC visual style (rounded rect, not circle), and labels need updating: FREE→Poison, INT→Loop Sync, DAW→DAW Sync
  implication: Fix = remove setName("round") entirely + update all three label strings

## Resolution

root_cause:
  Bug A: VelocityKnob::mouseDrag accumulated via getValue() which returns the integer-snapped
  parameter value after each async APVTS feedback cycle. For AudioParameterInt knobs (range 0..11),
  each small per-event drag delta (< 0.5 at normal speed) gets snapped back to the same integer,
  so net progress = 0. JUCE's standard Slider drag avoids this by using a private float accumulator
  (valueWhenLastDragged) that is independent of parameter snapping.

  Bug B: drawButtonBackground for "round" buttons only draws the circle shape. drawButtonText
  explicitly suppresses text for "round". The state text set by updateRndSyncButtonAppearance()
  was never displayed.

fix:
  Bug A: Added dragAccumValue_ double member to VelocityKnob. Seeded in mouseDown from getValue().
  Used dragAccumValue_ + change in mouseDrag (instead of getValue() + change). Changed
  sendNotificationAsync to sendNotificationSync. In Shift+drag path, re-syncs dragAccumValue_
  to getValue() to maintain seamless handoff from JUCE's fine-tune mode.

  Bug B round 1: Added text drawing to drawButtonBackground "round" section (set bold 8.5px font, set Clr::text colour, drawText centred in ellipse). This fixed text visibility but user required button shape to match DAW SYNC (rounded rect, not circle) and correct labels.

  Bug B round 2: Removed randomSyncButton_.setName("round") so button uses default rounded-rect LookAndFeel path (same as loopSyncBtn_). Updated updateRndSyncButtonAppearance() labels: FREE→Poison, INT→Loop Sync, DAW→DAW Sync. Updated tooltip to match.

  Bug B round 3: Applied correct color scheme:
    - Poison (mode 0): Clr::gateOff, setToggleState(false) — dark/inactive
    - Loop Sync (mode 1): Clr::gateOn.darker(0.45f), setToggleState(false) — dim green matching active REC GATES
    - DAW Sync (mode 2): buttonColourId=Clr::accent, setToggleState(true) — drawButtonBackground
      renders accent.brighter(0.3f) fill + Clr::highlight (pink-red 0xFFFF3D6E) ring border,
      exactly matching how loopSyncBtn_ appears when its toggle is ON.

verification:
  Build round 3: Release build succeeded, zero compiler errors (cmake --build).
  Code review: drawButtonBackground (else branch): fill=isOn?accent.brighter(0.3f):backgroundColour;
    ring=isOn?Clr::highlight:accent.brighter(0.5f). Setting toggleState(true) for DAW Sync triggers
    identical rendering to loopSyncBtn_ when synced.
  Code review: Looper REC buttons use gateOn.darker(0.45f) — Loop Sync now matches.
  Post-build copy failed (expected: COPY_PLUGIN_AFTER_BUILD permission issue, run do-reinstall.ps1).

files_changed:
  - Source/PluginEditor.h: VelocityKnob — added dragAccumValue_ member, updated mouseDown/mouseDrag
  - Source/PluginEditor.cpp: drawButtonBackground "round" — added text drawing block (round 1)
  - Source/PluginEditor.cpp: removed randomSyncButton_.setName("round"), updated tooltip and labels (round 2)
