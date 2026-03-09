# Phase 42: Warp Space Effect — CONTEXT

## Goal

When the looper enters playback mode, the joystick pad background transforms into a
cinematic warp tunnel with a 4000ms ease-in ramp. Stars stretch into radially accelerating
streaks emanating from the cursor (vanishing point). Looper stop ramps back down over 4000ms.

---

## Decisions

### A. Star motion model during warp

- **Existing ambient starfield (250 stars) persists during warp** — not replaced.
  - **Large stars freeze:** stars with `r > 1.2f` stop drifting entirely during warp
    (they read as distant background nebula — stationary).
  - **Small stars continue drifting:** stars with `r <= 1.2f` keep their lateral
    drift velocities unchanged — they read as nearby floating dust.
  - Both groups have their visual appearance (dot → streak) driven by `warpT`
    based on their distance from the vanishing point.

- **Dedicated warp star pool** added alongside the ambient field:
  - Capacity: `warpStarCapacity = (int)(population * 2)` — scales 2–128 with
    the population knob (pop=1 → barely visible, pop=64 → dense storm).
  - These are the primary tunnel effect. Separate `std::vector<WarpStar> warpStars_`
    in `JoystickPad`.

```cpp
struct WarpStar {
    float angle;   // radial direction from vanishing point (radians)
    float dist;    // current distance from vanishing point (normalized 0..1)
    float speed;   // current radial speed (accelerates from ~0.001 to ~0.04/frame)
};
```

- **Spawn:** at the vanishing point (`displayCx_/displayCy_`) with `dist ≈ 0`,
  random `angle`, near-zero initial speed.
- **Acceleration:** `speed += 0.0008f * warpT` each frame — slow near center
  (short dashes), fast near edge (long streaks).
- **Respawn:** when `dist > 1.2f` (exited pad bounds), reset to `dist = 0`,
  new random angle — continuous recycling.
- **In-flight trajectories are straight** — no mid-flight bending. Responsive
  feel comes entirely from new spawns originating from the updated cursor position
  each tick.

### B. Warp ramp

- **Trigger (ramp up):** `proc_.looperIsPlaying()` transitions false → true.
  Covers both: user pressing play directly AND auto-playback after a REC cycle
  completes.
- **Trigger (ramp down):** `proc_.looperIsPlaying()` transitions true → false.
  Starts immediately — no waiting for loop cycle to finish.
- **Duration:** 4000ms in each direction (120 ticks at 30 Hz).
- **Ramp variable:** `float warpRamp_` (0..1), updated in `timerCallback()`:

```cpp
const bool isPlaying = proc_.looperIsPlaying();
const float step = 1.0f / (4.0f * 30.0f);   // ≈ 0.00833/tick
warpRamp_ = isPlaying
    ? juce::jmin(1.0f, warpRamp_ + step)
    : juce::jmax(0.0f, warpRamp_ - step);
```

- **Easing:** `warpT = warpRamp_ * warpRamp_ * (3.0f - 2.0f * warpRamp_)`
  (smoothstep) — applied at draw time. `warpT` drives all warp visual parameters:
  streak length, color blend, freeze threshold, spawn rate.

### C. Vanishing point (cursor tracking)

- The radial origin of all warp stars tracks `displayCx_/displayCy_`
  (spring-smoothed cursor pixel position) — updated every timerCallback tick.
- Banking right shifts the vanishing point right → streaks lean right →
  cinematic "turning in warp" feel with zero extra code beyond reading existing atomics.
- Warp stars store `angle` from their spawn origin; at draw time their pixel
  position is computed as:

```cpp
const float vpX = displayCx_ / getWidth();   // normalized vanishing point
const float vpY = displayCy_ / getHeight();
const float px  = vpX + std::sin(ws.angle) * ws.dist;
const float py  = vpY - std::cos(ws.angle) * ws.dist;
```

### D. Streak visual: shape and color

**Shape (warp stars):**
- Each warp star draws as a `juce::Line` from tail to head:
  - **Head** (outer end, bright): current position `(px, py)`
  - **Tail** (inner end, dim): `(px - sin(angle) * streakLen, py + cos(angle) * streakLen)`
  - `streakLen = ws.speed * 400.0f * warpT`  (px at 400px pad width; scales with speed and ramp)
- Line drawn with `g.drawLine(tail → head, strokeWidth)` where `strokeWidth ≈ 1.0f`
  for distant stars, `1.5f` for the fastest (most radially displaced) ones.

**Color (warp stars — gradient along streak):**
- Brightness: bright at head, fading toward tail (physically correct light-trail).
- Color temperature: based on each star's distance from the vanishing point at draw time:
  - Near vanishing point (dist < 0.3): blue-white `#aaddff`
  - Far from vanishing point (dist > 0.7): barely warm white `#fff8f0`
  - Blend zone (0.3–0.7): linear interpolation between the two
- Gradient along streak: drawn as two-endpoint colour interpolation
  (`g.setGradientFill` with tail colour = head colour darkened to ~15% alpha).

**Ambient star appearance during warp (dot → streak):**
- At `warpT > 0`, ambient stars also elongate slightly in the direction of the
  vanishing point (radial from their position toward `vpX/vpY`).
- Streak length: `ambientStreakLen = s.r * warpT * 6.0f` (very subtle at low warp,
  max ~6× radius at full warp — they stay short compared to warp stars).
- Colour tint: at full `warpT`, stars within `dist < 0.3` of vanishing point shift
  ~30% toward `#aaddff`; peripheral stars shift ~20% toward `#fff8f0`. Applied
  using `s.c.interpolatedWith(targetClr, warpT * blend)`.

### E. Population knob interaction

- **Ambient star count:** existing `visCount` formula unchanged —
  `visCount = floor(starfield_.size() * (pop - 1) / 63)`. Low pop = few ambient stars.
- **Warp star pool capacity:** `warpStarCapacity = (int)(population * 2.0f)`.
  Warp star vector is resized in `timerCallback()` when capacity changes (or just
  capped at draw time — no allocation during playback).
- **At minimum population (pop=1):** 2 warp stars + ~0 ambient stars visible →
  effectively no warp effect. Consistent: warp grows with the world you've built.

---

## Code context

| Symbol | File | Notes |
|--------|------|-------|
| `StarDot { x, y, r, vx, vy, c }` | `PluginEditor.h:136` | Existing ambient star struct |
| `starfield_` | `PluginEditor.h:157` | 250 ambient stars, shuffled |
| `glowPhase_` | `PluginEditor.h:158` | Pattern for new `warpRamp_` float member |
| `displayCx_/displayCy_` | `PluginEditor.h:163` | Spring-smoothed cursor px — vanishing point |
| `proc_.looperIsPlaying()` | `PluginProcessor.h:60` | Ramp trigger source, read in timerCallback |
| `timerCallback()` | `PluginEditor.cpp:~1270` | 30 Hz — update `warpRamp_`, animate warpStars_ |
| `paint()` starfield block | `PluginEditor.cpp:~1538` | Layer 3 — extend with warp star draw pass |
| `resized()` starfield gen | `PluginEditor.cpp:1053` | Add `warpStars_` init here |
| `randomPopulation` | APVTS param | Raw value 1..64 — drives warpStarCapacity |
| `voiceTriggerFlash_` | `PluginProcessor.h:236` | Pattern for atomic read in timerCallback |

## New members (JoystickPad)

```cpp
struct WarpStar { float angle, dist, speed; };
std::vector<WarpStar> warpStars_;     // capacity = population * 2, max 128
float warpRamp_ = 0.0f;               // 0..1, raw linear ramp
// warpT = smoothstep(warpRamp_) computed at draw time
```

## Scope guardrail

- No changes to `LooperEngine`, `PluginProcessor` audio path, or APVTS parameters
- No new UI controls — effect is fully automatic, triggered by looper state
- `StarDot` struct is NOT changed — warp behaviour is computed at draw time from
  existing fields; `r` is the freeze discriminator (`r > 1.2f` = freeze)
- Phase 43 (resizable UI) handles coordinate scaling — use raw pixel coordinates here
- Cursor, crosshair, and all UI panels render on top; warp strictly within
  `JoystickPad` bounds (`clipToFit` already enforced by existing paint structure)
