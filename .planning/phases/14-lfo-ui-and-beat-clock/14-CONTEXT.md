# Phase 14: LFO UI and Beat Clock — Context

**Gathered:** 2026-02-26
**Status:** Ready for planning

<domain>
## Phase Boundary

Add two LFO control panels (LFO X and LFO Y) to the left of the joystick pad and a beat clock pulse indicator embedded in the BPM display label. All controls are attached to APVTS. The joystick pad shifts right to accommodate.

Technical contract (from success criteria, locked):
- LFO X and LFO Y panels are visible to the left of the joystick pad; the joystick pad is shifted right
- Each panel: shape dropdown, Rate slider, Phase slider, Distortion slider, Level slider, Enabled toggle (LED in header), Sync button — all attached to APVTS
- Beat clock dot embedded inline in the BPM display label — flashes once per beat
- Toggling Sync switches the Rate slider between Hz display and subdivision-step display — no layout shift
- The JoystickPad dot visually tracks the LFO-modulated X/Y position when either LFO is active (deferred from Phase 12 CONTEXT.md)

</domain>

<decisions>
## Implementation Decisions

### Editor size

**Width**: Grow from 920px to ~1120px.

Layout from left to right:
```
┌─────────────────────────────────────────────────────────────────┐
│  LEFT COLUMN (~460px)  │ LFO X │ LFO Y │  Joystick + knobs     │
│  (scale, looper, etc.) │ ~130px│ ~130px│       ~340px           │
└─────────────────────────────────────────────────────────────────┘
  ~460px                   ~130   ~130      ~340   = ~1060 + margins
```

- Left column unchanged (scale, trigger sources, looper, filter mod, gamepad, arp)
- Two new LFO panel columns inserted between divider and joystick
- Joystick area shifts right; joystick pad retains 300×300 size

**Height**: Unchanged at 810px.

---

### LFO panel arrangement

- **LFO X** is the **left** panel (X-axis LFO — modulates fifth/tension voices)
- **LFO Y** is the **right** panel (Y-axis LFO — modulates root/third voices)
- Panels are side-by-side horizontally, each ~130px wide
- Top-aligned with the joystick pad (same top edge)
- Natural height — panels are only as tall as the 7 controls require; bottom space is empty

---

### LFO panel visual style

- Same rounded-rect panel style as LOOPER / FILTER MOD / GAMEPAD panels (from Phase 11)
- Panel fill: `Clr::panel.brighter(0.12f)`
- Panel border: `Clr::accent.brighter(0.5f)`
- Corner radius: match existing panels (~6–8px)

**Panel header label:**
- Text: `"LFO X"` / `"LFO Y"` — knockout style floating on top border (same as other section panels)
- Font: `Clr::textDim`, 9–10px bold

**Enabled toggle (LED in header):**
- An 8px colored circle embedded in the panel title border area, to the right of the label text
- Off: dark grey fill (`Clr::gateOff` or similar)
- On: bright green (`Clr::gateOn`, `0xFF00E676`)
- Clicking the LED (or the header area) toggles the `lfoXEnabled` / `lfoYEnabled` APVTS parameter
- No separate [ON/OFF] button row needed

---

### LFO panel control layout (top to bottom)

All controls use **horizontal linear sliders** (same PixelLAF `drawLinearSlider` as elsewhere):

| Row | Control | Notes |
|-----|---------|-------|
| 1 | Shape dropdown (ComboBox) | Full-width ComboBox; 7 waveforms: Sine, Triangle, Saw Up, Saw Down, Square, S&H, Random |
| 2 | Rate | Slider; label "Rate" on left; value display changes by sync mode (see below) |
| 3 | Phase | Slider 0.0–1.0; label "Phase" |
| 4 | Level | Slider 0.0–1.0; label "Level" |
| 5 | Distortion | Slider 0.0–1.0; label "Dist" |
| 6 | [SYNC] button | Full-width TextButton; accent-toggle style (active = bright fill, inactive = dim) |

Row height: ~18–20px per slider row + 4px gap. Total panel height ≈ 160–180px.

**Label placement**: Short label (left-aligned, ~28px wide) + horizontal slider filling the rest of the row width.

---

### Rate slider — free mode

- Range: 0.01 Hz – 20 Hz
- Taper: **logarithmic** (equal knob travel per decade; from Phase 12 CONTEXT.md)
- Value display: e.g. `"2.50 Hz"`

### Rate slider — sync mode (SYNC button active)

- The same slider widget is used — no component swap, no layout shift
- Range maps to discrete subdivision steps:
  - Straight: 1/32, 1/16, 1/8, 1/4, 1/2, 1/1, 2/1, 4/1 (and up to loopLengthBars/1)
  - Dotted: 1/32., 1/16., 1/8., 1/4., 1/2.
  - Triplet: 1/32T, 1/16T, 1/8T, 1/4T, 1/2T
- Slider snaps to discrete integer steps (one step per subdivision entry)
- Value display: subdivision text, e.g. `"1/4"`, `"1/8."`, `"1/4T"`
- Label text stays `"Rate"` in both modes

**When SYNC is active and DAW is NOT playing**: LFO uses Free BPM knob value as the beat duration fallback. LFO continues moving.

---

### Beat clock dot (inline in BPM label)

**Host**: Inline inside the `bpmDisplayLabel_` text. The label text is formatted as e.g. `"120 BPM ●"` where `●` is rendered as a colored circle drawn over the label text area.

Implementation options:
1. Draw the dot in the PluginEditor `paint()` pass, positioned to the right of the BPM label bounds — no changes to the label widget itself
2. Or use a custom `juce::Component` overlaid on the label bounds

Claude's discretion on exact implementation — the dot must sit within or immediately adjacent to the BPM label text, and must not collide with neighboring controls.

**Visual behavior (LED pulse):**
- On beat trigger: dot fills with `Clr::accent` (cyan)
- Fade out: brightness decays over ~300ms (using the existing 30 Hz timer — approximately 9 frames to decay fully)
- Between beats: dot is dark (`Clr::textDim` at low opacity or a dim grey)
- Implementation: a `float beatPulse_` member (0.0–1.0), set to 1.0 on each beat, decremented by ~0.11 per 30 Hz tick until 0.0

**Dot size**: 8px diameter circle.

**Beat source (automatic):**
- When DAW transport is running: flashes on DAW beats (derived from `ppqPos` in `processBlock`)
- When DAW is stopped: flashes based on Free BPM knob (`randomFreeTempoKnob_`) value
- No user toggle required — the source switches automatically

**Beat detection signal**: The processor must expose a beat-occurred flag accessible from the editor repaint path (e.g. an `std::atomic<bool>` `beatOccurred_` that processBlock sets and the editor timer resets after reading).

---

### JoystickPad dot visual tracking (deferred from Phase 12)

When either LFO is active (enabled=true, level>0), the `JoystickPad` dot should visually move to reflect the LFO-modulated position, not just the raw joystick position.

- The processor must expose the final modulated X/Y values (post-LFO) as atomics readable from the UI thread (e.g. `std::atomic<float> modulatedJoyX_, modulatedJoyY_`)
- `JoystickPad::paint()` reads these instead of `proc_.joystickX` / `proc_.joystickY` when an LFO is active
- If both LFOs are off, fall back to the raw joystick position (current behavior)

</decisions>

<specifics>
## Specific Implementation Notes

- `setSize(1120, 810)` in PluginEditor constructor (up from 920)
- The `dividerX_` calculation in `resized()` needs adjustment — the new divider (between left column and LFO panels) remains at ~460px from the left edge
- LFO panel bounds: `lfoXPanelBounds_` and `lfoYPanelBounds_` — set in `resized()`, drawn in `paint()`
- Shape ComboBox items must match the waveform order in `LfoEngine` exactly (index 0 = Sine, 1 = Triangle, etc.)
- APVTS parameter IDs follow the pattern `"lfoXShape"`, `"lfoXRate"`, `"lfoXPhase"`, `"lfoXLevel"`, `"lfoXDist"`, `"lfoXEnabled"`, `"lfoXSync"` (and same for Y) — these were established in Phase 13
- Rate slider in sync mode: use a `valueToTextFunction` / `textToValueFunction` on the slider attachment to map integer indices → subdivision strings
- The `beatPulse_` float lives in `PluginEditor` and is updated in `timerCallback()`
- Beat signal: `proc_.beatOccurred_` atomic — processBlock sets it, editor timer reads-and-resets it each tick

</specifics>

<deferred>
## Deferred Ideas

- LFO panel Enabled LED could eventually have a slow "breathing" animation when enabled but no notes are playing — deferred, not part of v1.4 scope
- Clicking the shape ComboBox could show a mini waveform preview — deferred

</deferred>

---

*Phase: 14-lfo-ui-and-beat-clock*
*Context gathered: 2026-02-26*
