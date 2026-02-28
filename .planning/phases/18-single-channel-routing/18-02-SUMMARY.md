---
phase: 18-single-channel-routing
plan: 02
subsystem: ui
tags: [juce, combobox, apvts-attachment, routing-panel, multi-channel, single-channel]

# Dependency graph
requires:
  - phase: 18-01
    provides: singleChanMode and singleChanTarget APVTS params, voiceCh0..3 params already existed

provides:
  - routingModeBox_ ComboBox bound to singleChanMode (Multi-Channel / Single Channel toggle)
  - singleChanTargetBox_ ComboBox bound to singleChanTarget (ch 1-16, visible in Single Channel mode)
  - voiceChBox_[4] ComboBoxes bound to voiceCh0..3 (visible in Multi-Channel mode)
  - Routing panel in resized() below trigger section in left column
  - timerCallback() visibility toggle driven by singleChanMode param value

affects:
  - 18-03 (visual verification of this panel)
  - any phase that reads or modifies singleChanMode or voiceCh* from the UI

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "ComboBox wiring: items added before APVTS attachment created (consistent with existing trigSrc_ pattern)"
    - "Shared-position layout: singleChanTargetBox_ and voiceChGrid occupy same vertical band, toggled by timerCallback"
    - "Full type name juce::AudioProcessorValueTreeState::ComboBoxAttachment used in header when ComboAtt alias not yet in scope"

key-files:
  created: []
  modified:
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp

key-decisions:
  - "Used full JUCE type name in PluginEditor.h members (before using alias declared) to avoid C2923 template error"
  - "singleChanTargetBox_ and voiceChBox_ grid share the same vertical band in resized(); only visibility differs"
  - "Routing panel placed between trigger section and ARP/looper — removeFromTop() in left column layout"

patterns-established:
  - "Multi-mode UI panels: place all controls at same bounds, toggle via timerCallback on each 30Hz tick"

requirements-completed:
  - ROUT-01
  - ROUT-02

# Metrics
duration: 15min
completed: 2026-02-28
---

# Phase 18 Plan 02: Single-Channel Routing UI Summary

**Routing panel with routingModeBox_, singleChanTargetBox_, and voiceChBox_[4] wired to APVTS; visibility toggles in timerCallback on singleChanMode param**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-02-28T20:50:00Z
- **Completed:** 2026-02-28T21:05:00Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Added 14 new members to PluginEditor.h (routingLabel_, routingModeBox_, routingModeAtt_, singleChanTargetBox_, singleChanTargetAtt_, voiceChLabel_[4], voiceChBox_[4], voiceChAtt_[4])
- Wired constructor: 6 APVTS ComboBoxAttachments created (routingMode, singleChanTarget, voiceCh0..3) — items added before each attachment
- resized() routing panel below trigger section: routingLabel_ + routingModeBox_ + voice 2x2 grid sharing vertical band with singleChanTargetBox_
- timerCallback() reads singleChanMode param every 30Hz tick; setVisible() drives correct set of controls
- Release build: 0 errors

## Task Commits

1. **Task 1: Add Routing panel members to PluginEditor.h** - `5c78c84` (feat)
2. **Task 2: Wire Routing panel in PluginEditor.cpp** - `9072255` (feat)

## Files Created/Modified

- `Source/PluginEditor.h` - Added routingLabel_, routingModeBox_, routingModeAtt_, singleChanTargetBox_, singleChanTargetAtt_, voiceChLabel_[4], voiceChBox_[4], voiceChAtt_[4] in private section
- `Source/PluginEditor.cpp` - Constructor wiring, resized() layout block, timerCallback() visibility logic

## Decisions Made

- Used `juce::AudioProcessorValueTreeState::ComboBoxAttachment` as the full type name in PluginEditor.h because the `using ComboAtt = ...` alias is declared later in the private section — using the alias at member-declaration time caused C2923 compile error
- singleChanTargetBox_ and the 2x2 voice channel grid are placed at the same vertical coordinates in resized(); timerCallback setVisible() handles mutual exclusion — avoids double height in the panel

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Replaced `ComboAtt` alias with full type name in PluginEditor.h**
- **Found during:** Task 1 (compile step at start of Task 2)
- **Issue:** `using ComboAtt = ...` is declared after the routing panel members in the private section; MSVC C2923 — ComboAtt not a valid template type argument
- **Fix:** Changed three `std::unique_ptr<ComboAtt>` declarations to `std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>`
- **Files modified:** Source/PluginEditor.h
- **Verification:** Release build succeeded with 0 errors after fix
- **Committed in:** 5c78c84 (Task 1 commit includes fix)

---

**Total deviations:** 1 auto-fixed (Rule 1 - compile bug)
**Impact on plan:** Required fix; no scope creep. All specified functionality delivered.

## Issues Encountered

- MSVC C2923 on `std::unique_ptr<ComboAtt>` in header — `using` alias declared later in same class. Fixed by using full type. This is a forward-declaration ordering issue unique to in-class `using` aliases.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Routing panel built and wired; all 6 APVTS attachments functional
- Visual verification (Plan 18-03) can proceed: load VST3, verify panel visible below trigger section, test Multi-Channel and Single Channel switching

---
*Phase: 18-single-channel-routing*
*Completed: 2026-02-28*
