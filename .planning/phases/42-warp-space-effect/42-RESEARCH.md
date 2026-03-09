# Phase 42: Warp Space Effect — Research

**Researched:** 2026-03-09
**Domain:** JUCE paint-only visual effect — procedural particle animation in C++17
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**A. Star motion model during warp**

- Existing ambient starfield (250 stars) persists during warp — not replaced.
  - Large stars freeze: stars with `r > 1.2f` stop drifting entirely during warp
    (they read as distant background nebula — stationary).
  - Small stars continue drifting: stars with `r <= 1.2f` keep their lateral
    drift velocities unchanged — they read as nearby floating dust.
  - Both groups have their visual appearance (dot to streak) driven by `warpT`
    based on their distance from the vanishing point.
- Dedicated warp star pool added alongside the ambient field:
  - Capacity: `warpStarCapacity = (int)(population * 2)` — scales 2 to 128 with
    the population knob (pop=1 → barely visible, pop=64 → dense storm).
  - Separate `std::vector<WarpStar> warpStars_` in `JoystickPad`.
- WarpStar struct: `{ float angle; float dist; float speed; }`
- Spawn at vanishing point with `dist ≈ 0`, random `angle`, near-zero initial speed.
- Acceleration: `speed += 0.0008f * warpT` each frame.
- Respawn when `dist > 1.2f` (exited pad bounds), reset to `dist = 0`, new random angle.
- In-flight trajectories are straight — no mid-flight bending.

**B. Warp ramp**

- Trigger (ramp up): `proc_.looperIsPlaying()` false → true.
- Trigger (ramp down): `proc_.looperIsPlaying()` true → false.
- Duration: 4000ms = 120 ticks at 30 Hz.
- `warpRamp_` updated in `timerCallback()`:
  ```cpp
  const bool isPlaying = proc_.looperIsPlaying();
  const float step = 1.0f / (4.0f * 30.0f);   // ≈ 0.00833/tick
  warpRamp_ = isPlaying
      ? juce::jmin(1.0f, warpRamp_ + step)
      : juce::jmax(0.0f, warpRamp_ - step);
  ```
- Easing: `warpT = warpRamp_ * warpRamp_ * (3.0f - 2.0f * warpRamp_)` (smoothstep)
  applied at draw time.

**C. Vanishing point (cursor tracking)**

- Radial origin tracks `displayCx_/displayCy_` (spring-smoothed cursor pixel position).
- Vanishing point computed as normalized coordinates at draw time:
  ```cpp
  const float vpX = displayCx_ / getWidth();
  const float vpY = displayCy_ / getHeight();
  ```
- Warp star pixel position from stored angle/dist:
  ```cpp
  const float px  = vpX + std::sin(ws.angle) * ws.dist;
  const float py  = vpY - std::cos(ws.angle) * ws.dist;
  ```

**D. Streak visual: shape and color**

- Shape (warp stars): `juce::Line` from tail to head.
  - Head (bright, outer): `(px * w, py * h)`
  - Tail (dim, inner): `(px - sin(angle) * streakLen, py + cos(angle) * streakLen)` * dimensions
  - `streakLen = ws.speed * 400.0f * warpT`
  - `strokeWidth ≈ 1.0f` for distant stars, `1.5f` for fastest.
- Color (warp stars): gradient along streak via `g.setGradientFill`.
  - Head color based on dist from vanishing point at draw time.
  - Near VP (dist < 0.3): blue-white `#aaddff`
  - Far VP (dist > 0.7): barely warm white `#fff8f0`
  - Blend zone (0.3-0.7): linear interpolation
  - Tail color: head color darkened to ~15% alpha.
- Ambient star elongation during warp:
  - `ambientStreakLen = s.r * warpT * 6.0f` in the radial direction toward VP.
  - Color tint: dist < 0.3 of VP → 30% shift toward `#aaddff`; peripheral → 20% toward `#fff8f0`.
  - `s.c.interpolatedWith(targetClr, warpT * blend)`.

**E. Population knob interaction**

- Ambient star count: `visCount = floor(starfield_.size() * (pop - 1) / 63)` — unchanged.
- Warp star pool capacity: `warpStarCapacity = (int)(population * 2.0f)`.
- At minimum population (pop=1): 2 warp stars — effectively no warp effect.

**Scope guardrail**

- No changes to `LooperEngine`, `PluginProcessor` audio path, or APVTS parameters.
- No new UI controls — effect is fully automatic, triggered by looper state.
- `StarDot` struct is NOT changed.
- Phase 43 (resizable UI) handles coordinate scaling — use raw pixel coordinates here.
- Cursor, crosshair, and all UI panels render on top; warp strictly within `JoystickPad` bounds.

### Deferred Ideas (OUT OF SCOPE)

- No explicit deferred items in CONTEXT.md; scope guardrail effectively defers:
  - Any APVTS parameter changes
  - Any new UI controls
  - Coordinate scaling / Phase 43 concerns
</user_constraints>

---

## Summary

Phase 42 is a pure paint-only visual effect in `JoystickPad`. There are no audio, MIDI, or APVTS changes. All work is confined to `PluginEditor.h` and `PluginEditor.cpp` — specifically the `JoystickPad` class. The effect adds two new members (`warpStars_` vector and `warpRamp_` float), one new update block in `timerCallback()`, and one new paint pass in `paint()`.

The existing paint layer structure (Layers 1 through 7+) is well-understood from code inspection. Warp stars belong between Layers 3 (ambient starfield) and 4 (range circle), so they appear behind the heatmap, grid, particles, and cursor. Ambient star modification during warp extends the existing Layer 3 paint block without restructuring it.

The key technical decisions are already fully specified in CONTEXT.md and verified against the actual source code. This research focuses on confirming implementation correctness, identifying precise insertion points, and cataloguing JUCE API details needed for the warp draw pass.

**Primary recommendation:** Implement in two plans — Plan 01: warp ramp + warp star pool (logic only, no draw), Plan 02: full draw pass for warp stars + ambient star elongation.

---

## Standard Stack

### Core (already in project)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.4 | Graphics, component system | Project dependency |
| C++17 | - | Language | Project standard |
| SDL2 | 2.30.9 | (Gamepad only — not used here) | Project dependency |

No new dependencies required for this phase.

**JUCE Graphics APIs used in this phase:**

| API | Purpose | Notes |
|-----|---------|-------|
| `g.setGradientFill(ColourGradient)` | Streak gradient | Two-point linear gradient along streak |
| `g.strokePath(path, strokeType)` | Draw streaks | Alternative to `g.drawLine` for anti-aliasing |
| `g.drawLine(Line<float>, float)` | Draw streaks | Simpler; CONTEXT uses this variant |
| `juce::ColourGradient(c1, x1, y1, c2, x2, y2, isRadial)` | Tail-to-head gradient | `isRadial=false` for linear |
| `juce::Colour::interpolatedWith(other, t)` | Color blending | Used for Doppler color blend + tint |
| `juce::jmin / juce::jmax` | Clamping warpRamp_ | Standard JUCE clamp |
| `juce::Graphics::ScopedSaveState` | Isolate gradient fill state | Prevents fill leaking to next draw op |

---

## Architecture Patterns

### Existing Paint Layer Order (verified from source)

```
Layer 1:   Near-black fill (0xff05050f)
Layer 1.5: Space background fly-through (bgScrollY_, spaceBgBaked_)
Layer 1.6: Mid-rotation vignette deepening (bgTransPulse)
Layer 2:   Milky way band (alpha driven by randomProbability)
Layer 3:   Ambient starfield dots (count driven by randomPopulation)
[INSERT]   Warp stars draw pass — NEW in Phase 42
Layer 4:   Joystick range circle (cyan glow, BPM-breathing)
Layer 4b:  Perimeter arc (raw joystick polar angle)
Layer 4c:  Note-label compass (Root/Third/Fifth/Tension)
Layer 5:   Semitone grid (joystickXAtten/YAtten, in-scale differentiation)
Layer 6:   Particles (clipped to pad bounds)
Layer 7a:  Static centre reference dot
Layer 7b:  Cursor dark halo
Layer 7c:  Cursor dot + outline + ticks
Layer 7d:  Pitch axis crosshair (Phase 40)
Layer 8:   Border stroke (alpha 0.30f)  [not yet confirmed in code — likely after 7c]
```

The warp star pass belongs immediately after Layer 3 (ambient starfield). This ensures:
- Warp streaks draw on top of ambient stars (correct depth: warp stars are closer)
- Warp streaks draw below the heatmap (maintaining the space-behind-grid feel)
- The heatmap circle-clip does NOT clip warp stars (they fill the full pad)

### Recommended Project Structure

No new files needed. All changes in:
```
Source/
├── PluginEditor.h         — JoystickPad: 2 new members + WarpStar struct
└── PluginEditor.cpp       — timerCallback: warp ramp + star update
                           — resized(): warpStars_ init
                           — paint(): warp star draw pass + ambient elongation
```

### Pattern 1: Warp Ramp Update in timerCallback

Insert after the INV rotation block (around line 1361) and before `repaint()`:

```cpp
// ── Warp ramp (Phase 42) ─────────────────────────────────────────────────
{
    const bool isPlaying = proc_.looperIsPlaying();
    const float step = 1.0f / (4.0f * 30.0f);   // 4 s at 30 Hz
    warpRamp_ = isPlaying
        ? juce::jmin(1.0f, warpRamp_ + step)
        : juce::jmax(0.0f, warpRamp_ - step);
}

// ── Animate warp stars (Phase 42) ────────────────────────────────────────
if (warpRamp_ > 0.0f)
{
    const float warpT = warpRamp_ * warpRamp_ * (3.0f - 2.0f * warpRamp_);  // smoothstep
    const float vpX = displayCx_ / (float)getWidth();
    const float vpY = displayCy_ / (float)getHeight();

    const float pop = proc_.apvts.getRawParameterValue("randomPopulation")->load();
    const int capacity = juce::jmin(128, (int)(pop * 2.0f));

    // Resize pool without allocation during playback: only grow, never shrink mid-session
    if ((int)warpStars_.size() < capacity)
        warpStars_.resize(capacity, WarpStar{});

    static juce::Random warpRng(0xABCD1234LL);

    for (int i = 0; i < capacity; ++i)
    {
        auto& ws = warpStars_[i];
        ws.speed += 0.0008f * warpT;
        ws.dist  += ws.speed;

        if (ws.dist > 1.2f)
        {
            ws.dist  = 0.0f;
            ws.angle = warpRng.nextFloat() * juce::MathConstants<float>::twoPi;
            ws.speed = 0.0f;
        }
    }
}
```

### Pattern 2: Warp Star Draw Pass in paint()

Insert immediately after the ambient starfield loop (after line 1570, before Layer 4 comment):

```cpp
// ── Warp star draw pass (Phase 42) ───────────────────────────────────────
if (warpRamp_ > 0.0f && !warpStars_.empty())
{
    const float warpT  = warpRamp_ * warpRamp_ * (3.0f - 2.0f * warpRamp_);
    const float vpX    = displayCx_ / b.getWidth();
    const float vpY    = displayCy_ / b.getHeight();
    const float pop    = proc_.apvts.getRawParameterValue("randomPopulation")->load();
    const int   capaci = juce::jmin(128, (int)(pop * 2.0f));
    const int   drawN  = juce::jmin(capaci, (int)warpStars_.size());

    const juce::Colour nearClr = juce::Colour(0xFFAADDFF);   // blue-white
    const juce::Colour farClr  = juce::Colour(0xFFFFF8F0);   // warm white

    for (int i = 0; i < drawN; ++i)
    {
        const auto& ws = warpStars_[i];
        if (ws.dist < 0.001f) continue;  // not yet spawned

        const float px  = vpX + std::sin(ws.angle) * ws.dist;
        const float py  = vpY - std::cos(ws.angle) * ws.dist;

        // Skip if outside normalized bounds (clipping guard)
        if (px < -0.05f || px > 1.05f || py < -0.05f || py > 1.05f) continue;

        // Doppler color: dist-based blend
        const float distBlend = juce::jlimit(0.0f, 1.0f, (ws.dist - 0.3f) / 0.4f);
        const juce::Colour headClr = nearClr.interpolatedWith(farClr, distBlend);

        const float streakLen = ws.speed * 400.0f * warpT;
        const float strokeW   = (ws.dist > 0.6f) ? 1.5f : 1.0f;

        // Head pixel position
        const float hx = px * b.getWidth();
        const float hy = py * b.getHeight();
        // Tail pixel position (inner end, toward VP)
        const float tx = hx - std::sin(ws.angle) * streakLen;
        const float ty = hy + std::cos(ws.angle) * streakLen;

        if (streakLen < 0.5f)
        {
            // Very short: draw as dot
            g.setColour(headClr.withAlpha(warpT * 0.7f));
            g.fillEllipse(hx - 0.8f, hy - 0.8f, 1.6f, 1.6f);
        }
        else
        {
            // Draw as gradient line: tail dim → head bright
            juce::Graphics::ScopedSaveState ss(g);
            juce::ColourGradient grad(
                headClr.withAlpha(0.15f), tx, ty,
                headClr,                  hx, hy,
                false);  // linear, not radial
            g.setGradientFill(grad);
            g.drawLine(tx, ty, hx, hy, strokeW);
        }
    }
}
```

### Pattern 3: Ambient Star Elongation During Warp

Modify the existing Layer 3 starfield loop (around lines 1556-1570):

```cpp
// ── Layer 3: Starfield (count driven by randomPopulation 1-64) ───────────
if (!starfield_.empty())
{
    const float pop    = proc_.apvts.getRawParameterValue("randomPopulation")->load();
    const int visCount = (int)std::floor((float)starfield_.size() * (pop - 1.0f) / 63.0f);
    const int count    = juce::jmin(visCount, (int)starfield_.size());

    // Warp elongation params (computed once, used per star)
    const float warpT = (warpRamp_ > 0.0f)
        ? (warpRamp_ * warpRamp_ * (3.0f - 2.0f * warpRamp_))
        : 0.0f;
    const float vpX = (warpT > 0.0f) ? displayCx_ / b.getWidth()  : 0.5f;
    const float vpY = (warpT > 0.0f) ? displayCy_ / b.getHeight() : 0.5f;
    const juce::Colour nearTint = juce::Colour(0xFFAADDFF);
    const juce::Colour farTint  = juce::Colour(0xFFFFF8F0);

    for (int i = 0; i < count; ++i)
    {
        const auto& s = starfield_[i];

        if (warpT <= 0.0f)
        {
            // Original dot draw
            g.setColour(s.c);
            g.fillEllipse(s.x * b.getWidth()  - s.r,
                          s.y * b.getHeight() - s.r,
                          s.r * 2.0f, s.r * 2.0f);
        }
        else
        {
            // Elongated dot during warp
            const float ambientStreakLen = s.r * warpT * 6.0f;
            const float sx = s.x * b.getWidth();
            const float sy = s.y * b.getHeight();
            const float dx = sx - vpX * b.getWidth();
            const float dy = sy - vpY * b.getHeight();
            const float distToVp = std::sqrt(dx * dx + dy * dy)
                                   / juce::jmin(b.getWidth(), b.getHeight());

            // Radial direction from VP toward star (outward)
            const float mag = std::sqrt(dx * dx + dy * dy);
            const float dirX = (mag > 0.5f) ? dx / mag : 0.0f;
            const float dirY = (mag > 0.5f) ? dy / mag : 0.0f;

            // Color tint
            const float tintBlend = juce::jlimit(0.0f, 1.0f, (distToVp - 0.3f) / 0.4f);
            const float tintStr   = (tintBlend < 0.5f) ? (warpT * 0.30f) : (warpT * 0.20f);
            const juce::Colour tgt = nearTint.interpolatedWith(farTint, tintBlend);
            const juce::Colour dc  = s.c.interpolatedWith(tgt, tintStr);

            if (ambientStreakLen < 1.0f)
            {
                g.setColour(dc);
                g.fillEllipse(sx - s.r, sy - s.r, s.r * 2.0f, s.r * 2.0f);
            }
            else
            {
                const float tailX = sx - dirX * ambientStreakLen;
                const float tailY = sy - dirY * ambientStreakLen;
                juce::Graphics::ScopedSaveState ss(g);
                juce::ColourGradient grad(
                    dc.withAlpha(0.15f), tailX, tailY,
                    dc,                  sx,    sy,
                    false);
                g.setGradientFill(grad);
                g.drawLine(tailX, tailY, sx, sy, s.r * 1.5f);
            }
        }
    }
}
```

### Pattern 4: Large Star Freeze During Warp

In `timerCallback()`, modify the animate starfield block (around lines 1284-1294):

```cpp
// ── Animate starfield positions ────────────────────────────────────────────
for (auto& s : starfield_)
{
    // Large stars (r > 1.2f) freeze during warp — they read as distant nebula
    if (warpRamp_ > 0.0f && s.r > 1.2f)
        continue;

    s.x += s.vx;
    s.y += s.vy;
    if (s.x < 0.0f) s.x += 1.0f;
    if (s.x > 1.0f) s.x -= 1.0f;
    if (s.y < 0.0f) s.y += 1.0f;
    if (s.y > 1.0f) s.y -= 1.0f;
}
```

### Pattern 5: warpStars_ init in resized()

In `JoystickPad::resized()`, after the starfield generation block (around line 1105):

```cpp
// ── Init warp star pool (Phase 42) ────────────────────────────────────────
{
    const float pop = proc_.apvts.getRawParameterValue("randomPopulation")->load();
    const int capacity = juce::jmin(128, (int)(pop * 2.0f));
    warpStars_.assign(capacity, WarpStar{});
    // All fields zero-initialized: dist=0, angle=0, speed=0
    // Scatter angles so they don't all appear at angle=0 on first frame
    juce::Random initRng(0xWARPINIT12345678LL);
    for (auto& ws : warpStars_)
        ws.angle = initRng.nextFloat() * juce::MathConstants<float>::twoPi;
}
```

### Anti-Patterns to Avoid

- **Allocating `warpStars_` on every timerCallback tick:** Resize is a one-time grow operation; cap at draw time with `drawN = jmin(capacity, (int)warpStars_.size())`.
- **Computing `warpT` three times independently:** Compute once in timerCallback, cache in a member, OR recompute from `warpRamp_` in paint (the current pattern — identical formula, cheap). The CONTEXT uses the inline recompute pattern everywhere; stay consistent.
- **Writing warpT as a member:** CONTEXT spec keeps only `warpRamp_` as member; `warpT` is always derived locally. Consistent with project style (no intermediate cached derivatives).
- **Drawing warp stars outside `JoystickPad::paint()` clip region:** `getLocalBounds()` clip is implicitly enforced by JUCE component paint — no extra `clipToFit` needed. But bounds-check with `px < -0.05f || px > 1.05f` before drawing to skip invisible stars cheaply.
- **Using `g.setGradientFill` without `ScopedSaveState`:** Gradient fill state leaks to subsequent draw calls if not scoped. Every gradient block must be wrapped.
- **Calling `getRawParameterValue("randomPopulation")->load()` multiple times in one paint():** Already called for ambient starfield; cache in a local `float pop` at top of the starfield block and reuse.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Smoothstep easing | Custom polynomial | `warpRamp_ * warpRamp_ * (3.0f - 2.0f * warpRamp_)` | 3 ops, compiler-inlined, already chosen by CONTEXT |
| Color interpolation | Manual R/G/B lerp | `juce::Colour::interpolatedWith(other, t)` | JUCE built-in, premultiplied alpha-safe |
| Clamping | `std::max/min` | `juce::jmin / juce::jmax` | Consistent with rest of codebase |
| Random angles | `std::random` engine | `juce::Random` with fixed seed | Matches existing starfield pattern |
| Gradient line | Multi-segment polyline | `juce::ColourGradient` + `g.setGradientFill` + `g.drawLine` | JUCE hardware-accelerated where available |

---

## Common Pitfalls

### Pitfall 1: warpT Computed from Stale warpRamp_

**What goes wrong:** `paint()` recomputes `warpT = smoothstep(warpRamp_)`. If `warpRamp_` is read in `paint()` while `timerCallback()` is simultaneously writing it, the value is torn on 32-bit platforms.

**Why it happens:** `warpRamp_` is a plain `float` member, not atomic. JUCE UI components: `paint()` runs on the message thread; `timerCallback()` also runs on the message thread (JUCE Timer is message thread only). Race is impossible — both are same thread.

**How to avoid:** No action needed. JUCE `Timer::timerCallback()` and `Component::paint()` are both message thread operations. No data race.

**Warning signs:** If you ever move warp update to an audio thread or background thread, you must add synchronization.

### Pitfall 2: ScopedSaveState Missing Around Gradient Fill

**What goes wrong:** `g.setGradientFill(...)` persists as the current fill until the next `g.setColour()`. Any subsequent `g.fillEllipse()` or `g.fillRect()` accidentally uses the gradient.

**Why it happens:** JUCE Graphics state is mutable and not auto-restored.

**How to avoid:** Always wrap gradient draw blocks with `juce::Graphics::ScopedSaveState ss(g);`. Every existing paint block in this codebase already does this — follow the same pattern.

**Warning signs:** Nearby components (cursor dot, particles) rendering with unexpected color gradients.

### Pitfall 3: Warp Stars with dist=0 Drawing at Vanishing Point as Dot Cluster

**What goes wrong:** All freshly spawned (or zero-initialized) warp stars have `dist=0`, placing them at the exact VP pixel. They all draw as 0-length lines or invisible dots, but could cluster visibly on first activation.

**Why it happens:** `warpStars_.assign(capacity, WarpStar{})` zero-initializes all fields.

**How to avoid:** `continue` when `ws.dist < 0.001f` in both the timerCallback update and the paint pass. Include a per-star `if (ws.dist < 0.001f) continue;` guard. Stars near dist=0 do not draw — they appear naturally after a few ticks.

**Warning signs:** Cluster of faint dots visible at cursor position on looper start.

### Pitfall 4: Streak Length Scaling Mismatched with Pad Width

**What goes wrong:** `streakLen = ws.speed * 400.0f * warpT` assumes a ~400px pad. On a different window size, streaks are proportionally wrong.

**Why it happens:** Phase 43 (resizable UI) is deferred. Raw pixel coordinates are used per CONTEXT scope guardrail.

**How to avoid:** Document the assumption. If Phase 43 changes pad dimensions, the `400.0f` constant will need replacing with `b.getWidth()` or a similar expression. For Phase 42 this is correct.

**Warning signs:** Streaks visually too short or long after Phase 43 is applied.

### Pitfall 5: warpStars_ Capacity Mismatch Between timerCallback and paint()

**What goes wrong:** timerCallback updates `capacity` warp stars; paint() draws `drawN = jmin(capacity, warpStars_.size())`. If `capacity` grows but `warpStars_` was not resized, new indices are out of range.

**Why it happens:** If timerCallback skips the resize (e.g., `if (warpRamp_ == 0.0f)`) but paint() tries to draw with the current `capacity` formula.

**How to avoid:** Both timerCallback and paint() must compute `capacity = jmin(128, (int)(pop * 2.0f))` independently and cap against `warpStars_.size()` using `jmin`. Never index `warpStars_[i]` without a bounds check. The `drawN = jmin(capacity, (int)warpStars_.size())` pattern in paint() is the correct guard.

### Pitfall 6: draw call inside paint() calling getRawParameterValue for same param twice

**What goes wrong:** `randomPopulation` is already read for the ambient starfield count. Reading it again for warp star capacity is redundant but harmless (atomic load is cheap). However, for readability and future Phase 43 changes, extract to a local once.

**How to avoid:** Read `pop` once near the top of the ambient starfield block and reuse it in both the ambient count and the warp capacity expressions. This is already possible since both are in the same `paint()` call.

---

## Code Examples

Verified patterns from the existing codebase:

### Member Declaration in JoystickPad (PluginEditor.h, after line 184)

```cpp
// Phase 42: Warp space effect
struct WarpStar { float angle = 0.0f, dist = 0.0f, speed = 0.0f; };
std::vector<WarpStar> warpStars_;
float warpRamp_ = 0.0f;   // 0..1 linear; warpT = smoothstep(warpRamp_) at use sites
```

### Existing gradient fill pattern in JoystickPad::paint() (verified at line 1534)

```cpp
// Existing pattern used for vignette:
juce::Graphics::ScopedSaveState vsave(g);
juce::ColourGradient vig(
    juce::Colour(0x00000000),
    b.getCentreX(), b.getCentreY(),
    juce::Colour(0xFF000000).withAlpha(bgTransPulse * 0.22f),
    b.getCentreX() + gradR, b.getCentreY(),
    true);   // isRadial
g.setGradientFill(vig);
g.fillRect(b);
```

For warp streak (linear, not radial — `false` last arg):

```cpp
juce::Graphics::ScopedSaveState ss(g);
juce::ColourGradient grad(
    headClr.withAlpha(0.15f), tx, ty,
    headClr,                  hx, hy,
    false);
g.setGradientFill(grad);
g.drawLine(tx, ty, hx, hy, strokeWidth);
```

### juce::Random in paint context (verified at lines 1374, 1398)

```cpp
// Existing pattern for particle spawn:
static juce::Random rng;  // static local: one instance, reused across calls

// For warp stars: use a fixed-seed instance so behavior is deterministic:
static juce::Random warpRng(0xABCD1234LL);
```

Note: warp star angle randomization happens in timerCallback on respawn. A `static juce::Random` works because timerCallback is always message thread.

### Colour::interpolatedWith (JUCE built-in)

```cpp
// JUCE API:
juce::Colour result = colorA.interpolatedWith(colorB, t);  // t: 0=A, 1=B
// Example from CONTEXT:
s.c.interpolatedWith(targetClr, warpT * blend)
```

### looperIsPlaying() read pattern (verified at lines 1194, 1435)

```cpp
// Already in timerCallback (line 1194):
const bool looperJoyMode = proc_.looperIsPlaying() && proc_.looperHasJoystickContent();
// Phase 42 follows the same pattern:
const bool isPlaying = proc_.looperIsPlaying();
```

---

## State of the Art

| Old Approach | Current Approach | Notes |
|--------------|------------------|-------|
| All stars drift freely | Large stars (r > 1.2f) freeze during warp | Phase 42 addition |
| Ambient stars draw as dots always | Ambient stars elongate (streak) when warpT > 0 | Phase 42 addition |
| Single starfield `starfield_` | Dual system: `starfield_` + `warpStars_` | Phase 42 addition |

---

## Open Questions

1. **Hex literal `0xWARPINIT12345678LL` in resized() is not valid hex**
   - What we know: The `W` prefix is not a valid hex digit. This was written as pseudocode in this research.
   - What's unclear: nothing — it is a clear typo in research notes.
   - Recommendation: Use `0xBEEF42CAFE0000LL` or any valid 64-bit hex literal as the init seed.

2. **Whether `g.drawLine(tx, ty, hx, hy, strokeWidth)` is the correct JUCE overload**
   - What we know: JUCE provides `Graphics::drawLine(float x1, float y1, float x2, float y2, float lineThickness)` as a confirmed API. This is used extensively in the existing codebase (e.g., tick marks at lines 1801-1821).
   - Confidence: HIGH.

3. **Performance: ColourGradient per warp star (up to 128 gradient objects per frame)**
   - What we know: At 128 warp stars, each with a `ScopedSaveState` + `ColourGradient` construction per frame at 30 Hz, this is 3840 gradient objects/second. On desktop JUCE (software renderer), ColourGradient construction is lightweight. The existing vignette uses one per frame; this scales to 128.
   - Risk: On very slow machines the fill could be expensive.
   - Recommendation: Stars with `streakLen < 0.5f` fall through to `fillEllipse` (no gradient). At low warpT the majority of stars are near dist=0 with tiny streakLen, so the gradient path is only active for a fraction of the pool. Performance should be acceptable. If needed in Phase 43 or later, can batch to a single Path per color bucket.

---

## Validation Architecture

`workflow.nyquist_validation` is absent from `.planning/config.json` — treated as enabled.

### Test Framework

| Property | Value |
|----------|-------|
| Framework | CTest (CMake) + custom ChordJoystickTests (catch2-style) |
| Config file | `build/CTestTestfile.cmake` |
| Quick run command | `cmake --build "C:/Users/Milosh Xhavid/get-shit-done/build" --config Release --target ChordJoystickTests` |
| Full suite command | `ctest --test-dir "C:/Users/Milosh Xhavid/get-shit-done/build" -C Release` |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Notes |
|--------|----------|-----------|-------|
| (none specified) | Warp ramp increments at correct rate (120 ticks over 4 s at 30 Hz) | manual-only | JoystickPad is a JUCE Component — no unit test harness available without full JUCE processor instantiation. Visual inspection only. |
| (none specified) | Stars spawn at VP, accelerate, respawn on dist > 1.2f | manual-only | Requires running plugin. |
| (none specified) | warpT = smoothstep(warpRamp_) correct | manual-only | Trivial math formula, risk is low. |
| (none specified) | Looper stop ramps down gracefully | manual-only | Load plugin in DAW, verify in real-time. |

**Note:** This phase is paint-only. All behaviors are visual. There are no logic functions extractable to unit tests without JUCE Component infrastructure. Validation is UAT-based: run in DAW, press looper record/play, observe effect.

### Wave 0 Gaps

None — no new test files required. Existing test infrastructure has no coverage gap for paint-only visual changes.

---

## Sources

### Primary (HIGH confidence)

- Source code inspection: `Source/PluginEditor.cpp` lines 1117-1370 (timerCallback), 1491-1900+ (paint), 1057-1113 (resized)
- Source code inspection: `Source/PluginEditor.h` lines 120-185 (JoystickPad class)
- `.planning/phases/42-warp-space-effect/42-CONTEXT.md` — authoritative spec for all decisions
- `.planning/phases/31-paint-only-visual-foundation/31-CONTEXT.md` — paint layer order origin
- JUCE Graphics API: `g.drawLine`, `ColourGradient`, `ScopedSaveState` — confirmed via usage patterns in existing codebase
- `Source/PluginProcessor.h` lines 55-66 — `looperIsPlaying()` confirmed as `bool`, message-thread safe

### Secondary (MEDIUM confidence)

- JUCE 8.0.4 `juce::Graphics::drawLine(float x1, float y1, float x2, float y2, float lineThickness)` overload — confirmed by existing tick-mark draw calls at paint() lines 1801-1821
- `juce::Colour::interpolatedWith(other, t)` — confirmed by JUCE API design; not explicitly verified in this codebase but standard JUCE method

### Tertiary (LOW confidence)

- Performance estimate for 128 ColourGradient objects per frame — estimated from first principles, not benchmarked

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — project already uses JUCE; no new dependencies
- Architecture: HIGH — paint insertion points confirmed from line-level code inspection
- Pitfalls: HIGH — identified from direct code analysis of existing patterns
- Code examples: HIGH — all patterns derived from or consistent with existing codebase

**Research date:** 2026-03-09
**Valid until:** 2026-04-09 (stable JUCE 8.0.4 API; no external dependencies changing)
