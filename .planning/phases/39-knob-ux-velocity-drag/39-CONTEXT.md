# Phase 39 Context — Knob UX: Velocity Drag & Visual Indicators

**Phase goal:** Knob interaction feels professional — velocity-sensitive drag, EMA-smoothed speed, hover highlight, and 12-dot snap indicators on all octave/interval/transpose knobs.

---

## Decision 1 — Dot layout on octave/interval/transpose knobs

**Decided:** 12 evenly-spaced 2 px filled dots at each snap position around the rotary arc.

**Rationale:** All 8 target knobs share the same 0–11 range (12 values), confirmed from APVTS registration:
- `rootOctave / thirdOctave / fifthOctave / tensionOctave`: 0..11
- `thirdInterval / fifthInterval / tensionInterval`: 0..11
- `globalTranspose`: 0..11

**Implementation spec:**
- 12 dots placed at even angular intervals along the rotary arc (same arc bounds as the existing track ring: `rotaryStartAngle` to `rotaryEndAngle`)
- Each dot: 2 px radius filled circle
- Inactive dot colour: `Clr::accent` (mid-blue, ~50% alpha)
- Active dot (current value): `Clr::highlight` (bright pink-red, full alpha) — replaces the current red position ring
- The existing white indicator line (knob pointer) is **kept** — the brighter dot supplements it visually, it does not replace the pointer
- Red position ring / fill arc: **removed** for these 8 knobs only; other knobs keep the existing arc behaviour

**Detection in drawRotarySlider:** Check if the slider's APVTS parameter ID is one of the 8 target IDs. Practical approach: check `slider.getRange()` — if min==0, max==11, interval==1 → render dots mode. This is robust without needing string lookups in the paint path.

---

## Decision 2 — Velocity-sensitive drag (EMA on speed)

**Architecture:** EMA applied to the **drag velocity** (px/frame), not the output value. The smoothed speed drives a sensitivity multiplier. Output value is never filtered — what you dial is what you get.

**Parameters:**
- Base sensitivity: **300 px** for full-range sweep at 1× multiplier
- Velocity → multiplier curve: linear ramp from **1×** at ≤2 px/frame to **3×** at ≥10 px/frame (clamped both ends)
- EMA alpha: **0.25** (heavier smoothing = slower ramp-up/ramp-down, eliminates direction-reversal spike)
  - `smoothedSpeed = alpha * currentSpeed + (1 - alpha) * smoothedSpeed`
- Direction reversal: speed passes through 0 naturally → multiplier drops to 1× without a jump (no explicit reversal detection needed)
- Shift+drag: **kept** as the standard JUCE fine-tune modifier (~10× slower, ~3000 px sweep). Works on top of the velocity system.
- Double-click to reset to default: **kept** (JUCE standard).

**Implementation approach:** Subclass `juce::Slider` as `VelocityKnob` (or `VelocitySlider`). Override `mouseDrag()` to:
1. Compute raw delta from `event.getDistanceFromDragStartY()` diff between calls
2. Compute `currentSpeed = abs(delta)`
3. Update `smoothedSpeed_` via EMA
4. Derive `multiplier` from the linear ramp
5. Call `setValue(getValue() - delta * multiplier / (300.0 * range))` directly rather than relying on JUCE's internal drag handling
6. Store `prevY_` for delta calculation

**No impact on:** linear sliders (LFO faders use `drawLinearSlider`, separate code path, not subclassed here).

---

## Decision 3 — Velocity drag scope (which knobs)

**In scope (replace `juce::Slider` with `VelocityKnob`):**
- `globalTransposeKnob_` — 0..11 semitones
- `thirdIntKnob_`, `fifthIntKnob_`, `tensionIntKnob_` — 0..11 semitones (note: these are `ScaleSnapSlider` subclasses — `VelocityKnob` must be added to the `ScaleSnapSlider` hierarchy or as a parallel mixin)
- `joystickXAttenKnob_`, `joystickYAttenKnob_` — 0..127 wide range
- Mod Fix knobs (filterXAttenKnob_, filterYAttenKnob_) — 0..127
- +/- Mod X and Y (LFO cross-mod attenuation sliders from Phase 38.3)
- Tempo knob (freeBpmKnob_ or equivalent) — 30..240 BPM
- `randomProbabilityKnob_` — 0..100
- `randomPopulationKnob_` — 1..64
- Gate length slider in arp box

**Also in scope (LFO panel faders — linear sliders):**
- Rate, Phase, Level, Dist linear sliders for both LFO X and LFO Y
- These use `drawLinearSlider` — velocity behaviour needs to be added to the linear slider path, not `drawRotarySlider`
- Same EMA+multiplier logic, applied in a `VelocitySlider` subclass for linear sliders

**Out of scope (excluded):**
- Octave knobs (`rootOctKnob_` etc.) — only 12 values, coarse by design, fine drag already covers them
- Voice channel selectors, combo boxes — not sliders
- Any slider with very narrow range (e.g., singleChanTarget 1..16)

**ScaleSnapSlider conflict:** `thirdIntKnob_` etc. already subclass `ScaleSnapSlider : public juce::Slider`. Solution: make `ScaleSnapSlider` inherit from `VelocityKnob` instead of `juce::Slider`, so both `snapValue()` and velocity drag are present. Alternatively, add velocity logic directly to `ScaleSnapSlider` as it is small and self-contained.

---

## Decision 4 — Hover highlight

**Style:** Thin outer ring drawn just outside the track arc, at `~35% alpha` of `Clr::highlight` (bright pink-red). Appears on `isMouseOver() == true`, disappears immediately on mouse leave (no fade needed — JUCE repaints on every mouse enter/leave).

**Implementation:** In `PixelLookAndFeel::drawRotarySlider()`, add at the top of the function body:
```cpp
if (slider.isMouseOver())
{
    g.setColour(Clr::highlight.withAlpha(0.35f));
    g.drawEllipse(outerRingBounds.expanded(2.0f), 1.0f);
}
```
where `outerRingBounds` is the same rect used for the existing track arc.

**Coverage:** All knobs that go through `drawRotarySlider` — i.e., all `PixelLookAndFeel`-styled rotary sliders. This includes the 8 dot-mode knobs and all other rotary knobs. No exclusions needed.

**Linear sliders:** Hover on LFO faders is out of scope for this phase — the linear slider path (`drawLinearSlider`) is more complex and the faders are narrow; hover there adds little value.

---

## Code context

| Symbol | File | Role |
|---|---|---|
| `PixelLookAndFeel::drawRotarySlider` | PluginEditor.cpp ~L52 | All rotary knob painting — add dot mode + hover ring here |
| `ScaleSnapSlider` | PluginEditor.h ~L270 | Interval knobs — needs VelocityKnob inheritance or inline velocity logic |
| `styleKnob()` | PluginEditor.cpp ~L2167 | Sets slider style for all rotary knobs — used at construction |
| `Clr::highlight` | PluginEditor.cpp ~L8 | Bright pink-red — active dot + hover ring colour |
| `Clr::accent` | PluginEditor.cpp ~L8 | Mid-blue — inactive dot colour |
| `rootOctave` etc. | PluginProcessor.cpp ~L143 | APVTS range 0..11 confirms 12 dots |

## Deferred ideas (captured, not in this phase)
- Hover on linear sliders (LFO faders)
- Animated hover fade-in/out
- Custom text display on hover showing exact value + unit
