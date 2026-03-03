# Phase 28: Random Free Redesign - Research

**Researched:** 2026-03-03
**Domain:** C++ real-time audio / JUCE MIDI plugin / stochastic trigger systems
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Sync Matrix (RND SYNC authority rule)**

- **RND SYNC OFF** → Poisson random intervals, always. DAW Sync state does NOT override this. If the musician turns off RND SYNC, they always get truly random/free firing regardless of whether the DAW is running.
- **RND SYNC ON + DAW Sync active + DAW playing** → fire on DAW ppq grid at selected subdivision (existing ppq-index behavior)
- **RND SYNC ON + DAW Sync not active or DAW not playing** → fire on internal free-tempo clock grid at selected subdivision (existing `randomPhase_` sample counter)

> Deviation from RND-10 as written: RND-10 states "DAW Sync ON → DAW grid regardless of RND SYNC toggle." User explicitly overrides this: RND SYNC OFF = always random, DAW sync does not hijack free-random mode. Planner should follow this context, not RND-10 literally.

**Free-Random Interval Model (RND SYNC OFF)**

- Use Poisson distribution: each voice independently draws a random wait time from an exponential distribution. Average rate = Population (burst events per unit time).
- Intervals are truly non-grid-aligned — no regular periodicity, consecutive gaps differ.
- Subdivision knob does NOT determine when bursts start in free mode. It controls only burst-internal note spacing (see below).

**Population + Probability Semantics (redesigned)**

Population (1–64):
- = burst frequency / burst event count
- SYNC ON: exactly Population burst events fire per bar, spaced on the subdivision grid
- SYNC OFF: Population sets the Poisson rate (average burst events per bar equivalent)

Probability (0–100%):
- = burst size (notes per burst event)
- 0% → 0 notes (silence — nothing fires)
- 100% → 64 rapid re-triggers per burst event (granular feel)
- Linear mapping: Probability% × 64 = note count per burst (round to int, minimum 0)

Subdivision:
- = timing between individual notes within a burst (note-to-note spacing)
- Applies in BOTH SYNC ON and SYNC OFF modes
- Does NOT determine when bursts start in SYNC OFF mode

Burst mechanics:
- Each burst fires rapid re-triggers on the same voice (same MIDI channel for that voice)
- Each note in a burst is a separate note-on; pitch is sampled at burst-start time (existing sample-and-hold logic)
- Must guarantee note-off for every note-on — `randomGateRemaining_[v]` countdown per note, no MIDI hang even at 64 notes × fast subdivision

**MIDI Safety at High Probability**

- At Probability = 100% with fast subdivision (1/64T), 64 notes fire rapidly per burst event
- Each note in a burst MUST be properly tracked and note-off'd before the next note-on on the same pitch fires
- Planner should ensure the note-off countdown (`randomGateRemaining_`) is robust against burst overlap

### Claude's Discretion

- Exact Poisson algorithm (exponential wait draw using LCG, same seed pattern as existing codebase — `seed * 1664525u + 1013904223u`)
- How `hitsPerBarToProbability()` gets refactored or replaced for the new semantics
- Whether burst notes within a single event share one `randomGateRemaining_` countdown or each have their own

### Deferred Ideas (OUT OF SCOPE)

- None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| RND-08 | Random Free + RND SYNC OFF → gates fire at truly random intervals with no tempo grid alignment (Poisson distribution or equivalent random interval) | Poisson wait-time model using existing LCG; `randomPhase_[v]` repurposed as countdown timer drawn from exponential distribution |
| RND-09 | Random Free + RND SYNC ON + DAW Sync OFF → gates subdivided to internal free-tempo clock (existing `randomFreeTempo` BPM clock) | Existing `randomPhase_` sample-counter path already does this; only routing logic needs clarifying (currently uses `randomClockSync` bool which conflates two states) |
| RND-10 | Random Free + DAW Sync ON → gates subdivided to DAW beat grid (existing behavior, clarified/verified) | NOTE: Per CONTEXT.md, this only applies when RND SYNC is ALSO ON. RND SYNC OFF always means Poisson regardless of DAW state. |
</phase_requirements>

---

## Summary

Phase 28 redesigns the Random Free trigger timing in `TriggerSystem.cpp` to implement a two-axis control matrix: RND SYNC toggle (primary authority) and DAW Sync state (secondary authority). Currently the code has a single `randomClockSync` bool that conflates "use DAW grid" with "use internal clock" — it does not support a true Poisson/random-interval mode at all. The free mode (`else` branch of the sync check) runs a fixed-rate sample counter, which is periodic, not stochastic.

Additionally the phase redesigns Population and Probability semantics from "gating probability against scheduled slots" to "burst frequency and burst size." Population now drives how often a burst occurs (Poisson rate in free mode, slot count in sync mode). Probability now maps 0–100% linearly to 0–64 notes fired per burst event. This is a pure logic change — no APVTS parameters are added or renamed, no UI layout changes.

The work is entirely confined to `TriggerSystem.cpp`. Two functions need rewriting: the per-voice clock section (lines 111–145) and the RandomFree branch of the per-voice processing loop (lines 247–255). The `hitsPerBarToProbability()` helper is replaced or repurposed. A new per-voice state variable tracks burst countdown (notes remaining in the current burst). All changes are audio-thread only; no UI thread changes needed.

**Primary recommendation:** Rewrite the clock section to produce a three-branch decision (SYNC OFF always → Poisson, SYNC ON + DAW playing → ppq grid, SYNC ON + no DAW → free sample-counter). Add a `burstNotesRemaining_[4]` counter alongside the existing `randomGateRemaining_[4]` countdown. Use the existing LCG `nextRandom()` for Poisson exponential draw and burst-size determination.

---

## Standard Stack

### Core (no new libraries — pure internal refactor)

| Component | Version | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| JUCE 8.0.4 | 8.0.4 | AudioProcessor, APVTS, MidiMessage | Already in project |
| C++17 | — | `std::fmod`, `std::log`, `std::max`, arrays | Project toolchain (VS 18 2026) |
| LCG RNG | existing | Audio-thread-safe PRNG | Already in TriggerSystem.h as `nextRandom()` |

### No new dependencies

This phase adds no libraries. All math uses `<cmath>` (already included in TriggerSystem.cpp).

---

## Architecture Patterns

### Current State Machine (what exists now)

```
processBlock() per-voice clock block:
  for each voice v:
    if randomClockSync:
      if isDawPlaying && ppqPosition >= 0:
        if ppq_subdiv_index changed → randomFired[v] = true
      // else: no trigger (transport stopped)
    else:
      randomPhase_[v] += blockSize
      if randomPhase_[v] >= samplesPerSubdiv:
        randomPhase_[v] = fmod(...)
        randomFired[v] = true

RandomFree branch in per-voice loop:
  if randomFired[v]:
    popProb = hitsPerBarToProbability(population, subdiv)  // slot-fill probability
    userProb = randomProbability  // 0.0–1.0
    if nextRandom() < popProb && nextRandom() < userProb:
      trigger = true  // fire ONE note
```

**Problems with current approach:**
1. `randomClockSync=false` path is periodic (fixed subdivision grid), not stochastic
2. No differentiation between "DAW sync ON/OFF" and "RND SYNC ON/OFF" — one bool does both jobs
3. `hitsPerBarToProbability()` computes slot-fill probability for scheduled slots — wrong semantic
4. One trigger = one note; no burst concept exists

### Redesigned State Machine (target)

```
processBlock() per-voice clock block:
  for each voice v:
    if !randomClockSync:                         // RND SYNC OFF → always Poisson
      randomPhase_[v] -= blockSize               // count down toward next burst
      if randomPhase_[v] <= 0:
        randomFired[v] = true
        randomPhase_[v] = drawPoissonWait(population, sampleRate, bpm_equiv)
    else if isDawPlaying && ppqPosition >= 0:    // RND SYNC ON + DAW playing
      // existing ppq-index subdivision logic (unchanged)
      if ppq_subdiv_index changed → randomFired[v] = true
      // apply population slot-selection here (see SYNC ON section)
    else:                                         // RND SYNC ON + no DAW
      // existing sample-counter clock (unchanged)
      randomPhase_[v] += blockSize
      if randomPhase_[v] >= samplesPerSubdiv:
        randomPhase_[v] = fmod(...)
        randomFired[v] = true
      // apply population slot-selection here

RandomFree branch in per-voice loop:
  if randomFired[v]:
    burstSize = round(randomProbability * 64)   // 0..64 notes
    if burstSize > 0:
      burstNotesRemaining_[v] = burstSize        // start burst
      trigger = true                              // fire first note immediately
```

### Pattern 1: Poisson Exponential Wait Draw

**What:** Draw a random wait time from an exponential distribution parameterized by rate λ (average events per bar). Uses existing LCG.

**Mathematical basis:** If λ = events per bar, and samples per bar = `(sampleRate * 60 / bpm) * 4`, then:
```
mean_wait_samples = samplesPerBar / λ
exponential_draw = -mean_wait_samples * ln(U)   where U = uniform random in (0,1)
```

**Audio-thread safe — uses existing `nextRandom()` which returns float in [0,1).**

**Example:**
```cpp
// Source: project codebase pattern (TriggerSystem.h nextRandom) + math
float drawPoissonWait_samples(float population, double sampleRate, double bpm)
{
    // population = average burst events per bar (1..64)
    // samplesPerBar at 4/4 = sampleRate * 60/bpm * 4
    const double samplesPerBar = (sampleRate / (bpm / 60.0)) * 4.0;
    const double meanWait      = samplesPerBar / static_cast<double>(population);
    // Exponential draw: -mean * ln(U), U uniform in (0,1)
    // nextRandom() returns [0,1); guard against 0 to avoid -inf
    const float u = std::max(1e-6f, nextRandom());
    return static_cast<float>(-meanWait * std::log(static_cast<double>(u)));
}
```

**Confidence:** HIGH — exponential distribution is the standard inverse-CDF approach for Poisson inter-arrival times; math is well-established.

### Pattern 2: Burst Countdown State

**What:** Track how many notes remain in the current burst with a new per-voice integer array. Each processBlock call fires at most one burst note; the subdivision spacing determines inter-note timing within the burst.

**New member in TriggerSystem private section:**
```cpp
std::array<int, 4> burstNotesRemaining_ {};   // notes left to fire in current burst; 0 = idle
std::array<double, 4> burstPhase_ {};          // samples until next burst note fires (within burst)
```

**Burst logic pseudocode (within RandomFree per-voice block):**

```cpp
// On new burst trigger (randomFired[v] == true):
int burstSize = static_cast<int>(std::round(p.randomProbability * 64.0f));
if (burstSize > 0)
{
    burstNotesRemaining_[v] = burstSize;
    burstPhase_[v] = 0.0;   // fire first note immediately
}

// Each block — drain burst if active:
if (burstNotesRemaining_[v] > 0)
{
    burstPhase_[v] -= static_cast<double>(p.blockSize);
    if (burstPhase_[v] <= 0.0)
    {
        // Fire next burst note
        trigger = true;
        burstNotesRemaining_[v]--;
        // Schedule next note in burst
        if (burstNotesRemaining_[v] > 0)
            burstPhase_[v] = samplesPerSubdiv;   // spacing = subdivision
    }
}
```

**Confidence:** HIGH — this is a standard multi-event countdown pattern; no external library needed.

### Pattern 3: SYNC ON Population as Slot Selection

**What:** In SYNC ON mode, Population defines how many of the available subdivision slots per bar actually fire. This maps to the original `hitsPerBarToProbability()` concept but with clearer semantics: the probability of any given slot firing = min(Population / subdivsPerBar, 1.0).

**Note:** The existing `hitsPerBarToProbability()` already implements this correctly. In SYNC ON mode it is reused as-is. Only the SYNC OFF path (Poisson) is new.

**Example (SYNC ON slot selection for RandomFree):**
```cpp
// SYNC ON path: randomFired[v] is already true for the slot
// Decide whether this slot fires a burst
float slotProb = juce::jlimit(0.0f, 1.0f,
    p.randomPopulation / static_cast<float>(subdivsPerBar));
if (nextRandom() < slotProb)
{
    int burstSize = static_cast<int>(std::round(p.randomProbability * 64.0f));
    if (burstSize > 0)
        // start burst
}
```

### Anti-Patterns to Avoid

- **Firing all burst notes in one block:** A 64-note burst at 1/64 subdivision would overlap note-off windows and create MIDI garbage. Fire one note per block, use `burstPhase_` countdown to space them.
- **Resetting Poisson wait with subdivision math:** In free mode, do NOT use `samplesPerSubdiv` as the period — the whole point is that the wait is stochastic, not periodic.
- **Drawing `log(0)`:** `nextRandom()` can theoretically return 0.0f. Guard with `std::max(1e-6f, u)` before computing `-log(u)`.
- **Skipping burstNotesRemaining_ reset on mode switch:** The universal mode-switch in the per-voice loop already fires `fireNoteOff()` on source change. Also reset `burstNotesRemaining_[v] = 0` and `burstPhase_[v] = 0.0` there to avoid ghost burst notes after a mode switch.
- **Using `bpm` (DAW BPM) for Poisson rate in free mode:** Free mode uses `randomFreeTempo`, not `p.bpm`. Use `p.randomFreeTempo` for the exponential draw when `randomClockSync == false`.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Poisson inter-arrival time | Custom distribution tables, lookup arrays | Inverse-CDF via `-mean * log(U)` with existing LCG | One-liner; numerically correct; no alloc |
| Thread-safe RNG | std::mt19937, std::random_device | Existing `nextRandom()` LCG in TriggerSystem | Already audio-thread safe; matches LFO pattern |
| MIDI note tracking | Custom pitch registries | Existing `activePitch_[v]` + `gateOpen_[v]` + `randomGateRemaining_[v]` | Already handles note-off correctly |

**Key insight:** This is a pure logic refactor. All infrastructure (RNG, gate tracking, MIDI callbacks) already exists. The work is reshaping three short code sections, not building new machinery.

---

## Common Pitfalls

### Pitfall 1: The Three-State Clock Confusion

**What goes wrong:** Checking only `randomClockSync` creates two states (sync on/off). The design needs three behaviors (Poisson / internal-clock / DAW-grid). Writing `if randomClockSync` with a nested `if isDawPlaying` inside the `else` branch is the old anti-pattern — it makes free mode still periodic.

**Why it happens:** The current code was designed before Poisson free mode was a requirement.

**How to avoid:** Restructure as: `if (!randomClockSync) { Poisson } else if (isDawPlaying && ppqPos >= 0) { DAW grid } else { internal clock }`. The outer condition on `!randomClockSync` must be first — it has absolute authority.

**Warning signs:** Free-mode bursts arriving at perfectly regular intervals = still using the old sample-counter.

### Pitfall 2: Burst Overlap with randomGateRemaining_

**What goes wrong:** A burst fires 64 notes. Each note-on sets `randomGateRemaining_[v]` to gateLength * subdivSamples. If gateLength > 1/64 of a bar, notes overlap. Each new note-on fires `fireNoteOff` on the previous activePitch_, then opens a new gate. At high burst rates this is correct behavior (each burst note chokes the previous), but the operator must understand this is the intended behavior, not a bug.

**Why it happens:** `fireNoteOn` already calls `fireNoteOff` if gate is open (line 283–284 of current TriggerSystem.cpp). The choke-before-retriggger pattern is already there.

**How to avoid:** No change needed for single-pitch bursts — choke-on-retriggger is correct. Document that burst notes at same pitch will always be chokeretriggered, which is the desired "roll" effect.

**Warning signs:** MIDI hangs = note-off was skipped. Check that `burstNotesRemaining_[v]` is reset to 0 on mode switch and on `resetAllGates()`.

### Pitfall 3: Poisson Wait Initialized to 0

**What goes wrong:** On plugin load or transport start, `randomPhase_[v]` starts at 0 (from `resetAllGates()`). Under the countdown model (phase decrements toward 0), starting at 0 means the first burst fires in the very first block. This may be desirable (immediate response) but could surprise the user on transport restart.

**Why it happens:** `resetAllGates()` zeros all per-voice arrays.

**How to avoid:** On mode switch from SYNC ON → SYNC OFF (detected via `prevSrc_` or a new `prevSyncMode_` flag), draw a fresh Poisson wait immediately rather than inheriting 0 from reset. Alternatively, initialize `randomPhase_[v]` to the drawn wait value at the moment SYNC OFF is detected for the first time, not 0. Since `resetAllGates()` sets to 0, document that the first burst after reset fires immediately (this matches the "reactive, not predictive" design intent in CONTEXT.md).

**Warning signs:** Unexpected immediate burst on plugin load or transport restart.

### Pitfall 4: Population Units Mismatch Between Modes

**What goes wrong:** In SYNC ON mode, Population = "slots per bar that fire." In SYNC OFF mode, Population = "average burst events per bar" (Poisson rate λ). A musician setting Population=4 expects "roughly 4 bursts per bar" in both modes — the math must produce equivalent subjective density.

**Why it happens:** The mapping is consistent conceptually (4 bursts/bar either way) but the implementation differs: slot probability vs. exponential rate.

**How to avoid:** In SYNC OFF, use `λ = population` directly as the Poisson rate (events per bar). `mean_wait_samples = samplesPerBar / population`. This gives average Population bursts per bar, matching the SYNC ON interpretation. Use `randomFreeTempo` for BPM when computing samplesPerBar in SYNC OFF mode.

**Warning signs:** SYNC OFF sounds much denser or sparser than SYNC ON at identical Population settings.

### Pitfall 5: randomGateRemaining_ Semantics During Burst

**What goes wrong:** Currently `randomGateRemaining_[v]` is one countdown per trigger event (samples until note-off). During a burst of N notes, this counter is reset N times (once per note). The last reset determines when the final burst note's gate closes. This is correct — the last note gets a full gate-length duration.

**Why it happens:** The existing note-on path at line 278–300 already handles this: every trigger fires `fireNoteOff` if gate open (choking previous note), then `fireNoteOn`, then sets `randomGateRemaining_[v]`. No special burst-aware gate logic is needed IF burst notes are fired one-per-block via `burstPhase_` countdown.

**How to avoid:** Do NOT accumulate `randomGateRemaining_` across burst notes. Let each trigger within the burst reset it normally. The per-block auto-note-off at lines 312–321 handles it correctly.

**Warning signs:** Notes sustaining beyond gateLength × subdivDuration = remaining countdown not being reset at each burst note-on.

---

## Code Examples

Verified patterns from existing source:

### Existing LCG RNG (TriggerSystem.h line 143)
```cpp
// Source: Source/TriggerSystem.h
uint32_t rng_ = 0x1234ABCD;
float nextRandom() { rng_ = rng_ * 1664525u + 1013904223u; return (rng_ >> 1) / float(0x7FFFFFFF); }
```
Returns float in [0.0, 1.0). Used in both LfoEngine and TriggerSystem. Audio-thread safe (no mutex, no alloc, no syscall).

### Existing fireNoteOn / fireNoteOff Choke Pattern (TriggerSystem.cpp lines 278–308)
```cpp
// Source: Source/TriggerSystem.cpp
if (trigger)
{
    if (src == TriggerSource::RandomFree || src == TriggerSource::RandomHold)
    {
        if (gateOpen_[v].load())
            fireNoteOff(v, ch - 1, 0, p);
        fireNoteOn(v, p.heldPitches[v], ch - 1, 0, p);
        // ... set randomGateRemaining_[v]
    }
}
```
All burst notes go through this same block. The choke-before-retriggger is already handled correctly.

### Existing Per-Subdiv Sample Counter (TriggerSystem.cpp lines 138–144)
```cpp
// Source: Source/TriggerSystem.cpp (SYNC OFF path, currently periodic)
randomPhase_[v] += static_cast<double>(p.blockSize);
if (samplesPerSubdiv > 0.0 && randomPhase_[v] >= samplesPerSubdiv)
{
    randomPhase_[v] = std::fmod(randomPhase_[v], samplesPerSubdiv);
    randomFired[v] = true;
}
```
This needs to become a countdown draw in the Poisson path. In SYNC ON + no DAW path, it remains as-is but `samplesPerSubdiv` uses `randomFreeTempo` BPM (already correct since `effectiveBpm = randomFreeTempo` when `!randomClockSync`).

### Poisson Draw Pattern (new, derivable from math)
```cpp
// Exponential inter-arrival time for Poisson process with rate λ events/bar
// Source: inverse-CDF method — standard textbook algorithm
float drawPoissonWaitSamples(float population, double sampleRate, double freeTempoBpm)
{
    const double samplesPerBar = (sampleRate * 60.0 / freeTempoBpm) * 4.0;
    const double meanWait      = samplesPerBar / static_cast<double>(population);
    const float  u             = std::max(1e-6f, nextRandom());  // guard log(0)
    return static_cast<float>(-meanWait * std::log(static_cast<double>(u)));
}
```

### hitsPerBarToProbability — SYNC ON reuse
```cpp
// Source: Source/TriggerSystem.cpp (existing, retained for SYNC ON path)
// In SYNC ON mode: probability that a given subdivision slot fires a burst
static float hitsPerBarToProbability(float population, RandomSubdiv subdiv)
{
    // ... existing implementation ...
    return juce::jlimit(0.0f, 1.0f, population / subdivsPerBar);
}
// Usage in SYNC ON path:
if (nextRandom() < hitsPerBarToProbability(p.randomPopulation, p.randomSubdiv[v]))
{
    int burstSize = static_cast<int>(std::round(p.randomProbability * 64.0f));
    if (burstSize > 0)  { /* start burst */ }
}
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `randomClockSync` = one bool for two states | Three-branch: Poisson / internal-clock / DAW-grid | Phase 28 | Musician gets unambiguous mode semantics |
| randomPhase_[v] = periodic sample counter in free mode | randomPhase_[v] = Poisson wait countdown in free mode | Phase 28 | Truly stochastic timing, no grid aliasing |
| One note per trigger event | N notes per burst event (N = Probability × 64) | Phase 28 | Granular/roll feel at high Probability |
| `hitsPerBarToProbability()` used in free mode | `hitsPerBarToProbability()` SYNC ON only; Poisson rate used in free mode | Phase 28 | Population semantics consistent across modes |

**Deprecated/outdated after Phase 28:**
- The `else` branch of `if (randomClockSync)` producing a periodic sample clock when `randomClockSync=false`. It is replaced by the Poisson wait draw.

---

## Implementation Scope Map

This section maps the design to exact code locations to help the planner scope tasks:

### Files Changed: TriggerSystem.cpp only

**Change 1: Clock block restructure (lines 107–145)**

Current structure:
```
if (randomClockSync)   → DAW ppq grid path
else                   → periodic sample-counter path (free mode)
```

New structure:
```
if (!randomClockSync)             → Poisson countdown path (new)
else if (isDawPlaying && ppq>=0)  → DAW ppq grid path (unchanged)
else                              → internal clock sample-counter (unchanged, using randomFreeTempo)
```

The Poisson path repurposes `randomPhase_[v]` as a countdown (decrements from drawn value toward 0) rather than an accumulator (increments toward threshold). This is a semantic flip — the reset path in `resetAllGates()` sets it to 0, which under countdown means "fire immediately on next block" — acceptable per design intent.

**Change 2: RandomFree branch in per-voice loop (lines 247–255)**

Current: one trigger = one note, using double-probability gate.

New: one trigger = burst of N notes, where N = `round(randomProbability * 64)`. Burst drainage is handled by `burstNotesRemaining_[v]` and `burstPhase_[v]` countdown.

**Change 3: New private member arrays (TriggerSystem.h)**

Add to private section:
```cpp
std::array<int,    4> burstNotesRemaining_ {};  // notes left in current burst (0 = idle)
std::array<double, 4> burstPhase_          {};  // samples until next burst-internal note fires
```

Also add `resetAllGates()` zeroing for these two arrays (same pattern as existing arrays).

**Change 4: `hitsPerBarToProbability()` — retire from free path, keep for SYNC ON**

The function remains in the file but is only called from the SYNC ON slot-selection path. The SYNC OFF Poisson draw replaces it in that context. The function's existing `SixtyFourth` default case covers the subdivision enum correctly including new triplet values from Phase 27.

**No changes to:**
- PluginProcessor.cpp (parameter assembly unchanged)
- TriggerSystem.h public API (no new public methods)
- RandomHold source (uses same `randomFired[v]` array; burst semantics should be applied there too if desired — but CONTEXT.md does not mention RandomHold, so leave unchanged unless planner decides otherwise)

---

## Open Questions

1. **RandomHold burst semantics**
   - What we know: RandomHold uses the same `randomFired[v]` array and the same probability check as RandomFree
   - What's unclear: Should RandomHold also get burst behavior (N notes per trigger), or remain single-note-per-trigger?
   - Recommendation: Apply the same burst semantics to RandomHold for consistency. The CONTEXT.md focuses on RandomFree but does not explicitly exclude Hold. The planner should apply burst to both sources since `randomProbability` = burst size now applies globally.

2. **Burst notes remaining on transport restart**
   - What we know: `resetAllGates()` is called from `releaseResources()` and `processBlockBypassed()`
   - What's unclear: Should a DAW transport stop (not full reset) also drain any in-progress burst?
   - Recommendation: Yes. When `wasPlaying_` transitions from true→false (transport stop), set `burstNotesRemaining_[v] = 0` and `burstPhase_[v] = 0.0` to avoid burst drainage continuing after transport stops.

3. **Poisson wait minimum floor**
   - What we know: At Population=64, mean wait = samplesPerBar/64. At 120 BPM that's ~88200/64 ≈ 1378 samples ≈ 31ms. At Population=64 + 240 BPM, mean_wait ≈ 15ms.
   - What's unclear: Should there be a minimum floor (e.g., 10ms = ~441 samples at 44100 Hz) to prevent machine-gun notes at extreme settings?
   - Recommendation: Apply a minimum floor of 10ms (matching the existing `minDurationSamples` pattern at line 297 of TriggerSystem.cpp). This prevents MIDI saturation at Population=64 + fast tempo.

---

## Sources

### Primary (HIGH confidence)
- Source/TriggerSystem.cpp — full code read; all existing patterns verified directly
- Source/TriggerSystem.h — full header read; all private members and public API verified
- Source/PluginProcessor.cpp lines 1340–1385 — APVTS parameter assembly confirmed
- .planning/phases/28-random-free-redesign/28-CONTEXT.md — design decisions locked

### Secondary (MEDIUM confidence)
- Exponential distribution inverse-CDF: `-mean * log(U)` — standard textbook method; no external source needed, derivable from Poisson process definition

### Tertiary (LOW confidence)
- None

---

## Metadata

**Confidence breakdown:**
- Architecture: HIGH — full source code read; all relevant functions inspected directly
- Implementation scope: HIGH — exact line numbers and function names identified
- Poisson math: HIGH — inverse-CDF for exponential distribution is standard; verified internally consistent with existing LCG output range [0,1)
- Burst mechanics: HIGH — choke-and-retriggger pattern already present in codebase; extension is additive
- Edge cases (transport stop, burst drain): MEDIUM — logic derived from existing patterns but not exercised in code yet

**Research date:** 2026-03-03
**Valid until:** 2026-04-03 (JUCE 8.0.4 stable; no external dependencies)
