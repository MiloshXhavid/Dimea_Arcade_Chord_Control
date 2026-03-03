# Phase 28: Random Free Redesign - Context

**Gathered:** 2026-03-03
**Status:** Ready for planning

<domain>
## Phase Boundary

Redesign the Random Free trigger timing system in TriggerSystem so behavior is unambiguously determined by two controls — RND SYNC toggle and DAW Sync. Simultaneously redesign the semantics of Population and Probability knobs to represent burst frequency and burst size respectively. No UI layout changes, no new APVTS parameters — only logic and semantics change.

</domain>

<decisions>
## Implementation Decisions

### Sync Matrix (RND SYNC authority rule)

- **RND SYNC OFF** → Poisson random intervals, always. DAW Sync state does NOT override this. If the musician turns off RND SYNC, they always get truly random/free firing regardless of whether the DAW is running.
- **RND SYNC ON + DAW Sync active + DAW playing** → fire on DAW ppq grid at selected subdivision (existing ppq-index behavior)
- **RND SYNC ON + DAW Sync not active or DAW not playing** → fire on internal free-tempo clock grid at selected subdivision (existing `randomPhase_` sample counter)

> ⚠️ **Deviation from RND-10 as written**: RND-10 states "DAW Sync ON → DAW grid regardless of RND SYNC toggle." User explicitly overrides this: RND SYNC OFF = always random, DAW sync does not hijack free-random mode. Planner should follow this context, not RND-10 literally.

### Free-Random Interval Model (RND SYNC OFF)

- Use **Poisson distribution**: each voice independently draws a random wait time from an exponential distribution. Average rate = Population (burst events per unit time).
- Intervals are truly non-grid-aligned — no regular periodicity, consecutive gaps differ.
- Subdivision knob does NOT determine when bursts start in free mode. It controls only burst-internal note spacing (see below).

### Population + Probability Semantics (redesigned)

**Population (1–64):**
- = burst frequency / burst event count
- SYNC ON: exactly Population burst events fire per bar, spaced on the subdivision grid
- SYNC OFF: Population sets the Poisson rate (average burst events per bar equivalent)

**Probability (0–100%):**
- = burst size (notes per burst event)
- 0% → 0 notes (silence — nothing fires)
- 100% → 64 rapid re-triggers per burst event (granular feel)
- Linear mapping: Probability% × 64 = note count per burst (round to int, minimum 0)

**Subdivision:**
- = timing between individual notes within a burst (note-to-note spacing)
- Applies in BOTH SYNC ON and SYNC OFF modes
- Does NOT determine when bursts start in SYNC OFF mode

**Burst mechanics:**
- Each burst fires rapid re-triggers on the same voice (same MIDI channel for that voice)
- Each note in a burst is a separate note-on; pitch is sampled at burst-start time (existing sample-and-hold logic)
- Must guarantee note-off for every note-on — `randomGateRemaining_[v]` countdown per note, no MIDI hang even at 64 notes × fast subdivision

### MIDI Safety at High Probability

- At Probability = 100% with fast subdivision (1/64T), 64 notes fire rapidly per burst event
- Each note in a burst MUST be properly tracked and note-off'd before the next note-on on the same pitch fires
- Planner should ensure the note-off countdown (`randomGateRemaining_`) is robust against burst overlap

### Claude's Discretion

- Exact Poisson algorithm (exponential wait draw using LCG, same seed pattern as existing codebase — `seed * 1664525u + 1013904223u`)
- How `hitsPerBarToProbability()` gets refactored or replaced for the new semantics
- Whether burst notes within a single event share one `randomGateRemaining_` countdown or each have their own

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `randomPhase_[v]`: sample accumulator for free-mode clock — needs to become a Poisson wait timer (draw random wait on each burst, count down, fire burst when elapsed)
- `prevSubdivIndex_[v]`: used for DAW ppq grid detection in SYNC ON — reusable as-is for the SYNC ON path
- `randomGateRemaining_[v]`: note-off countdown — needs extension to handle per-note burst countdowns
- `hitsPerBarToProbability()`: converts Population → gating probability — needs redesign to map Population → Poisson rate, Probability → burst size
- LCG pattern (`seed * 1664525u + 1013904223u`): already in LfoEngine — use same for Poisson RNG, audio-thread safe

### Established Patterns
- `randomFired[v]` bool array per block: fire decisions aggregated before per-voice loop — preserve this pattern, but `randomFired[v]` may now trigger a burst of N notes rather than a single note
- Note-on/off via `fireNoteOn()` / `fireNoteOff()` helpers — all burst notes must go through these

### Integration Points
- `TriggerSystem::process()` — all changes confined here (subdivision clock block + RandomFree branch)
- `PluginProcessor.cpp` line ~1381: `tp.randomClockSync` and `tp.randomFreeTempo` read from APVTS — no changes needed to processor parameter collection
- No APVTS changes (no new parameters): existing `randomClockSync`, `randomFreeTempo`, `randomPopulation`, `randomProbability`, `randomSubdiv[v]` are reused with new semantics

</code_context>

<specifics>
## Specific Ideas

- "Granular feel" at Probability 100% + 64 notes — the burst should sound like a tight cluster or micro-roll
- The Poisson wait should be drawn fresh after each burst completes, not pre-scheduled — reactive, not predictive

</specifics>

<deferred>
## Deferred Ideas

- None — discussion stayed within phase scope

</deferred>

---

*Phase: 28-random-free-redesign*
*Context gathered: 2026-03-03*
