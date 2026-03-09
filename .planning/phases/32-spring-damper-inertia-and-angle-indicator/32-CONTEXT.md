# Phase 32: Spring-Damper Inertia and Angle Indicator — Context

**Gathered:** 2026-03-03
**Status:** Ready for planning

<domain>
## Phase Boundary

Two visual features added to `JoystickPad`:

1. **Spring-damper inertia** — the cursor dot's screen position lags behind the actual joystick
   value with physical weight. Fast moves cause visible lag (~90 ms settle); slow moves track
   precisely. **Pitch is always instantaneous** — only the visual dot position is smoothed.

2. **Perimeter arc + note labels** — a thin arc segment on the inscribed circle's circumference
   shows the polar angle from center to the raw joystick position. Four note labels
   (Root, Third, Fifth, Tension) sit at the cardinal positions as a permanent compass.

No new APVTS parameters. No PluginProcessor changes. Pure `timerCallback()` + `paint()` work.

</domain>

<decisions>
## Implementation Decisions

### Spring-Damper Model

- **Algorithm**: Velocity-spring in pixel space (not normalized). Each `timerCallback()` frame:
  ```
  springVelX_ += (targetCx - displayCx_) * kSpring;
  springVelY_ += (targetCy - displayCy_) * kSpring;
  springVelX_ *= kDamping;
  springVelY_ *= kDamping;
  displayCx_  += springVelX_;
  displayCy_  += springVelY_;
  ```
- **Constants**: `kSpring = 0.18f`, `kDamping = 0.72f`
  - Settle time: ~90 ms (5–6 frames at 60 Hz to within 5% of target)
  - Character: slightly underdamped — one small overshoot on fast moves, then settles; feels like
    physical inertia without being bouncy
- **Target**: `targetCx/Cy` = the same pixel position currently computed in `timerCallback()` as
  `cx/cy` (from `dispX/dispY`, which already respects the LFO-active branch)
  - The existing `curCx_/curCy_` assignment at line ~1479 becomes the spring target, not the
    display position
- **Sample-and-hold contract**: `proc_.joystickX` and `proc_.joystickY` are NEVER read with intent
  to write back or smooth. The spring only operates on derived **pixel coordinates**.
- **Initialization**: On first frame (when `prevCx_` is still -9999), snap `displayCx_/Cy_` to
  `curCx_/curCy_` directly (no spring ramp from (0,0) on load)
- **Clamping**: `displayCx_/Cy_` is NOT clamped — the spring can overshoot the pad bounds
  slightly (overshoot = 1–3 px). The `paint()` dot draw uses `displayCx_/Cy_` directly without
  re-clamping, so the overshoot is visually visible (adds to the physical feel). The existing
  border hides out-of-bounds extremes naturally.

### What Changes in paint()

- The cursor dot section at line ~2535 currently re-reads `dispX/dispY` from the processor and
  recomputes `cx/cy` as local floats. In Phase 32, that computation is **removed**; `paint()`
  reads `displayCx_` and `displayCy_` directly (spring-smoothed pixel position).
- **Nothing else in paint() changes** — particles spawn from `curCx_/curCy_` (raw target,
  line ~1479), not display position, so gold dust trails still follow the real joystick.

### New Private Members

```cpp
// Phase 32: spring-damper display position (pixel coords, display-only)
float displayCx_   = 0.0f;   // spring-smoothed cursor pixel X
float displayCy_   = 0.0f;   // spring-smoothed cursor pixel Y
float springVelX_  = 0.0f;   // spring velocity X (pixels/frame)
float springVelY_  = 0.0f;   // spring velocity Y (pixels/frame)
```

### Perimeter Arc

- **Position**: On the inscribed circle's circumference — `circleR` computed in `paint()` from
  `juce::jmin(b.getWidth(), b.getHeight()) * 0.5f`, centered at `b.getCentreX/Y()`
- **Source**: Arc angle is derived from **raw joystick position** (`proc_.joystickX/Y`), NOT
  from `displayCx_/Cy_`. This ensures the arc always reflects the sounding pitch direction
  instantly — even while the dot is still catching up.
- **Angle**: `atan2(-rawJoyY, rawJoyX)` in screen coordinates (Y negated: joystick +Y = screen up)
- **Arc span**: ±20° (40° total) centered on the computed polar angle
- **Style**: `juce::Path::addArc()` on the `circleRect`, line width 1.5px
- **Color**: `Clr::highlight` (cyan) at alpha 0.55f
- **Draw order**: drawn immediately after the circle outline (before particles and cursor dot)
- **Center dead zone**: if `std::sqrt(rawX²+rawY²) < 0.08f`, arc is not drawn (joystick at rest
  near center — no meaningful direction)

### Note Labels (Permanent Compass)

- **Positions** (relative to circle circumference, outside by ~12px):
  - **Root**: bottom of vertical axis (joyY = -1 → screen bottom) — label at bottom of circle
  - **Third**: top of vertical axis (joyY = +1 → screen top) — label at top of circle
  - **Fifth**: left of horizontal axis (joyX = -1 → screen left) — label at left of circle
  - **Tension**: right of horizontal axis (joyX = +1 → screen right) — label at right of circle
- **Idle opacity**: alpha 0.38f, font 10px, color `Clr::text`
- **Active highlight**: the label nearest to the current arc angle brightens to alpha 0.75f
  — "nearest" = label within ±50° of the current polar angle
- **Implementation**: drawn in `paint()` after the arc, before particles — 4 fixed
  `g.drawText()` calls with the active label computed from current angle
- **No new members**: angle is recomputed in `paint()` from the same raw joystick reads already
  present at the cursor dot section

### Draw Order in paint() (changes from Phase 31)

Insertion points relative to Phase 31 order:
1. Near-black base fill
2. Galaxy background
3. Warp convergence glow
4. Planet pool
5. Grid lines + warp rings + starfield
6. Gamepad corner mask
7. Circle outline (existing)
8. **[NEW] Perimeter arc** (on circle edge, raw-position angle)
9. **[NEW] Note labels** (4 cardinal compass labels, nearest brightened)
10. Particles
11. Static centre reference
12. Dark halo behind cursor dot
13. Base glow ring (Phase 31 idle glow)
14. **Cursor dot** — now reads `displayCx_/displayCy_` (spring-smoothed) instead of recomputed cx/cy
15. Border rect (teal pulsing)
16. Warp flash overlay

### Handling the LFO display branch

The current cursor draw in `paint()` has:
```cpp
const float displayX = (lfoXActive || lfoYActive) ? proc_.modulatedJoyX_... : proc_.joystickX...;
```
In Phase 32:
- **Spring target** (`curCx_/Cy_` in timerCallback) continues to track `dispX/dispY` (LFO-aware)
  → the spring smooths the LFO-modulated display, which is correct
- **Perimeter arc** reads `proc_.joystickX/Y` directly (raw, no LFO) — arc always shows true
  joystick direction, not LFO-shifted position

### Claude's Discretion
- Exact LCG seed or RNG for snap-detection threshold (PS4 spring-back guard is `kSnapThresh=0.30f`
  already in the sphere system — no interaction with display spring)
- Exact font used for note labels (use existing `g.setFont()` style in the pad)
- Whether to use `juce::Path` or `g.drawArc()` for the arc — use `juce::Path::addArc()` for
  consistent anti-aliasing with the rest of the pad

</decisions>

<code_context>
## Existing Code Insights

### Integration Points

- **`timerCallback()`** lines ~1450–1480: spring target (`cx/cy`) is computed here from
  `dispX/dispY` and stored in `curCx_/curCy_`. Spring-damper update integrates after this.
- **`paint()`** lines ~2535–2620: cursor dot section computes `cx/cy` locally from `dispX/dispY`.
  In Phase 32, replace that local computation with reads of `displayCx_/displayCy_`.
- **`circleRect`** is already declared in `paint()` at line ~1820:
  `juce::Rectangle<float>(b.getCentreX()-circleR, b.getCentreY()-circleR, circleR*2, circleR*2)`.
  Perimeter arc reuses this rect directly.
- **`JoystickPad` private members** in `PluginEditor.h` lines 157–200: add 4 new spring members.

### Key Constraint (from MEMORY.md)

> Spring-damper display position (`displayCx_/Cy_`) must NEVER write back to
> `proc_.joystickX/Y` — sample-and-hold contract is inviolable.

### Particle spawn still uses raw target

Gold particles (`spawnGoldParticles`) are called at line ~1475 with `cx, cy` (the raw target).
This is intentional — particle trails emanate from where the sound actually is.

### No Processor Changes

Zero changes to `PluginProcessor.h/cpp`. Phase is entirely UI-side.

</code_context>

<deferred>
## Deferred Ideas

- **Radial line from center to arc** — suggested as an option, rejected for this phase; arc-only
  is cleaner with the busy starfield background
- **Velocity-based arc width** — arc span grows wider during fast moves; interesting but adds
  complexity to Phase 32; candidate for Phase 33 emotional states
- **Spring affects particle spawn direction** — particles could lean toward overshoot direction
  during lag; deferred to Phase 33
- **Pitch-position star markers** — stars positioned at semitone intervals driven by attenuation
  knobs; deferred from Phase 31; still a candidate for a future visual phase

</deferred>

---

*Phase: 32-spring-damper-inertia-and-angle-indicator*
*Context gathered: 2026-03-03*
