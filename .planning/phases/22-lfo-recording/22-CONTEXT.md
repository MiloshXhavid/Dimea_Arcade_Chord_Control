# Phase 22: LFO Recording - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Add Arm/Record/Playback state machine to each LfoEngine so players can capture one loop cycle of live LFO output and replay it in perfect sync with the looper. Distort stays adjustable during playback. Scoped to: ring buffer capture + state machine per LFO + UI Arm/Clear buttons + control gray-out.

Recording/playback is per-LFO (X and Y independently). No arpeggiator, no looper extension, no new MIDI routing.

</domain>

<decisions>
## Implementation Decisions

### Per-LFO Independence
- LFO X and LFO Y each get their own independent Arm + Clear buttons
- Both LFOs can be armed simultaneously — both capture on the same looper record cycle
- One LFO can be in playback while the other is live or recording

### Looper Coupling
- Recording is tightly coupled to the looper: capture ONLY starts when the looper enters record mode
- Capture ends exactly when the looper exits record mode (one loop length captured automatically)
- LFO playback syncs to the looper — the captured shape loops in time with the looper
- **Looper Clear** → resets LFO to live mode (recording/playback state wiped)
- **Looper Stop** → LFO keeps its current state (stays in playback if it was playing)

### Arm Button States
- **Unarmed** — button off/dim
- **Armed (waiting)** — button blinks, matching the looper REC button blink pattern
- **Recording** — button lit steady (capturing in progress)
- **Playback** — button stays lit steady to indicate "playback active"
- Can re-arm an LFO that is already in playback mode — arms for the NEXT looper record cycle and overwrites the current buffer

### Arm Blocked Condition
- Arm button is blocked (non-interactive) when the LFO Enabled toggle is OFF
- LFO must be enabled before it can be armed

### UI Grayout During Playback
- When an LFO enters playback mode: Shape, Freq, Phase, and Level controls are visually grayed out and non-interactive
- Distort slider stays fully adjustable and audibly affects playback output
- Clear button is available to exit playback and return to live mode

### Preset Persistence
- Session-only — the recorded buffer is NOT saved with the preset
- Loading any preset always starts with LFO in live (unarmed) mode
- No buffer serialization needed in getStateInformation / setStateInformation

### Claude's Discretion
- Runtime ring buffer resolution (4096 points with interpolation on playback is a sensible default)
- Exact interpolation method for playback (linear is fine)
- Exact position of Arm and Clear buttons within each LFO panel (ROADMAP says Arm goes next to Sync button)

</decisions>

<specifics>
## Specific Ideas

- Arm blink pattern should match the existing looper REC button blink — consistent visual language
- Arm button doubles as a playback indicator (stays lit, no separate indicator needed)
- Clear button is only meaningful during playback — can be hidden or grayed out when not in playback mode (Claude decides)

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `LfoEngine` (LfoEngine.h/cpp): standalone class — add arm/record/playback state machine here. `process()` and `reset()` are the only public API; state machine fits naturally alongside them.
- `LooperEngine`: has `armWait()`, `isRecordArmed()`, `isRecording()`, `isRecordPending()` — mirror this pattern for LfoEngine arm states
- `styleButton()`, `setAlpha()`: already used for grayed-out OPTION mode indicator — apply same pattern for LFO playback gray-out
- `ButtonParameterAttachment`: used for toggle buttons (SUB8, loopSyncBtn_, etc.) — Arm/Clear are manual onClick (not APVTS-backed) since they drive transient state, not persistent params
- Timer-based blink pattern: already used for loopRecBtn_ blink — reuse same timerCallback() logic for LFO Arm blink

### Established Patterns
- Atomic state flags for cross-thread communication (e.g., `lfoXSubdivMult_`, `subOctSounding_`) — LFO recording state should expose atomic read flags for UI polling
- `timerCallback()` in PluginEditor polls processor state and updates UI at ~30Hz — LFO arm/playback state should be polled here
- `drawAbove()` label pattern — labels auto-follow component position

### Integration Points
- `PluginProcessor::processBlock()`: reads LFO state per block — add recording dispatch here (check if looper just entered/exited record mode and notify LfoEngine)
- `PluginProcessor::lfoX_` / `lfoY_`: the two LfoEngine instances — each needs arm/record/playback state
- `PluginEditor`: LFO panels have `lfoXSyncBtn_` / `lfoYSyncBtn_` next to which Arm buttons are added; `lfoXPanelBounds_` / `lfoYPanelBounds_` for layout
- APVTS: no new persistent params needed (session-only state); arm/clear are imperative actions not APVTS params
- LooperEngine state detection: `looper_.isRecording()` transition edge (false→true) starts capture; (true→false) ends capture

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 22-lfo-recording*
*Context gathered: 2026-03-01*
