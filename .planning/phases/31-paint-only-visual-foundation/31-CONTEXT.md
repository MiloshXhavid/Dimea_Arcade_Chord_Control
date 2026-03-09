---
phase: 31
title: Paint-Only Visual Foundation
status: context-complete
discussed: 2026-03-04
---

# Phase 31 Context: Paint-Only Visual Foundation

## Phase Goal

Establish the joystick pad's visual identity as a realistic deep-space scene — milky way band, dense starfield, heatmap overlay, semitone grid, and a beat-synced cursor glow — implemented as pure paint operations with zero per-frame allocation where possible.

**Scope note:** This is a re-implementation of the original Phase 31 (previously executed then reset with phases 32-34). The scope has been updated based on user discussion. No audio/MIDI functions are changed — all new knob interactions are visual-only read-only uses of existing APVTS parameters.

---

## Decision Log

### 1. Background: Milky Way Band

**Decision:** A realistic milky way is the primary background of the joystick pad.

- **Technique:** Procedurally generated `juce::Image` baked once in `JoystickPad::resized()`, never rebuilt in `paint()`.
- **Band angle:** Diagonal top-left → bottom-right (~-30° from horizontal).
- **Color tone:** Neutral white/grey realistic — cool white core with slight blue-grey fringe. Not stylized.
- **Band construction approach:** Multi-layer Gaussian blur simulation:
  - Wide soft glow (large sigma, very low alpha) as outer nebula haze
  - Medium layer for the main band body
  - Narrow bright core (high brightness, tight width)
  - All baked into the image via pixel-by-pixel computation using perpendicular distance from the band center line
- **Band prominence:** Moderate — clearly visible as a milky way but the heatmap gradient (drawn on top) dominates the overall color.
- **Band brightness at runtime:** Scales with `randomDensity` parameter (1–8 hits/bar):
  - This is a **visual-only read** — no audio/MIDI behavior changes
  - Density=1 → band drawn at ~15% alpha (barely visible)
  - Density=8 → band drawn at ~100% alpha (full brightness)
  - Implementation: `g.setOpacity(bandAlpha)` before `g.drawImage(milkyWayCache_)` in `paint()`

### 2. Starfield

**Decision:** Dense starfield of hundreds of tiny stars, pre-generated in `resized()`, rendered dynamically in `paint()`.

- **Count:** 300 stars total pre-generated, deterministic fixed seed.
- **Star properties baked at generate time:** position (normalized 0..1), radius (0.4–2.5px), color (mostly white, ~15% blue-tinted, ~8% warm yellow), brightness.
- **Visible count at runtime:** Scales linearly with `randomPopulation` parameter (1–64):
  - Population=1 → 0 stars visible (or ~1–2)
  - Population=32 (default) → ~150 stars (~50%)
  - Population=64 → all 300 stars visible
  - This is a **visual-only read** — no audio/MIDI behavior changes
  - Implementation: in `paint()`, draw only `floor(300 * (pop-1)/63.0f)` stars from the pre-sorted vector
- **Sort order:** Stars sorted by brightness descending at generate time, so brightest stars appear first as Population increases.

### 3. Heatmap Gradient

**Decision:** Keep the blue→magenta radial gradient from the original Phase 31, drawn on top of the milky way.

- Baked as `juce::Image` in `resized()`.
- Drawn inside `ScopedSaveState` with inscribed circle clip.
- Drawn AFTER the milky way band, so the heatmap tints and partially obscures the space background.
- The milky way remains visible through the heatmap because the heatmap is semi-transparent (same as original implementation).

### 4. Semitone Grid

**Decision:** Grid lines represent semitone positions on each axis, driven by existing APVTS parameters.

- **X axis (horizontal grid lines):** `joystickYAtten` parameter (0–127, default 24) = semitone range of Y axis (root/third voices). One horizontal line per semitone.
- **Y axis (vertical grid lines):** `joystickXAtten` parameter (0–127, default 24) = semitone range of X axis (fifth/tension voices). One vertical line per semitone.
- **Display cap:** If semitone range > 48, merge lines to avoid rendering more than 48 per axis (every 2nd semitone shown). If > 96, show every 4th. Prevents visual noise at high attenuation values.
- **In-scale vs out-of-scale differentiation:**
  - In-scale semitones (matching current scale + globalTranspose offset): alpha 0.28f, slightly wider stroke (1.2px)
  - Out-of-scale semitones: alpha 0.10f, thin stroke (0.8px)
  - Scale is read from APVTS `scaleNote0..11` + `scalePreset` + `globalTranspose` — visual-only read
- **Line style:**
  - Horizontal lines (Y-axis semitones): dashed `{3.0f, 3.0f}` via `g.drawDashedLine()`
  - Vertical lines (X-axis semitones): solid via `g.drawLine()`
- **Center-weighted brightness ramp:** Lines fade toward center, peak at edges (preserves cursor visibility at center).

### 5. Cursor Visuals

**Decision:** Dark halo retained; cyan glow ring replaced with beat-synced breathing animation.

- **Dark halo:** `0xff05050f` at 0.65f alpha, radius = dotR + 8px. Unchanged from original.
- **Glow ring:** Cyan (`Clr::highlight`) with animated alpha and radius:
  - **Breathing:** Alpha oscillates between 0.15f and 0.45f, radius between dotR+4 and dotR+12
  - **Sync:** Beat-synced — one full inhale/exhale per quarter note (uses `beatPulse_` atomic already present in the codebase for the beat pulse dot feature)
  - **Gate-open behavior:** When any voice gate is open → breathing pauses, glow stays at full brightness (alpha 0.45f, outer radius). Resumes breathing smoothly on gate release.
  - **Implementation note:** `beatPulse_` decays in `timerCallback()`. For the breathing pattern, use a separate `glowPhase_` float (0..1, advances per timer tick) that resets to 0 on beat. Paused by checking `anyGateOpen_` flag (already maintained for particle system).

### 6. Original Phase 31 Elements — Status

| Element | Status |
|---------|--------|
| Near-black `0xff05050f` background | ✅ Keep |
| 70-star deterministic starfield (fixed seed, StarDot struct) | ❌ Removed — replaced by new 300-star system |
| Blue→magenta radial heatmap (`heatmapCache_`) | ✅ Keep |
| Axis-differentiated grid (horizontal dashed, vertical solid) | ✅ Keep, upgraded to semitone-driven |
| Dark halo behind cursor | ✅ Keep |
| Static cyan glow ring | ❌ Replaced by beat-synced breathing glow |
| Border alpha 0.30f | ✅ Keep |

---

## New Private Members Required

In `JoystickPad` (PluginEditor.h):

```cpp
// Milky way
juce::Image milkyWayCache_;     // baked in resized()

// Starfield (replaces original starfield_)
struct StarDot { float x, y, r; juce::Colour c; };
std::vector<StarDot> starfield_; // 300 pre-generated stars, sorted brightness desc

// Cursor breathing
float glowPhase_ = 0.0f;        // 0..1, advances with beat
```

Remove: `prevBoundsW_`, `prevBoundsH_` if they were only used for starfield regeneration guard.

---

## APVTS Parameters Read (Visual Only)

| Parameter ID | UI Label | Used For | Range |
|---|---|---|---|
| `randomDensity` | Random Density | Milky way band alpha | 1–8 |
| `randomPopulation` | Population | Star count (0–300 linear) | 1–64 |
| `joystickXAtten` | Semitone X | Vertical grid line count | 0–127 |
| `joystickYAtten` | Semitone Y | Horizontal grid line count | 0–127 |
| `scaleNote0..11` | Scale notes | In-scale grid highlighting | bool |
| `scalePreset` | Scale Preset | In-scale grid highlighting | 0–19 |
| `globalTranspose` | Key | Grid scale offset | 0–11 |

All reads are one-way (`apvts.getRawParameterValue(...)->load()`). No APVTS listeners or callbacks added.

---

## Paint Layer Order (bottom to top)

1. Near-black fill (`0xff05050f`)
2. Milky way band image (alpha = f(randomDensity))
3. Starfield dots (count = f(randomPopulation))
4. Heatmap radial gradient image (circle-clipped)
5. Grid lines (semitone-driven, in/out-of-scale differentiated)
6. Particle system (existing — unchanged)
7. Cursor: dark halo → glow ring (breathing/paused) → dot fill → crosshair ticks
8. Border stroke (alpha 0.30f)

---

## Out of Scope (Deferred)

- Original 70-star deterministic starfield with fixed seed — deferred to later phase
- Any changes to audio/MIDI behavior
- Any new APVTS parameters
- Spring-damper animation (Phase 32)
- Angle indicator (Phase 32)

---

## Requirements Covered

| Req ID | Description |
|--------|-------------|
| VIS-03 | Joystick pad background has radial gradient cached in resized() |
| VIS-07 | Joystick pad has space/arcade visual theme — space background, glow, visual character |
| VIS-12 | Horizontal grid lines brighter than vertical (X-axis dominant feel) |

---

_Context by: Claude (gsd-discuss-phase) — 2026-03-04_
