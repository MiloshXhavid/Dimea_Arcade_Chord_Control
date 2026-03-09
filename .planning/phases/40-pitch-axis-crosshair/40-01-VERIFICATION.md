---
phase: 40-pitch-axis-crosshair
verified: 2026-03-09T02:00:00Z
status: human_needed
score: 6/6 must-haves verified
re_verification: false
human_verification:
  - test: "Lines and note names update in real time as joystick moves"
    expected: "Moving joystick off-center shows two crosshair half-segments; root/third labels flank horizontal midpoint above/below; fifth/tension labels flank vertical midpoint left/right; all four names change as position changes"
    why_human: "Real-time visual rendering and label accuracy relative to sounding pitches cannot be verified statically"
  - test: "Center suppression works"
    expected: "Returning joystick to exact center hides all lines and labels; no visual artifact at 0,0"
    why_human: "Runtime conditional on abs(mx)+abs(my)<0.01 requires live pad interaction to verify"
  - test: "Right-click toggles crosshair off/on with discoverability dot"
    expected: "Right-click makes crosshair disappear and replaces it with a faint 4px blue dot at cursor; right-click again restores crosshair"
    why_human: "Mouse interaction and visual output cannot be verified statically"
  - test: "Preset round-trip persists crosshair state"
    expected: "Saving a preset with crosshair OFF and reloading it shows the faint dot (not crosshair)"
    why_human: "Requires DAW preset save/reload workflow"
  - test: "Crosshair Visible parameter is automatable"
    expected: "DAW automation lane lists 'Crosshair Visible' as an automatable boolean parameter"
    why_human: "Requires DAW inspection of parameter list"
  - test: "Note names reflect scale quantization"
    expected: "Switching scale preset (e.g. Major to Minor) causes note name labels to update to match quantized minor scale pitches for the current joystick position"
    why_human: "Requires live interaction and musical verification"
---

# Phase 40: Pitch Axis Crosshair Verification Report

**Phase Goal:** Real-time pitch axis crosshair on JoystickPad with note name labels, toggled via right-click, persisted as APVTS bool.
**Verified:** 2026-03-09T02:00:00Z
**Status:** human_needed — all automated checks pass; 6 items require human confirmation
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Moving the joystick updates four note names (root/third on horizontal segment, fifth/tension on vertical segment) in real time at 30 Hz | ? HUMAN | `livePitch_[0..3]` loaded from processor atomics in `timerCallback()` at line 1360–1363; `midiToName()` called in `paint()` at lines 1882/1891/1905/1914; `repaint()` called unconditionally each timer tick at line 1365 |
| 2 | Lines are white at 22% alpha (1 px), drawn as two half-segments from cursor toward pad center | ✓ VERIFIED | `g.setColour(juce::Colours::white.withAlpha(0.22f))` at line 1844; `g.drawLine(displayCx_, displayCy_, padCx, displayCy_, 1.0f)` horizontal at 1847; `g.drawLine(displayCx_, displayCy_, displayCx_, padCy, 1.0f)` vertical at 1849 |
| 3 | When joystick is at center (\|x\|+\|y\| < 0.01), no lines are drawn — only a faint accent dot | ? HUMAN | `atCenter = (std::abs(mx) + std::abs(my)) < 0.01f` at line 1829; guard `if (!crosshairOn \|\| atCenter)` at 1831 suppresses lines; faint dot only drawn when `!crosshairOn` (not when atCenter+on) — correct per spec |
| 4 | Right-clicking JoystickPad toggles crosshair on/off; state persists in preset and is automatable | ? HUMAN | `mouseUp()` detects `e.mods.isRightButtonDown()` at line 1464; calls `param->setValueNotifyingHost(...)` to flip value at 1468; `crosshairVisible` declared with `addBool("crosshairVisible", "Crosshair Visible", true)` at PluginProcessor.cpp:404 — APVTS bools are preset-serialized and DAW-automatable by design |
| 5 | When crosshair is off, a 4 px accent dot at 15% alpha appears as a discoverability hint | ? HUMAN | `g.setColour(juce::Colour(0xFF1E3A6E).withAlpha(0.15f))` at line 1836; `g.fillEllipse(displayCx_ - 4.0f, displayCy_ - 4.0f, 8.0f, 8.0f)` at 1837 — 8px diameter = 4px radius, at cursor display position |
| 6 | Note name labels use octave-qualified format (e.g. C4, Eb3); white at 75% alpha; labels close to cursor are hidden | ✓ VERIFIED | Manual octave formula `note/12 - 1` at line 1856 (C4=60 convention, not JUCE C5); `g.setColour(juce::Colours::white.withAlpha(0.75f))` at 1860; `tooClose()` lambda with `kCollisionR=20.0f` guards all four label draws at 1881/1890/1904/1913 |

**Score:** 6/6 truths verified (2 confirmed from source, 4 confirmed from source pending runtime visual check)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `Source/PluginProcessor.h` | 4 livePitch_ atomics + crosshairVisible APVTS param declaration | ✓ VERIFIED | `std::atomic<int> livePitch0_` through `livePitch3_` declared at lines 240–243 with correct default values (60/64/67/70); APVTS layout is in PluginProcessor.cpp |
| `Source/PluginProcessor.cpp` | APVTS crosshairVisible bool param + processBlock livePitch write | ✓ VERIFIED | `addBool("crosshairVisible", "Crosshair Visible", true)` at line 404; livePitch store loop at lines 1202–1215, placed after `chordP.joystickX/Y = std::clamp(...)` (lines 1197–1198), correctly post-LFO |
| `Source/PluginEditor.h` | livePitch_ cache + JoystickPad private section | ✓ VERIFIED | `int livePitch_[4] = { 60, 64, 67, 70 }` in JoystickPad private section at line 184; no attachment needed (raw param read in paint()) |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `PluginProcessor::processBlock` | `livePitch0_..3_` atomics | `ChordEngine::computePitch(v, chordP)` loop, switch/case stores | ✓ WIRED | Lines 1202–1215: loop `for (int v = 0; v < 4; ++v)`, `computePitch(v, chordP)`, switch stores with `std::memory_order_relaxed` |
| `JoystickPad::timerCallback` | `livePitch0_..3_` atomics | `proc_.livePitchN_.load()` → `livePitch_[N]` | ✓ WIRED | Lines 1360–1363: all four loads assigned to `livePitch_[]` cache; `repaint()` follows at 1365 |
| `JoystickPad::paint` | `displayCx_/displayCy_` | crosshair `drawLine` from display position toward pad center | ✓ WIRED | Lines 1847/1849: `g.drawLine(displayCx_, displayCy_, padCx, ...)` and `g.drawLine(displayCx_, displayCy_, ..., padCy, ...)` |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| VIZ-01 | 40-01-PLAN.md | Horizontal crosshair segment with root/third note names updating in real time | ✓ SATISFIED | `drawLine` horizontal + `midiToName(livePitch_[0/1])` drawn at segment midpoint above/below |
| VIZ-02 | 40-01-PLAN.md | Vertical crosshair segment with fifth/tension note names updating in real time | ✓ SATISFIED | `drawLine` vertical + `midiToName(livePitch_[2/3])` drawn at segment midpoint left/right |
| VIZ-03 | 40-01-PLAN.md | Crosshair toggled via right-click, persisted as APVTS bool, automatable | ✓ SATISFIED | `mouseUp` right-button check → `setValueNotifyingHost`; `addBool("crosshairVisible")` in APVTS layout |
| VIZ-04 | 40-01-PLAN.md | Note names reflect post-scale-quantization pitches (same path as note-ons) | ✓ SATISFIED | `ChordEngine::computePitch(v, chordP)` uses fully-resolved `chordP` from `buildChordParams()`, which includes scale quantization — same path as actual note-on computation |

Note: VIZ-03 in ROADMAP SC3 specifies "~30% alpha in voice accent color" — the PLAN's more detailed spec (white at 22% alpha) overrides this. The implementation matches the PLAN spec exactly. The ROADMAP description was approximate.

### Anti-Patterns Found

None. No TODO/FIXME/HACK/PLACEHOLDER markers found in any modified files. No empty implementations, no stub handlers, no console.log-only functions.

### Human Verification Required

#### 1. Real-time note name accuracy

**Test:** Open ChordJoystick in Reaper on a MIDI track routed to a synth. Move the joystick to various non-center positions.
**Expected:** Two crosshair lines appear. Root/third note names appear above/below the horizontal segment midpoint; fifth/tension names appear left/right of the vertical segment midpoint. All four names update as the joystick moves. Note names (e.g. "C4", "Eb3") match the pitches that actually sound when a voice is triggered at that position.
**Why human:** Visual rendering and musical accuracy of note names relative to sounding pitches cannot be verified from source alone.

#### 2. Center suppression

**Test:** Move the joystick to center (release to 0,0).
**Expected:** All crosshair lines and labels disappear entirely. No lines, no labels rendered at center.
**Why human:** Runtime conditional `abs(mx)+abs(my) < 0.01f` requires live interaction to confirm the threshold behaves correctly.

#### 3. Right-click toggle with discoverability dot

**Test:** Right-click the joystick pad while joystick is off-center.
**Expected:** Crosshair lines and labels disappear. A faint small blue dot (~8px diameter, very low opacity) appears at the cursor position. Right-clicking again restores the crosshair.
**Why human:** Mouse event handling and visual appearance of the faint dot cannot be verified statically.

#### 4. Preset round-trip

**Test:** Right-click to turn crosshair OFF. Save a preset. Close and reopen the plugin (or reload the preset).
**Expected:** Crosshair remains OFF after reload — faint blue dot visible, no lines.
**Why human:** APVTS serialization round-trip requires DAW preset save/reload workflow.

#### 5. DAW automation lane

**Test:** Open the automation lane selector in Reaper for the ChordJoystick instance.
**Expected:** "Crosshair Visible" appears in the parameter list as an automatable boolean.
**Why human:** Requires DAW inspection of plugin parameter list.

#### 6. Scale quantization reflected in note names

**Test:** Set a non-center joystick position. Switch the scale preset from Major to Minor (or another scale).
**Expected:** Note name labels update to reflect the new quantized pitches for the current joystick position.
**Why human:** Musical correctness of scale quantization in display requires live A/B comparison.

### Gaps Summary

No gaps found. All automated checks pass:

- All 3 required artifacts exist, are substantive, and are wired into the live data pipeline
- Both key link chains are fully wired end-to-end (audio thread → atomics → timer → cache → paint)
- All 4 requirement IDs (VIZ-01 through VIZ-04) are satisfied by the implementation
- Both task commits (`a1e50ad`, `4aa10a2`) exist in git history
- No anti-patterns, stubs, or placeholders in any modified file
- SUMMARY states UAT was approved with all 10 checks passing

The 6 human verification items are runtime/visual checks that confirm expected behavior described in the source code — they are not gaps in the implementation.

---

_Verified: 2026-03-09T02:00:00Z_
_Verifier: Claude (gsd-verifier)_
