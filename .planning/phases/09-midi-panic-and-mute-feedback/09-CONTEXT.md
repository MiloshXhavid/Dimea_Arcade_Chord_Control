# Phase 09: MIDI Panic and Mute Feedback - Context

**Gathered:** 2026-02-25
**Status:** Ready for planning

<domain>
## Phase Boundary

Add a PANIC! button to the plugin UI and expand the MIDI panic sweep from the current 4-voice-channels + filter-channel approach to a full unconditional 16-channel sweep. The button fires as a one-shot action with brief visual feedback — no persistent mute state remains after firing.

Technical contract (from success criteria, locked):
- Panic emits exactly 48 events: CC64=0, CC120=0, CC123 (allNotesOff) × 16 channels
- Zero CC121 events
- Plugin resumes normal MIDI output immediately after panic fires
- Flash animation reuses the existing 30 Hz editor timer (no second timer added)

</domain>

<decisions>
## Implementation Decisions

### Button placement
- Lives in the **Gamepad section**, on the right side of the gamepad status label row
- Layout: `PS4 Connected         [ PANIC! ]`
- The status label and button share the same row (status label left, button right)

### Button appearance
- Label: **"PANIC!"** (with exclamation mark)
- Width: ~60px — same compact size as other small buttons (RST, DEL, etc.)
- Idle color: Claude's discretion — pick what fits the existing palette best

### Flash feedback
- Duration: **~167ms** (5 frames × 30 Hz timer) — same as RST/DEL flash pattern
- Flash color: **Clr::highlight** — consistent with existing button feedback
- Implementation: consumes existing `flashPanic_` atomic counter in timerCallback, same pattern as `flashLoopReset_` / `flashLoopDelete_`

### Claude's Discretion
- Idle button color (accent, dim, or a subtle warning tint — whatever looks cleanest)
- Exact pixel layout within the gamepad row (left-aligned status label, right-aligned button, or proportioned as fits)

</decisions>

<specifics>
## Specific Ideas

- User explicitly noted: "v small and labeled MIDI PANIC" in an early answer, then settled on "PANIC!" — the short label with exclamation is the final choice
- Flash should feel identical to RST/DEL: snappy confirmation, not a dramatic animation
- The button is intentionally small — it's a utility action, not a prominent feature

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 09-midi-panic-and-mute-feedback*
*Context gathered: 2026-02-25*
