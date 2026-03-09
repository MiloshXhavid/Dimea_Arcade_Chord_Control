---
phase: 43-resizable-ui
plan: 01
subsystem: ui
tags: [juce, resize, constrainer, aspect-ratio, persistence, xml, scale-factor]

# Dependency graph
requires:
  - phase: 43.2-living-space
    provides: stable PluginEditor/PluginProcessor baseline after living-space draw passes
provides:
  - std::atomic<double> savedUiScale_ in PluginProcessor — persisted via XML round-trip
  - scaleFactor_ float member in PluginEditor — derived from window width at resize time
  - setScaleFactor/getScaleFactor on PixelLookAndFeel — font scaling foundation
  - JUCE resize API wired: setResizeLimits(840,630,2240,1680) + setFixedAspectRatio(4:3) + setResizable(true,false)
affects: [43-02-layout-scaling, any phase that reads scaleFactor_ or savedUiScale_]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - scaleFactor_ derived from getWidth()/1120.0f in resized() — single derivation point
    - savedUiScale_ persisted as XML attribute on root element (not APVTS PARAM child) — survives apvts.replaceState without becoming an automatable parameter
    - setResizeLimits before setFixedAspectRatio before setResizable — mandatory JUCE order

key-files:
  created: []
  modified:
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp

key-decisions:
  - "savedUiScale_ stored as XML attribute on root element (not PARAM child) — apvts.replaceState ignores non-PARAM attributes, so scale factor does not become an automatable parameter and does not interfere with DAW automation lanes"
  - "scaleFactor_ derived from getWidth()/1120.0f in resized() — this is the single source of truth; constructor reads savedUiScale_ only to compute initial setSize, then resized() takes over"
  - "setResizeLimits(840,630,2240,1680) encodes 0.75x min and 2.0x max at 1120x840 base"
  - "setResizable(true, false) — host-driven resize only, no JUCE corner handle drawn inside plugin UI"
  - "PixelLookAndFeel font methods scale by scaleFactor_: getLabelFont multiplies the label's existing height (preserves per-label sizing), getComboBoxFont/getTextButtonFont use direct multiplied literals"

patterns-established:
  - "Scale pattern: all layout pixel constants in resized() will be multiplied by scaleFactor_ in Plan 02 — this plan establishes the member and derivation only"
  - "Font scaling in paint(): literal font heights multiplied by scaleFactor_ inline at setFont call sites"

requirements-completed: [RES-01, RES-04]

# Metrics
duration: 20min
completed: 2026-03-09
---

# Phase 43 Plan 01: Resizable UI Summary

**JUCE resize constrainer wired with 4:3 aspect lock (0.75x–2.0x), scaleFactor_ foundation established, and scale persistence via XML round-trip on PluginProcessor**

## Performance

- **Duration:** ~20 min
- **Started:** 2026-03-09T22:54:51Z
- **Completed:** 2026-03-09T23:00:00Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- PluginProcessor now persists `savedUiScale_` as an XML attribute on the state root element — survives save/load without polluting APVTS automation
- PluginEditor constructor reads saved scale, calls `setResizeLimits` / `setFixedAspectRatio` / `setResizable` in mandatory order — window is now host-resizable
- `scaleFactor_` derived fresh from `getWidth()/1120.0f` on every `resized()` call and written back to `proc_.savedUiScale_` for persistence
- PixelLookAndFeel font methods and literal font sizes in `paint()` scale by `scaleFactor_` — visual text readability maintained at non-1x sizes

## Task Commits

Each task was committed atomically:

1. **Task 1: Processor persistence — savedUiScale_ + XML round-trip** - `a428b24` (feat)
2. **Task 2: Editor resize infrastructure — scaleFactor_, PixelLAF scale, JUCE resize API** - `0fb4098` (feat)

## Files Created/Modified

- `Source/PluginProcessor.h` — added `std::atomic<double> savedUiScale_ { 1.0 }` to public section
- `Source/PluginProcessor.cpp` — getStateInformation writes `uiScaleFactor` attribute; setStateInformation reads it back clamped to 0.75..2.0
- `Source/PluginEditor.h` — added `scaleFactor_` member to PluginEditor; added `setScaleFactor`/`getScaleFactor` and `scaleFactor_` to PixelLookAndFeel
- `Source/PluginEditor.cpp` — constructor resize API calls; resized() scale derivation; paint() font scaling; PixelLookAndFeel font method scaling

## Decisions Made

- `uiScaleFactor` sits on the root XML element as an attribute, not as a `<PARAM>` child. This means `apvts.replaceState()` ignores it entirely — the scale factor does not become an automatable parameter.
- `scaleFactor_` is derived from `getWidth()/1120.0f` on every `resized()` call and written back to `proc_.savedUiScale_`. The constructor reads `savedUiScale_` only to compute initial `setSize`. This gives a clean single source of truth.
- `setResizable(true, false)` — first arg enables host-driven resize; second arg `false` means no JUCE corner handle drawn inside the plugin window.

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None. Build clean on both tasks.

## Next Phase Readiness

- Resize infrastructure complete; Plan 02 can now wrap all layout constants in `sc()` (or multiply by `scaleFactor_`) inside `resized()`
- `scaleFactor_` is accessible anywhere in PluginEditor that needs it
- LookAndFeel font scaling is live; component-level fonts set at construction time still use fixed heights — Plan 02 will update those in `resized()`

---
*Phase: 43-resizable-ui*
*Completed: 2026-03-09*
