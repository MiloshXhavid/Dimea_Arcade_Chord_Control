# Phase 43 — Resizable UI: Context

## Goal
Plugin window resizes proportionally from 0.75x to 1.0x (downscaling only) with locked aspect ratio — scale factor remembered per project instance across sessions.

---

## Decisions

### 1. Resize Interaction
- **OS corner drag only** — host exposes the resize grip; user drags the window corner
- No custom resize handle drawn in the UI
- No step presets or right-click menu for size

### 2. Resize Style
- **Free continuous drag** — no snapping to discrete steps
- Window tracks the drag position continuously within the allowed range

### 3. Scale Range & Aspect Ratio
- **Minimum: 0.75x** → 840×630 (floor raised from 0.5x for readability — pixel font stays ~7.5px)
- **Maximum: 1.0x** → 1120×840 (upscaling deferred — layout needs further work for >1x)
- **Default / base size: 1x** → 1120×840
- **Aspect ratio locked** at 1120:840 (4:3) via `ComponentBoundsConstrainer`
- **UAT finding (2026-03-10):** 2x scale broke layout; decision: cap max at 1x, ship downscaling only

### 4. Persistence
- Scale factor saved **per instance** inside `getStateInformation` / `setStateInformation`
- Stored as a ValueTree property on the APVTS state (not an APVTS parameter — no automation)
- On load, editor restores to saved scale before first paint

### 5. No MIDI Side Effects
- Resize must not trigger any MIDI output or APVTS parameter changes
- Scale factor is editor-only state; processor knows nothing about it

---

## Deferred Ideas (captured, out of scope for Phase 43)
- **Mini mode (Phase 44):** Single-click toggle collapses UI to joystick-only widget (~280×280), re-expand restores full view. Warp stars and starfield still run. All controls hidden. User requested this as a DAW corner visual FX use case.

---

## Code Context

### Current state
- `PluginEditor.cpp` line ~2380: `setSize(1120, 840)` — single fixed call, no resize logic
- No `setResizable`, no `ComponentBoundsConstrainer`, no `AffineTransform` anywhere
- `resized()` uses JUCE `Rectangle::removeFromLeft/Top` column-based layout — relative within columns but all pixel constants (margins, row heights) are hardcoded integers
- `paint()` uses absolute pixel draws (header 28px, margins 8px, row heights, font size 10px)
- State serialization: `getStateInformation` / `setStateInformation` in `PluginProcessor.cpp` — XML of APVTS ValueTree. Scale factor needs to be added here.

### Key layout constants to scale (in resized / paint)
- `getLocalBounds().reduced(8)` — 8px outer margin
- `area.removeFromTop(28)` — header bar
- `area.removeFromBottom(60)` — footer instructions
- `constexpr int kLeftColW = 448` — left column
- LFO column widths: 150px each
- Font: `pixelFont_.withHeight(10.0f)` — must scale with factor

### Integration points
- `PluginProcessor::getStateInformation` / `setStateInformation` — add scale factor property
- `PluginEditor` constructor — read saved scale, call `setSize` with scaled dimensions, store `scaleFactor_`
- `PluginEditor::resized()` — derive `scaleFactor_ = (float)getWidth() / 1120.0f`, apply to all layout constants
- `PluginEditor::paint()` — same scale factor for any hardcoded pixel draws not driven by component bounds
