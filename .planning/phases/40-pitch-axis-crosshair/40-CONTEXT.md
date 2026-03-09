# Phase 40: Pitch Axis Crosshair Visualization — CONTEXT

## Goal
Two subtle lines extend from the cursor to the center axes of the joystick pad, with
octave-qualified note names centered on each segment, giving the player immediate pitch
feedback without cluttering the space visual.

## Decisions

### A. Note name data pipeline

- **Audio thread** writes 4 new `std::atomic<int> livePitch0_..3_` to `PluginProcessor`
  each `processBlock`, computed from the live cursor position (not sample-and-hold).
- These use the same `buildChordParams()` + `ChordEngine::computePitch()` path as actual
  note-ons, so they reflect the effective position (gamepad overrides mouse when deflected).
- `JoystickPad::timerCallback()` reads the 4 atomics at 30 Hz — no APVTS polling needed.
- Note names track **raw `modulatedJoyX_/Y_`** (not spring-smoothed `displayCx_/displayCy_`),
  for tight lock with actual sounding pitches.
- Pitch values are post-scale-quantization (same as note-on path).
- Note name format: **octave-qualified** — e.g. "C4", "Eb3", "G4", "Bb4". Use
  `juce::MidiMessage::getMidiNoteName()` or manual `kN[pc] + String(octave)`.

### B. Visibility and toggle

- Crosshair is **always visible by default** — no trigger/gate/pad-state condition.
- **Center suppression:** lines hidden when `std::abs(modulatedJoyX_) + std::abs(modulatedJoyY_) < 0.01f`
  (reuses existing ~1% dead zone threshold).
- **Toggle:** right-click on `JoystickPad` flips a new `crosshairVisible` APVTS `bool`
  parameter (default ON). Persists with preset, automatable.
- **When OFF:** a faint dot (~4 px radius, `Clr::accent` at ~15% alpha) is drawn at
  `displayCx_/displayCy_` as a discoverability hint. No other trace.
- A `[right-click to toggle crosshair]` tooltip covers discoverability.

### C. Line geometry and label placement

- **Line shape:** two half-segments forming an "L" anchored at pad center (0,0):
  - Horizontal segment: from `(displayCx_, displayCy_)` → `(padCenterX, displayCy_)`
  - Vertical segment: from `(displayCx_, displayCy_)` → `(displayCx_, padCenterY)`
- Note names are **centered on their segment** (midpoint of each half-segment).
- **Label collision suppression:** if the cursor is close enough that a centered label
  would overlap the cursor sprite, that label is hidden. Threshold: label center within
  ~20px of cursor center.
- **Font size:** fixed ~10–11px, matching existing small labels in the plugin UI.
- **Note name layout:**
  - Horizontal segment → Root name above the line, Third name below the line
  - Vertical segment → Fifth name to the left of the line, Tension name to the right

### D. Color scheme

- **Lines:** `juce::Colours::white.withAlpha(0.22f)`, 1px stroke.
  White at low alpha reads as a clean cool gray on the dark space background, clearly
  distinct from the semitone grid (`Clr::accent` blue at 35% alpha), and unifies the
  crosshair as a neutral HUD layer.
- **Note name labels:** `juce::Colours::white.withAlpha(0.75f)` — bright enough to read
  clearly, clearly more prominent than the line, no per-voice coloring needed (position
  already identifies the voice: above=root, below=third, left=fifth, right=tension).
- `kVoiceClr[4]` is NOT used for crosshair — reserved for particles/bursts only.

## Code context

| Symbol | File | Notes |
|--------|------|-------|
| `modulatedJoyX_/Y_` | `PluginProcessor.h:203` | Live cursor atomics, already read by JoystickPad |
| `buildChordParams()` | `PluginProcessor.cpp:564` | Audio-thread call site for livePitch computation |
| `ChordEngine::computePitch()` | `ChordEngine.h:47` | Pure static, safe anywhere |
| `displayCx_/displayCy_` | `PluginEditor.h:163` | Spring-smoothed pixel cursor — use for draw position |
| `Clr::accent` | `PluginEditor.cpp:12` | `0xFF1E3A6E` mid-blue — NOT used for crosshair lines |
| `voiceTriggerFlash_` | `PluginProcessor.h:236` | Pattern reference for new livePitch_ atomics |
| `mouseDoubleClick` | `PluginEditor.h:120` | Already wired — do NOT reassign; use `mouseUp` with right-button check |
| `crosshairVisible` | new APVTS bool | Default true, right-click toggles |
| `livePitch0_..3_` | new processor atomics | Written in processBlock after buildChordParams() call |

## Scope guardrail
- No new UI buttons or controls — toggle is right-click only
- No changes to spring-damper, starfield, or particle systems
- No changes to audio path — `livePitch_` atomics are write-only side effect of existing computation
- Phase 43 (resizable UI) handles font scaling — use fixed size here
