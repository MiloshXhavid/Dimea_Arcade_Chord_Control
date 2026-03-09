---
status: awaiting_human_verify
trigger: "6 UI bugs: gate-length slider axis, invert-mode crosshair lines, tempo knob size, BPM label layout, knob smoothing verification, and starfield direction bug"
created: 2026-03-09T21:00:00Z
updated: 2026-03-09T22:00:00Z
---

## Current Focus

hypothesis: Correction 1 (BPM label below knob) and Bug 7 (INV mode breaks Mod Fix knobs) both fixed. Awaiting user verification.
test: Build succeeded, VST3 installed to local AppData path
expecting: BPM label is below knob; Mod Fix knobs work and track stick in INV mode
next_action: user verifies both fixes

## Symptoms

expected: 6 UI behaviours working correctly + BPM label below knob + Mod Fix works in INV mode
actual: 6 distinct bugs (see below) + BPM label was moved above knob (wrong) + Mod Fix knobs broken in INV mode
errors: none — all runtime visual/UX bugs
reproduction: open plugin UI and interact with each area
started: introduced across recent phases (43.2 starfield co-rotate, phase 45 ARP redesign)

## Eliminated

- hypothesis: starfield normalized-coord drawing was wrong (s.x * b.getWidth())
  evidence: b = getLocalBounds().toFloat() so b.getX()=0,b.getY()=0; coords are correct
  timestamp: 2026-03-09T21:15:00Z

- hypothesis: BPM knob (randomFreeTempoKnob_) lacks velocity smoothing
  evidence: randomFreeTempoKnob_ IS declared as VelocityKnob (PluginEditor.h:500); VelocityKnob has EMA velocity-sensitive drag. Bug 5 = NOT a bug, smoothing already present.
  timestamp: 2026-03-09T21:25:00Z

- hypothesis: Bug 7 caused by bad display atomic or processor INV logic
  evidence: Processor correctly swaps axes in filterXOffsetDisplay_ (blkInv check at line 2247-2256 of PluginProcessor.cpp). Root cause is the editor's INV attachment-swap block not resetting/rewiring filterXOffsetAtt_ and filterYOffsetAtt_, so knob stays attached to wrong APVTS param and timer immediately reverts any user drag.
  timestamp: 2026-03-09T22:00:00Z

## Evidence

- timestamp: 2026-03-09T21:05:00Z
  checked: PluginEditor.h:599, PluginEditor.cpp:3472
  found: arpGateTimeKnob_ declared as VelocityKnob but setSliderStyle(LinearHorizontal) — VelocityKnob::mouseDrag uses getDistanceFromDragStartY() (vertical drag), wrong for horizontal slider
  implication: Bug 1 root cause. Fix = change declaration to VelocitySlider.

- timestamp: 2026-03-09T21:08:00Z
  checked: PluginEditor.cpp:2057-2128 (crosshair draw)
  found: Root/Third labels always on horizontal midpoint; Fifth/Tension always on vertical midpoint. In INV mode axes visually swap but assignments stay the same.
  implication: Bug 2 root cause. Fix = swap label assignments when invOn.

- timestamp: 2026-03-09T21:10:00Z
  checked: PluginEditor.cpp:4350-4377
  found: Probability knob height = 56px effective. Tempo knob = removeFromTop(56).reduced(2,2) = 52px effective. 4px smaller.
  implication: Bug 3 root cause. Fix = removeFromTop(60).reduced(2,2) = 56px to match.

- timestamp: 2026-03-09T21:12:00Z
  checked: PluginEditor.cpp:4349-4377
  found: Density/Probability labels ABOVE knobs (14px). BPM label BELOW knob (12px). Inconsistent.
  implication: Bug 4 root cause was misread as needing label above — user wants label BELOW (original). Correction 1 reverts this.

- timestamp: 2026-03-09T21:15:00Z
  checked: PluginEditor.h:500
  found: randomFreeTempoKnob_ IS VelocityKnob — EMA velocity-sensitive drag is present
  implication: Bug 5 = NOT a bug. No fix needed.

- timestamp: 2026-03-09T21:20:00Z
  checked: PluginEditor.cpp:1429-1437, 1317-1330
  found: bgRotPrev_ = 0.0f in header. First tick snaps bgRotAngle_ to target but NOT bgRotPrev_. Second tick: driftHeading_ += π/2 - 0 = π/2 jump → all stars suddenly bunch to edge.
  implication: Bug 6 root cause. Fix = also set bgRotPrev_ = newTarget * π/180 in bgRotInitialized_ block.

- timestamp: 2026-03-09T22:00:00Z
  checked: PluginEditor.cpp timerCallback INV attachment swap block (~5418), PluginProcessor.cpp filterXOffsetDisplay_ write (~2251)
  found: INV attachment-swap block resets and rewires Mode/Atten knob attachments but NOT the Offset (MOD FIX) knob attachments. In INV mode, filterXOffsetKnob_ (now in Y column) stays attached to filterXOffset param, but the timer writes filterXOffsetDisplay_ (which reflects effXOffset = filterYOffset in INV mode) to it every tick. So any user drag is immediately reverted: the drag changes filterXOffset but the display value comes from filterYOffset, making the knob snap back.
  implication: Bug 7 root cause. Fix = add filterXOffsetAtt_.reset()/filterYOffsetAtt_.reset() and cross-wire in INV block, same pattern as Atten knobs.

## Resolution

root_cause: |
  Bug 1: arpGateTimeKnob_ is VelocityKnob (Y drag) but LinearHorizontal needs X drag.
  Bug 2: crosshair label assignments don't swap in INV mode.
  Bug 3: Tempo knob 52px effective vs Probability knob 56px effective.
  Correction 1: BPM label was incorrectly moved above knob (wrong direction) — must be below.
  Bug 5: NOT a bug — already VelocityKnob with EMA smoothing.
  Bug 6: bgRotPrev_ uninitialized relative to snapped bgRotAngle_ on first tick.
  Bug 7: INV attachment-swap block omits filterXOffsetAtt_/filterYOffsetAtt_ — knob stays
    attached to wrong APVTS param, timer immediately reverts any user drag.

fix: |
  Bug 1: PluginEditor.h:599 — changed VelocityKnob → VelocitySlider for arpGateTimeKnob_
  Bug 2: PluginEditor.cpp ~2087 — added invOn-aware voice index mapping (voiceH0/H1/V0/V1)
  Bug 3: PluginEditor.cpp ~4376 — removeFromTop(72-14) for knob, remaining 14px for label below
  Correction 1: PluginEditor.cpp ~4376 — BPM label moved back below knob (reverted prior "above" change)
  Bug 5: no fix needed
  Bug 6: PluginEditor.cpp ~1437 — added bgRotPrev_ = newTarget * π/180 in bgRotInitialized_ snap block
  Bug 7: PluginEditor.cpp ~5418 — added filterXOffsetAtt_.reset()/filterYOffsetAtt_.reset() in
    the INV attachment-swap block; added cross-wired recreations: INV=filterYOffset→filterXOffsetKnob_,
    filterXOffset→filterYOffsetKnob_; normal=filterXOffset→filterXOffsetKnob_, filterYOffset→filterYOffsetKnob_
verification: build succeeded; VST3 compiled, linked, and installed to local AppData/Programs/Common/VST3
files_changed:
  - Source/PluginEditor.h
  - Source/PluginEditor.cpp
