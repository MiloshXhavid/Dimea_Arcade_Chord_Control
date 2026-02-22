# Phase 04: Per-Voice Trigger Sources and Random Gate - Research

**Researched:** 2026-02-22
**Domain:** JUCE audio-thread trigger logic, ppqPosition-based subdivision timing, joystick threshold detection, wall-clock fallback timing, APVTS parameter addition
**Confidence:** HIGH (based primarily on direct codebase reading — all source files read in full)

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Trigger source selector UI**
- Control: Small 3-way toggle per voice using short codes: PAD / JOY / RND
- Position: Below each TouchPlate pad (4 toggles total, one per voice)
- Inactive pad behavior: When a voice is in JOY or RND mode, its TouchPlate pad dims visually and becomes non-functional (no manual trigger from pad in those modes)
- The toggle is a per-voice APVTS parameter (triggerSource0..3 already defined)

**Joystick motion threshold**
- Scope: One global threshold — all joystick-mode voices share the same sensitivity setting
- Control: A slider labeled THRESHOLD (or SENS) — not a knob
- Gate model: Continuous gate — note-on fires when motion exceeds threshold, note-off fires when motion falls back below threshold (joystick stops moving)
- Axis: Both axes contribute — any movement in any direction triggers; combined magnitude used for threshold comparison
- Minimum gate duration: ~50ms minimum to prevent sub-millisecond click artifacts from tiny joystick jitter

**Random gate behavior**
- Density model: Average hits per bar (range 1–8) — not raw probability percentage
- Pitch at trigger time: Current joystick position at the moment the random trigger fires (live pitch, not sample-and-hold)
- Subdivision: Per-voice — each voice in RND mode has its own subdivision selector (1/4, 1/8, 1/16, 1/32); uses existing randomSubdiv APVTS parameter
- Transport-stopped behavior: Random gate fires based on wall-clock time even when DAW transport is stopped
- When transport IS playing: Use ppqPosition for subdivision alignment to stay in sync with DAW

**Note duration for non-pad triggers**
- Random notes: Configurable gate-time knob — range from very short to full subdivision length
- Joystick motion notes: Duration = however long motion stays above threshold; minimum 50ms
- Retrigger rule: Same as Phase 03 — always close-then-reopen, one note at a time per voice (no polyphonic overlap)
- Random retrigger: If a note is still ringing when the next random trigger fires, cut the previous note immediately and start the new one

### Claude's Discretion
- Exact slider/knob placement in the editor layout (wherever it fits cleanly near existing controls)
- Implementation of wall-clock timing when transport is stopped (e.g., juce::Time::getMillisecondCounter() or a sample-counting fallback)
- How combined XY magnitude is computed for threshold comparison (e.g., sqrt(dx²+dy²) or max(|dx|, |dy|))
- APVTS parameter name for the gate-time knob (e.g., `randomGateTime` or `randomGate`)
- The gate-time knob range and default value

### Deferred Ideas (OUT OF SCOPE)
- Per-voice joystick threshold (each voice has its own sensitivity) — future phase
- Variable velocity on random/joystick triggers — velocity implementation deferred globally
- SDL2 gamepad button triggers for joystick and random mode selection — Phase 06
- CC-controllable density parameter (external modulation of random density) — backlog
</user_constraints>

---

## Summary

Phase 04's scope is significantly smaller than it first appears. TriggerSystem already has the skeleton for all three trigger sources: the `TriggerSource` enum, per-voice `sources[]` array, `joystickTrig_` atomic, and the random subdivision clock are all present and wired into processBlock. The APVTS parameters `triggerSource0..3`, `randomDensity`, and `randomSubdiv` are defined, attached, and flowing into `TriggerSystem::ProcessParams` in processBlock.

The **actual gaps** are precisely scoped: (1) The joystick trigger model is a one-shot edge trigger (fires once when `joyMoved` is true), not the continuous gate model the user wants — note-off never fires when motion stops below threshold, and there is no configurable threshold; (2) The random density parameter is defined as raw probability 0..1 float, but the user wants hits-per-bar (1–8) — the parameter range and the probability math in TriggerSystem need changing; (3) Per-voice randomSubdiv is missing — the current design uses one global `randomSubdiv` for all voices; (4) Random gate when transport is stopped does not use wall-clock fallback (it only works when processBlock is running, which is always the case since the plugin is loaded, but the ppqPosition-based alignment only syncs when `pos.isPlaying` is true); (5) A random gate-time (note duration) parameter and its enforcement are completely absent; (6) TouchPlate pad dimming for JOY/RND mode voices is absent from the UI; (7) No joystick threshold APVTS parameter or slider exist yet.

**Primary recommendation:** Two plans match the roadmap structure. Plan 04-01 addresses joystick motion trigger (threshold, continuous gate model, minimum gate duration, pad dimming). Plan 04-02 addresses random gate (density model change, per-voice subdiv, ppqPosition sync, wall-clock fallback, gate-time knob).

---

## Codebase Reality Check (HIGH confidence — direct source reading)

### What Is ALREADY Implemented

| Feature | File | Status |
|---------|------|--------|
| TriggerSource enum: TouchPlate=0, Joystick=1, Random=2 | TriggerSystem.h:7 | DONE |
| RandomSubdiv enum: Quarter/Eighth/Sixteenth/ThirtySecond | TriggerSystem.h:10 | DONE |
| Per-voice sources[4] in ProcessParams | TriggerSystem.h:48 | DONE |
| joystickTrig_ atomic bool | TriggerSystem.h:78 | DONE |
| Joystick delta detection (dx+dy > 0.005f) | TriggerSystem.cpp:53-60 | DONE — but wrong model |
| Joystick trigger routing in processBlock per-voice | TriggerSystem.cpp:105-109 | DONE — but fires one-shot |
| Random subdivision clock (sample counter phase) | TriggerSystem.cpp:62-77 | DONE — uses global subdiv |
| Random trigger probability check | TriggerSystem.cpp:112 | DONE — but wrong density model |
| APVTS params: triggerSource0..3, randomDensity, randomSubdiv | PluginProcessor.cpp:108-115 | DONE |
| trigSrc_[4] combo boxes in PluginEditor | PluginEditor.h:99, .cpp:289-299 | DONE |
| randomDensityKnob_ + randomSubdivBox_ in PluginEditor | PluginEditor.h:101-104, .cpp:301-310 | DONE |
| trigSrcAtt_[4] APVTS attachments | PluginEditor.h:134 | DONE |
| sources[]/heldPitches[]/midiChannels[]/randomDensity/randomSubdiv flowing into TriggerSystem::ProcessParams | PluginProcessor.cpp:303-354 | DONE |
| ppqPosition flowing into processBlock | PluginProcessor.cpp:261-270 | DONE |
| AudioPlayHead::CurrentPositionInfo populated | PluginProcessor.cpp:261-268 | DONE |

### What Is MISSING or WRONG

| Gap | File | What Needs Doing |
|-----|------|-----------------|
| Joystick trigger model is one-shot edge (not continuous gate) | TriggerSystem.cpp:59-60, 105-109 | Replace with threshold + gate-open/gate-close model |
| No configurable joystick threshold (hardcoded 0.005f delta) | TriggerSystem.cpp:55 | Add APVTS param `joystickThreshold`, pass in ProcessParams |
| No joystick threshold slider in UI | PluginEditor | Add slider for joystick threshold |
| No 50ms minimum gate enforcement for joystick | TriggerSystem | Add per-voice sample counter for min gate duration |
| randomDensity is 0..1 probability (not 1–8 hits/bar) | PluginProcessor.cpp:113, TriggerSystem.cpp:112 | Change param range to 1.0..8.0; change probability math |
| randomSubdiv is global (one for all voices) | TriggerSystem.h:53, PluginProcessor.cpp:351 | Add per-voice randomSubdiv; need 4 APVTS params or change model |
| No random gate-time (note duration) parameter | — | Add `randomGateTime` APVTS param; enforce in TriggerSystem |
| Random gate does not use ppqPosition for phase alignment when playing | TriggerSystem.cpp:62-77 | Add ppqPosition input and alignment logic |
| Wall-clock fallback for transport-stopped not robust | TriggerSystem.cpp:62-77 | Sample-counting approach works but needs to be independent of ppq |
| TouchPlate pad does not dim/disable in JOY or RND mode | PluginEditor.cpp:76-88 | Check triggerSource per voice in TouchPlate::paint(); disable mouseDown if not PAD mode |
| Random pitch uses heldPitches[v] (sample-and-hold) not live joystick | PluginProcessor.cpp:313-314, TriggerSystem.cpp:120 | For RND mode voices, pass live (not held) pitch at trigger time |
| Per-voice randomSubdiv UI does not exist | PluginEditor | Need to decide: 4 combo boxes or keep global? (see Open Questions) |

---

## Standard Stack

### Core (all already in use — no new dependencies)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| juce::AudioPlayHead | JUCE 8.0.4 | ppqPosition and bpm from DAW | Already in processBlock; the playhead is queried every block |
| juce::Time | JUCE 8.0.4 | Wall-clock milliseconds for transport-stopped fallback | `juce::Time::getMillisecondCounterHiRes()` is the JUCE-native approach |
| APVTS AudioParameterFloat | JUCE 8.0.4 | New params: joystickThreshold, randomGateTime | Already used for all other parameters in this project |
| std::atomic | C++17 | Thread-safe communication between audio thread and message thread | Already used throughout TriggerSystem |

### No New Dependencies

Phase 04 requires zero new libraries or frameworks. All required capabilities are in JUCE 8.0.4 which is already the locked dependency.

---

## Architecture Patterns

### Pattern 1: Continuous Joystick Gate Model

**Current (wrong) model:**
```
Every processBlock: if (|dx| + |dy|) > 0.005f → joystickTrig_ = true
Per-voice: if (joystickTrig_) → fire note-on (one-shot)
No note-off path for joystick source
```

**Required model:**
```
Every processBlock:
  magnitude = sqrt(dx² + dy²)  [or max(|dx|, |dy|) — Claude's discretion]
  if magnitude > threshold:
    joystickMoving_ = true
    joystickStillSamples_[v] = 0   (reset stillness counter)
  else:
    joystickStillSamples_[v] += blockSize

Per-voice (JOY source):
  if joystickMoving_ and gate NOT open:
    fire note-on                    (start continuous gate)
  if !joystickMoving_ and gate IS open:
    if joystickStillSamples_[v] >= minGateSamples:
      fire note-off                 (end continuous gate after minimum duration)
```

**Key insight:** `joystickTrig_` as a one-shot atomic bool cannot model a continuous gate. The gate stays open as long as motion is above threshold. This requires tracking "still for N samples" per voice.

**Minimum gate duration:**
```cpp
// 50ms minimum gate to prevent jitter artifacts
const int minGateSamples = static_cast<int>(0.050 * p.sampleRate);  // ~2205 at 44.1kHz
```

**Magnitude computation (Claude's discretion — two options):**

Option A: `max(|dx|, |dy|)` — cheaper, no sqrt, no precision issues at audio-thread rate
```cpp
const float magnitude = std::max(std::abs(dx), std::abs(dy));
```

Option B: `sqrt(dx² + dy²)` — geometric magnitude, more intuitive threshold calibration
```cpp
const float magnitude = std::sqrt(dx * dx + dy * dy);
```

Recommendation: Use `max(|dx|, |dy|)` (Chebyshev distance). In a -1..+1 normalized space, max is computationally cheaper and the difference in feel is negligible at audio-thread granularity. The threshold parameter calibration is more intuitive (0.01 means "1% of full deflection in either axis").

**Confidence:** HIGH — logic is deterministic; no library dependency.

### Pattern 2: Per-Voice Subdivision Selector — APVTS Design

**Current state:** One global `randomSubdiv` APVTS AudioParameterChoice at index 0–3. All voices share it.

**User decision:** Per-voice random subdivision (each voice in RND mode has its own).

**Options:**

Option A: 4 separate APVTS params (`randomSubdiv0..3`), 4 combo boxes in UI
- Requires new APVTS params (parameter ID changes = breaking preset change if any presets were saved)
- Currently no presets have been saved (plugin is not yet distributed), so this is safe
- Each voice has full independence

Option B: Reuse single `randomSubdiv` and add 3 more params only if needed
- Simplest if all voices are typically set to the same subdivision

**Recommendation:** Use Option A: 4 separate params `randomSubdiv0`, `randomSubdiv1`, `randomSubdiv2`, `randomSubdiv3`. The existing `randomSubdiv` param was designed with this in mind (the param name is not voice-indexed, which suggests it was a placeholder). Since no presets exist yet, the rename is free. The ProcessParams struct already has `randomSubdiv` as a single value — extend it to `randomSubdiv[4]`.

**Important:** The existing `randomSubdivBox_` in PluginEditor is a single combo box. For per-voice subdivision, the design needs either:
- 4 small combo boxes (one per voice, placed below each trigSrc combo) — matches CONTEXT.md "fits below existing TouchPlate pads without expanding height"
- Or keep it global for Phase 04 and make it per-voice in a future enhancement

Given CONTEXT.md explicitly states "each voice in RND mode has its own subdivision selector" this is locked — 4 per-voice combo boxes are required.

**Confidence:** HIGH — APVTS param addition is straightforward.

### Pattern 3: Random Density Model Change (1–8 hits/bar)

**Current state:**
```cpp
// PluginProcessor.cpp:113
addFloat(ParamID::randomDensity, "Random Density", 0.0f, 1.0f, 0.5f);

// TriggerSystem.cpp:112
if (randomFired && nextRandom() < p.randomDensity)
    trigger = true;
```
The current model: `randomDensity` is a raw probability (0.0–1.0). Every subdivision boundary, generate a random float; fire if it's below the density threshold.

**Required model:** Average hits per bar (1–8). A subdivision fires on average `density` times per bar.

**Conversion:** Hits per bar = (hits per subdivision) × (subdivisions per bar).
Subdivisions per bar for the 4 options:
- 1/4: 4 subdivisions/bar → P(fire) = density / 4
- 1/8: 8 subdivisions/bar → P(fire) = density / 8
- 1/16: 16 subdivisions/bar → P(fire) = density / 16
- 1/32: 32 subdivisions/bar → P(fire) = density / 32

In general: `P(fire) = density / subdivisionsPerBar`

```cpp
// subdivisionsPerBar for each RandomSubdiv:
static int subdivisionsPerBar(RandomSubdiv s)
{
    switch (s) {
        case RandomSubdiv::Quarter:      return 4;
        case RandomSubdiv::Eighth:       return 8;
        case RandomSubdiv::Sixteenth:    return 16;
        case RandomSubdiv::ThirtySecond: return 32;
    }
    return 8;
}

// In processBlock per-voice:
const float prob = p.randomDensity[v] / static_cast<float>(subdivisionsPerBar(p.randomSubdiv[v]));
if (randomFired[v] && nextRandom() < prob)
    trigger = true;
```

**APVTS parameter change required:**
```cpp
// Old (must change):
addFloat(ParamID::randomDensity, "Random Density", 0.0f, 1.0f, 0.5f);

// New:
addFloat(ParamID::randomDensity, "Random Density", 1.0f, 8.0f, 4.0f);
```

Default 4.0 = 4 hits per bar is a musically reasonable default for generative texture.

**Note on parameter range change:** Changing an existing APVTS float parameter's range is a breaking change for any saved presets. Since this is pre-release and no presets have been saved/distributed, this is safe to do.

**Confidence:** HIGH — straightforward probability math.

### Pattern 4: ppqPosition-Based Subdivision Alignment

**Current state:** TriggerSystem uses a simple sample counter (`randomPhase_`) that increments by `blockSize` each block and fires when it crosses `samplesPerSubdiv`. This is a free-running clock that drifts relative to DAW beat position.

**Required:** When transport is playing, align to DAW ppqPosition. When stopped, fall back to wall-clock or sample-counting.

**ppqPosition alignment approach:**

The trick is to detect subdivision boundary crossings using the ppqPosition passed in from the DAW. ProcessParams already receives `bpm` and `ppqPosition` is passed through LooperEngine::ProcessParams — but NOT into TriggerSystem::ProcessParams currently.

**Required addition to ProcessParams:**
```cpp
struct ProcessParams
{
    // ... existing fields ...
    double ppqPosition    = -1.0;  // -1 = transport stopped
    bool   isDawPlaying   = false;
};
```

**Algorithm when playing:**
```cpp
// For voice v with subdivision s:
const double subdivBeats = subdivisionBeats(p.randomSubdiv[v]);
// ppqPhase = fractional position within the subdivision cycle
const double ppqPhase = std::fmod(p.ppqPosition, subdivBeats);
const double ppqPhaseEnd = std::fmod(p.ppqPosition + (p.blockSize / samplesPerBeat), subdivBeats);
// Fired if a boundary was crossed (phase wrapped around)
const bool subdBoundaryCrossed = (ppqPhaseEnd < ppqPhase || ppqPhase < epsilon);
```

A simpler and reliable approach (already used in LooperEngine):
```cpp
// Convert ppqPosition to an integer subdivision index
const int64_t subdivIndex = static_cast<int64_t>(p.ppqPosition / subdivBeats);
// Fire if the index changed since last block
if (subdivIndex != prevSubdivIndex_[v])
{
    prevSubdivIndex_[v] = subdivIndex;
    randomFired[v] = true;
}
```

**When transport is stopped (wall-clock fallback):**
```cpp
// Sample-counting independent of ppq
// randomPhase_ already exists — use it as the fallback when !isDawPlaying
if (!p.isDawPlaying)
{
    randomPhase_[v] += p.blockSize;
    if (randomPhase_[v] >= samplesPerSubdiv[v])
    {
        randomPhase_[v] = std::fmod(randomPhase_[v], samplesPerSubdiv[v]);
        randomFired[v] = true;
    }
}
```

Alternative: Use `juce::Time::getMillisecondCounterHiRes()` for wall-clock fallback. However, this is not recommended from the audio thread because it involves a system call. Sample-counting is the correct audio-thread approach.

**Confidence:** HIGH — ppqPosition alignment is the standard pattern used by every beat-synced JUCE plugin. The `prevSubdivIndex_` approach avoids floating-point drift.

### Pattern 5: Random Gate-Time (Note Duration)

**Current state:** No random gate-time parameter exists. Random-triggered notes are fire-and-forget one-shots — fireNoteOn with no scheduled note-off. The only way they end currently is via retrigger (fireNoteOff immediately before the next fireNoteOn).

**Required:** A configurable gate-time that determines how long each random-triggered note sustains before auto note-off.

**Implementation approach:** Sample countdown per voice.

```cpp
// TriggerSystem private state:
std::array<int, 4> randomGateRemaining_ {0, 0, 0, 0};  // samples until auto note-off

// In processBlock per-voice (RND source):
if (trigger)
{
    if (gateOpen_[v].load())
        fireNoteOff(v, ch, 0, p);
    fireNoteOn(v, p.heldPitches[v], ch, 0, p);
    randomGateRemaining_[v] = static_cast<int>(p.randomGateTime * samplesPerSubdiv[v]);
}

// Auto note-off countdown (after per-voice processing):
if (p.sources[v] == TriggerSource::Random && gateOpen_[v].load())
{
    randomGateRemaining_[v] -= p.blockSize;
    if (randomGateRemaining_[v] <= 0)
        fireNoteOff(v, ch, p.blockSize - 1, p);  // note-off at end of block
}
```

**APVTS parameter:**
```cpp
// Range: 0.0 (shortest practical = 1/32 of subdivision) to 1.0 (full subdivision length)
// Default: 0.5 (half the subdivision length)
addFloat("randomGateTime", "Random Gate Time", 0.0f, 1.0f, 0.5f);
```

The gate duration in samples = `randomGateTime * samplesPerSubdiv`. At `randomGateTime = 1.0`, the note lasts exactly one full subdivision. At `0.0`, it should be a short minimum (e.g., 10ms) rather than 0 to avoid clicking.

**Practical minimum:**
```cpp
const int minDurationSamples = static_cast<int>(0.010 * p.sampleRate);  // 10ms floor
const int gateSamples = std::max(minDurationSamples,
    static_cast<int>(p.randomGateTime * samplesPerSubdiv[v]));
```

**Confidence:** HIGH — sample-countdown is the standard MIDI generator pattern for timed notes.

### Pattern 6: TouchPlate Dimming for Non-PAD Modes

**Current state:** `TouchPlate::paint()` always reads `proc_.isGateOpen(voice_)` for the color, and mouseDown/mouseUp always call `proc_.setPadState()` regardless of mode.

**Required:** When a voice is in JOY or RND mode, the TouchPlate pad dims visually and becomes non-functional.

**Implementation in TouchPlate:**
```cpp
void TouchPlate::mouseDown(const juce::MouseEvent&)
{
    // Only active in PAD mode
    const int src = (int)proc_.apvts.getRawParameterValue(
        "triggerSource" + juce::String(voice_))->load();
    if (src == 0)  // 0 = TouchPlate
    {
        proc_.setPadState(voice_, true);
        repaint();
    }
}
void TouchPlate::mouseUp(const juce::MouseEvent&)
{
    const int src = (int)proc_.apvts.getRawParameterValue(
        "triggerSource" + juce::String(voice_))->load();
    if (src == 0)
        proc_.setPadState(voice_, false);
    repaint();
}

void TouchPlate::paint(juce::Graphics& g)
{
    const int src = (int)proc_.apvts.getRawParameterValue(
        "triggerSource" + juce::String(voice_))->load();
    const bool isPadMode = (src == 0);
    const bool active = isPadMode && proc_.isGateOpen(voice_);

    // Dim the pad in non-pad mode
    const juce::Colour fillClr = isPadMode
        ? (active ? Clr::gateOn : Clr::gateOff)
        : Clr::gateOff.darker(0.3f);  // dimmed inactive appearance

    g.setColour(fillClr);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

    g.setColour(isPadMode ? Clr::text : Clr::textDim);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText(label_, getLocalBounds(), juce::Justification::centred);

    g.setColour((active ? Clr::gateOn : Clr::accent).brighter(isPadMode ? 0.2f : 0.0f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 8.0f, 1.5f);
}
```

**APVTS read from message thread in paint():** `getRawParameterValue()` returns `std::atomic<float>*` — loads are lock-free and safe from any thread. The 30Hz timer calls `repaint()` on pads, so dimming state will update within 33ms of a source mode change.

**Confidence:** HIGH — reading APVTS atomics from the paint thread is established safe practice (confirmed in Phase 03 research and implemented in timerCallback).

### Pattern 7: Random Pitch — Live vs Held

**User decision:** "Pitch at trigger time: Current joystick position at the moment the random trigger fires (live pitch, not sample-and-hold)."

**Current state:** `processBlock` in PluginProcessor pre-computes `freshPitches[4]` from `buildChordParams()` and stores them into `heldPitch_` before calling `trigger_.processBlock(tp)`. TriggerSystem uses `p.heldPitches[v]` for both TouchPlate AND random/joystick triggers.

**Gap:** For TouchPlate triggers, the pitch is sampled at trigger time (correct — sample-and-hold). For random triggers, the user wants the live pitch (current joystick position at the exact moment the random clock fires), which is what `freshPitches[v]` already provides — so as long as `heldPitch_[v]` is always equal to `freshPitches[v]` at the time processBlock runs, this is fine.

Looking at PluginProcessor.cpp:313-314:
```cpp
for (int v = 0; v < 4; ++v)
    heldPitch_[v] = freshPitches[v];  // updates unconditionally every block
```

This means `heldPitch_[v]` IS the live pitch for every block. For random triggers, this is correct — the pitch reflects the current joystick position at the time the block runs.

The actual sample-and-hold for TouchPlate would require NOT updating `heldPitch_[v]` after a pad is pressed. But that is the existing Phase 03 behavior and is marked as already working. The resolution: for RND source, the pitch IS effectively live because `heldPitch_[v]` is updated every block from the current joystick position.

**Conclusion:** No change needed for random pitch behavior — it already works as specified because heldPitch_ is refreshed every processBlock from the current joystick position.

**Confidence:** HIGH — direct code reading confirms this.

### Anti-Patterns to Avoid

- **Calling `std::sqrt` in the audio thread for magnitude** is safe (sqrt is a fast hardware instruction on x86/x64), but using `max(|dx|, |dy|)` is even cheaper and avoids the mathematical approximation discussion entirely.
- **Using `juce::Time::getMillisecondCounterHiRes()` in processBlock** is legal but introduces a system call into the audio thread. Sample-counting is the standard alternative and avoids any OS latency.
- **Changing per-voice randomSubdiv in the middle of a block** could cause a phase discontinuity in the subdivision clock. Since the user can only change this from the message thread (APVTS combo box), and processBlock reads it at the top of the block, this is fine — the worst case is a one-block delay before the new subdivision takes effect.
- **Not resetting randomGateRemaining_[v] in resetAllGates()** would cause ghost note-offs after transport stop or bypass. Must add it to resetAllGates().
- **Not resetting per-voice prevSubdivIndex_[v] when transport restarts** causes a missed first beat when the user presses play. Reset prevSubdivIndex_[v] = -1 whenever `isDawPlaying` transitions from false to true.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| ppqPosition beat counting | Custom beat phase tracker | Read `pos.ppqPosition` from AudioPlayHead in processBlock | Already done — value already flows through; just needs to reach TriggerSystem |
| Wall-clock timing | OS timer or juce::Timer | Sample-count accumulation in processBlock | Sample-counting is audio-thread-safe and exactly as accurate as needed for musical subdivisions |
| Configurable parameter range | Manual float-to-int mapping | AudioParameterFloat with new min/max/default | APVTS handles range validation, UI display, and preset persistence |
| Beat subdivision alignment | Phase-accumulator from scratch | Integer subdivision index derived from ppqPosition/subdivBeats | Avoids floating-point accumulation drift; robust to transport repositioning |

**Key insight:** The hardest part of Phase 04 is the joystick continuous gate model — changing TriggerSystem from a one-shot trigger dispatch to a gate-open/gate-close state machine. Everything else is parameter additions and minor arithmetic changes.

---

## Common Pitfalls

### Pitfall 1: Joystick Gate Fires Every Block When Still Moving

**What goes wrong:** The continuous gate model correctly fires note-on when motion starts. But the condition "motion exceeds threshold" remains true for multiple consecutive blocks as long as the user holds the joystick in motion — which would retrigger every block, not hold the note open.

**Why it happens:** Confusing "magnitude > threshold → fire note-on" with "magnitude > threshold → gate is open."

**How to avoid:** The gate is a state, not an event. Use `gateOpen_[v]` to track state:
```cpp
if (magnitude > threshold && !gateOpen_[v].load())
    fireNoteOn(...)   // only fire on gate OPEN transition
// ... gate stays open until magnitude drops AND min duration met
if (magnitude <= threshold && gateOpen_[v].load() && stillSamples[v] >= minGateSamples)
    fireNoteOff(...)  // fire on gate CLOSE transition
```

**Warning signs:** Rapid-fire MIDI note-ons filling the buffer while joystick is in constant motion.

### Pitfall 2: Joystick Threshold Too Sensitive — Jitter At Rest

**What goes wrong:** Joystick values at rest have small floating-point noise (even with no physical movement, the USB poll can produce ±0.001 variation). With a very low threshold, the gate opens and closes many times per second when the joystick is "at rest."

**Why it happens:** The threshold is set too close to the noise floor of the input source.

**How to avoid:** The 50ms minimum gate duration prevents sub-50ms open/close cycles. Also, the threshold APVTS parameter default should be set conservatively (e.g., 0.02 rather than 0.001).

**Warning signs:** Spurious note-on/off pairs at ~100ms intervals when joystick is untouched.

### Pitfall 3: randomSubdiv Per-Voice Breaks the Shared Clock

**What goes wrong:** The current random clock is a single `randomPhase_` counter shared across all voices. Moving to per-voice subdivision means voices can fire at different rates — the single phase counter is no longer valid.

**Why it happens:** Original design assumed one global subdivision.

**How to avoid:** Change `randomPhase_` to `randomPhase_[4]` (one accumulator per voice, or one per voice for the ppqPosition approach: `prevSubdivIndex_[4]`).

**Warning signs:** Compile error on `randomPhase_` after making the ProcessParams change, or voices misfiring at wrong rates.

### Pitfall 4: randomGateRemaining_ Not Reset on Voice Source Change

**What goes wrong:** Voice 0 is in RND mode, a note is playing, user switches to PAD mode. `randomGateRemaining_[0]` is still counting down and fires a spurious note-off.

**Why it happens:** The gate-time countdown is not cleared when trigger source changes.

**How to avoid:** In TriggerSystem::processBlock(), when `src != TriggerSource::Random` and `randomGateRemaining_[v] > 0`, clear the countdown:
```cpp
if (src != TriggerSource::Random)
    randomGateRemaining_[v] = 0;
```

**Warning signs:** Unexpected note-off shortly after switching a voice from RND to PAD mode.

### Pitfall 5: ppqPosition Jump on Transport Repositioning

**What goes wrong:** User scrubs to a new position in the DAW timeline. `prevSubdivIndex_[v]` was at bar 4, now ppqPosition jumps to bar 1. The index comparison fires immediately (change detected) — OK. But if the jump is to the exact same subdivision index, nothing fires — also OK.

The real problem: `prevSubdivIndex_[v]` holds a stale large value. On the next block, the new smaller index is detected as a change, so one extra trigger fires. This is acceptable for most musical uses, but if precision is required:

**How to avoid:** On transport start or reposition (detect `pos.isPlaying` transitioning from false to true, or by watching for large ppqPosition jumps), reset `prevSubdivIndex_[v] = -1` to force a clean baseline.

**Warning signs:** One spurious extra trigger at transport start or after scrubbing.

### Pitfall 6: APVTS Parameter Addition Causes Plugin State Incompatibility

**What goes wrong:** Adding new parameters (`joystickThreshold`, `randomGateTime`, per-voice `randomSubdiv0..3`) changes the APVTS state tree. Old preset XML will not contain these keys. JUCE's `setStateInformation` uses `apvts.replaceState()` which will silently leave missing params at their default values — this is safe and standard JUCE behavior.

**The actual risk:** If `randomSubdiv` (the old global param) is renamed to `randomSubdiv0..3` (per-voice), any preset saved before this change will no longer restore the subdivision setting. Since the plugin is pre-release with no distributed presets, this is acceptable.

**How to avoid:** Document that existing test presets saved before Phase 04 should be discarded and re-saved after the parameter rename.

**Warning signs:** None visible — JUCE silently uses default values for missing params.

---

## Code Examples

### Joystick Threshold — Continuous Gate Logic in processBlock

```cpp
// TriggerSystem.cpp — replace joystick section of processBlock

// Compute combined joystick magnitude (Chebyshev distance — cheaper than Euclidean)
const float dx = p.joystickX - static_cast<float>(prevJoystickX_);
const float dy = p.joystickY - static_cast<float>(prevJoystickY_);
const float magnitude = std::max(std::abs(dx), std::abs(dy));
prevJoystickX_ = p.joystickX;
prevJoystickY_ = p.joystickY;

const bool joyAboveThreshold = magnitude > p.joystickThreshold;
const int  minGateSamples    = static_cast<int>(0.050 * p.sampleRate);

// Per-voice (inside the voice loop, where src == TriggerSource::Joystick):
if (src == TriggerSource::Joystick)
{
    if (joyAboveThreshold)
    {
        joystickStillSamples_[v] = 0;
        if (!gateOpen_[v].load())
            trigger = true;              // open gate on threshold crossing
    }
    else
    {
        joystickStillSamples_[v] += p.blockSize;
        if (gateOpen_[v].load() && joystickStillSamples_[v] >= minGateSamples)
            fireNoteOff(v, ch, 0, p);   // close gate after 50ms stillness
    }
}
```

### Per-Voice Subdivision Index Clock (ppqPosition-based)

```cpp
// TriggerSystem.h — add to private state:
std::array<int64_t, 4> prevSubdivIndex_ {-1, -1, -1, -1};
std::array<double, 4>  randomPhase_     {0.0, 0.0, 0.0, 0.0};  // fallback

// TriggerSystem.cpp — per-voice random clock check:
bool randomFired[4] = {};
for (int v = 0; v < 4; ++v)
{
    const double subdivBeats     = subdivisionBeats(p.randomSubdiv[v]);
    const double samplesPerBeat  = (p.sampleRate * 60.0) / p.bpm;
    const double samplesPerSubdiv = samplesPerBeat * subdivBeats;

    if (p.isDawPlaying && p.ppqPosition >= 0.0)
    {
        // DAW-synced: detect integer subdivision boundary crossing
        const int64_t idx = static_cast<int64_t>(p.ppqPosition / subdivBeats);
        if (idx != prevSubdivIndex_[v])
        {
            prevSubdivIndex_[v] = idx;
            randomFired[v] = true;
        }
    }
    else
    {
        // Transport stopped: sample-count fallback
        randomPhase_[v] += static_cast<double>(p.blockSize);
        if (samplesPerSubdiv > 0.0 && randomPhase_[v] >= samplesPerSubdiv)
        {
            randomPhase_[v] = std::fmod(randomPhase_[v], samplesPerSubdiv);
            randomFired[v] = true;
        }
    }
}
```

### Density → Probability Conversion

```cpp
// TriggerSystem.cpp — per-voice random trigger decision:
static float hitsPerBarToProbability(float density, RandomSubdiv subdiv)
{
    // density = average hits per bar (1..8)
    // return = probability of firing per subdivision event
    const float subdivsPerBar = static_cast<float>(subdivisionsPerBar(subdiv));
    return juce::jlimit(0.0f, 1.0f, density / subdivsPerBar);
}

// Usage:
if (randomFired[v])
{
    const float prob = hitsPerBarToProbability(p.randomDensity, p.randomSubdiv[v]);
    if (nextRandom() < prob)
        trigger = true;
}
```

### New APVTS Parameters Required

```cpp
// PluginProcessor.cpp — createParameterLayout() additions:

// Replace existing randomDensity (0..1) with (1..8):
addFloat("randomDensity", "Random Density", 1.0f, 8.0f, 4.0f);

// Replace single randomSubdiv with per-voice:
const juce::StringArray subdivChoices { "1/4", "1/8", "1/16", "1/32" };
layout.add(std::make_unique<juce::AudioParameterChoice>("randomSubdiv0", "Random Subdiv Root",    subdivChoices, 1));
layout.add(std::make_unique<juce::AudioParameterChoice>("randomSubdiv1", "Random Subdiv Third",   subdivChoices, 1));
layout.add(std::make_unique<juce::AudioParameterChoice>("randomSubdiv2", "Random Subdiv Fifth",   subdivChoices, 1));
layout.add(std::make_unique<juce::AudioParameterChoice>("randomSubdiv3", "Random Subdiv Tension", subdivChoices, 1));

// New: joystick threshold slider
addFloat("joystickThreshold", "Joystick Threshold", 0.001f, 0.1f, 0.015f);

// New: random gate time (fraction of subdivision)
addFloat("randomGateTime", "Random Gate Time", 0.0f, 1.0f, 0.5f);
```

### ProcessParams Struct Changes (TriggerSystem.h)

```cpp
struct ProcessParams
{
    NoteCallback   onNote;
    int            blockSize        = 512;
    double         sampleRate       = 44100.0;
    double         bpm              = 120.0;

    TriggerSource  sources[4]       = {};
    int            heldPitches[4]   = {};
    int            midiChannels[4]  = {};

    // Per-voice random params (changed from single to array)
    RandomSubdiv   randomSubdiv[4]  = {};
    float          randomDensity    = 4.0f;  // hits per bar (1..8)
    float          randomGateTime   = 0.5f;  // fraction of subdivision

    // Joystick (updated)
    float          joystickX        = 0.0f;
    float          joystickY        = 0.0f;
    float          joystickThreshold = 0.015f;  // new: configurable threshold

    // Timing (new)
    double         ppqPosition      = -1.0;
    bool           isDawPlaying     = false;
};
```

### resetAllGates() — Required Additions

```cpp
void TriggerSystem::resetAllGates()
{
    for (int v = 0; v < 4; ++v)
    {
        gateOpen_[v].store(false);
        activePitch_[v] = -1;
        padPressed_[v].store(false);
        padJustFired_[v].store(false);
        joystickStillSamples_[v] = 0;    // NEW
        randomGateRemaining_[v]  = 0;    // NEW
        prevSubdivIndex_[v]      = -1;   // NEW
        randomPhase_[v]          = 0.0;  // NEW (now per-voice)
    }
    allTrigger_.store(false);
    joystickTrig_.store(false);
}
```

---

## UI Layout Analysis

### Current UI (920×700) — Relevant Right Column

```
RIGHT COLUMN (after split at colW/2 - 4):
  [Joystick Pad] ~padSize × padSize (~342×342)
  [X Range knob] [Y Range knob]          70px tall
  [ROOT] [3RD] [5TH] [TEN]  ←--- TouchPlates, 70px tall
  [Cut Atten knob] [Res Atten knob]      70px tall
  [Gamepad: --]                          16px
  [! Channel conflict]                   16px

LEFT COLUMN:
  [Scale Preset]                         100px
  [Chord intervals: Transpose 3rd 5th Ten]  80px
  [Octave offsets]                       80px
  [Trigger sources section]              90px + 60px random row
  [Looper buttons]                       36px
  [Looper controls]                      60px
```

### New UI Elements Needed

1. **Joystick threshold slider** — labeled THRESHOLD or SENS
   - Type: `juce::Slider::LinearHorizontal` (horizontal slider per user decision)
   - APVTS param: `joystickThreshold`
   - Placement options:
     - Option A: Below the filter attenuator knobs in the right column (currently unused space)
     - Option B: In the left column, near the trigger source selectors section
   - Recommendation: Right column, after filter attenuators (before gamepad status). This keeps all joystick-related controls together.

2. **Per-voice randomSubdiv combo boxes** (4 total)
   - Replace the single `randomSubdivBox_` with `randomSubdivBox_[4]`
   - Placement: Below the trigSrc_[4] row (currently uses 22px of the 90px trigger section)
   - The existing randomDensity knob and single randomSubdiv box use a 60px row — this row should become 4 per-voice subdiv combo boxes

3. **Random gate-time knob**
   - Type: Rotary knob (matches existing style)
   - APVTS param: `randomGateTime`
   - Placement: Adjacent to randomDensity knob in the left column random section

4. **TouchPlate dimming**
   - No new UI elements — handled in TouchPlate::paint() and mouseDown/mouseUp logic
   - The 30Hz timer already calls repaint() on all pads, so dimming updates within 33ms

---

## TriggerSystem Private State Changes Summary

New members needed in TriggerSystem.h:

```cpp
// Replace single per-block fields with per-voice arrays:
std::array<double, 4>   randomPhase_          {0.0, 0.0, 0.0, 0.0};  // was single double
std::array<int64_t, 4>  prevSubdivIndex_      {-1, -1, -1, -1};      // new: ppq sync
std::array<int, 4>      joystickStillSamples_ {};                      // new: threshold gate
std::array<int, 4>      randomGateRemaining_  {};                      // new: auto note-off

// Remove:
// double randomPhase_  (replaced by array)
// prevJoystickX_/prevJoystickY_ remain (still needed for delta computation)
// joystickTrig_ atomic may be removed (replaced by inline magnitude comparison in processBlock)
```

---

## Open Questions

1. **Per-voice randomSubdiv: UI layout concern**
   - What we know: User decision is explicit — per-voice subdivision is required. Existing `randomSubdivBox_` (single combo) is in the left column trigger section occupying a 60px row.
   - What's unclear: Do 4 small per-voice subdiv combos fit below the 4 trigSrc combos without expanding the editor height? Each combo is ~80px wide × 22px tall. At the current left-column width (~446px), 4 combo boxes of 111px each fit in one row.
   - Recommendation: Replace the single `randomSubdivBox_` with `randomSubdivBox_[4]`. Place them in a row below `trigSrc_[4]`, each column-aligned with its voice. Keep the label row compressed. The existing 90px trigger section should expand to ~120px to accommodate.

2. **APVTS parameter rename: `randomSubdiv` → `randomSubdiv0..3`**
   - What we know: The original `randomSubdiv` APVTS param was declared as `randomSubdiv` (no voice index). Renaming it would orphan any saved session state. Pre-release, no sessions have been distributed.
   - What's unclear: Are there any unit tests that reference `randomSubdiv` by string? Check tests/ directory.
   - Recommendation: Do a grep of `randomSubdiv` in tests/ before implementing. If tests use it, update them in the plan.

3. **`joystickTrig_` atomic: keep or remove?**
   - What we know: The current `joystickTrig_` atomic is set from both `notifyJoystickMoved()` (UI/gamepad thread) and the processBlock delta check. With the new continuous gate model, the in-processBlock delta check replaces the joystickTrig_ for audio-thread purposes.
   - What's unclear: Is `notifyJoystickMoved()` called from anywhere in the current codebase? If not, it can be removed or kept as a dead stub.
   - Recommendation: Keep the stub as it may be used in Phase 06 (gamepad). Clean it up then.

4. **Transport-stopped random gate: always running or only when plugin is loaded?**
   - What we know: processBlock runs whenever the DAW has the plugin active, whether transport is playing or not.
   - What's unclear: Does the DAW call processBlock when transport is stopped? Behavior varies by DAW.
   - Confirmed (from project history): Reaper calls processBlock even with transport stopped. The sample-count fallback in TriggerSystem will work correctly in Reaper.
   - Recommendation: Implement sample-count fallback and document that the "always-on generative" behavior depends on the DAW continuing to call processBlock with transport stopped.

---

## Phase 04 Plan Structure Recommendation

### Plan 04-01: Joystick Motion Trigger

**Scope:** New APVTS parameter, threshold slider, continuous gate model, 50ms minimum, pad dimming.

Work items:
1. Add `joystickThreshold` APVTS param (float 0.001..0.1, default 0.015)
2. Add `joystickStillSamples_[4]` to TriggerSystem private state
3. Replace joystick edge-trigger logic in processBlock with continuous gate model (threshold comparison, open on crossing, close after 50ms stillness)
4. Add `joystickThreshold` to ProcessParams struct
5. Pass `joystickThreshold` from APVTS into ProcessParams in PluginProcessor::processBlock
6. Update `resetAllGates()` to clear `joystickStillSamples_[4]`
7. Add joystick threshold slider to PluginEditor (horizontal, labeled THRESHOLD, right column)
8. Update `TouchPlate::paint()` to dim when triggerSource != 0 (PAD)
9. Update `TouchPlate::mouseDown/mouseUp` to no-op when triggerSource != 0

### Plan 04-02: Random Gate

**Scope:** Density model change, per-voice subdivision, ppqPosition sync, gate-time knob.

Work items:
1. Change `randomDensity` APVTS param range from 0..1 to 1..8, default 4.0
2. Add `randomSubdiv0..3` APVTS params; remove/replace `randomSubdiv`
3. Add `randomGateTime` APVTS param (float 0..1, default 0.5)
4. Change `randomPhase_` (single double) to `randomPhase_[4]` in TriggerSystem
5. Add `prevSubdivIndex_[4]` and `randomGateRemaining_[4]` to TriggerSystem
6. Update ProcessParams: `randomSubdiv[4]`, `ppqPosition`, `isDawPlaying`, `randomGateTime`
7. Implement ppqPosition subdivision alignment + wall-clock fallback in processBlock
8. Implement density → probability conversion (hitsPerBarToProbability)
9. Implement randomGateRemaining_ countdown with auto note-off
10. Update resetAllGates() for new arrays
11. Update PluginProcessor::processBlock to populate new ProcessParams fields
12. Update PluginEditor: replace single randomSubdivBox_ with randomSubdivBox_[4], add randomGateTime knob, update layout

---

## Sources

### Primary (HIGH confidence — direct codebase reading)

- `Source/TriggerSystem.h` — ProcessParams struct, private state, API surface
- `Source/TriggerSystem.cpp` — full processBlock implementation, subdivision clock, joystick delta logic, random probability check
- `Source/PluginProcessor.cpp` — APVTS parameter layout, ProcessParams population, ppqPosition/bpm passthrough
- `Source/PluginProcessor.h` — joystickX/Y atomics, setPadState, isGateOpen API
- `Source/PluginEditor.h` — existing UI members (trigSrc_[4], randomDensityKnob_, randomSubdivBox_)
- `Source/PluginEditor.cpp` — resized() layout, timerCallback(), TouchPlate paint/mouse
- `Source/GamepadInput.h` — confirms joystick values available as atomics from timer thread
- `.planning/phases/04-per-voice-trigger-sources-and-random-gate/04-CONTEXT.md` — locked decisions and deferred scope

### Secondary (MEDIUM confidence — JUCE API from training, stable API)

- JUCE 8.0.4 `AudioPlayHead::CurrentPositionInfo::ppqPosition` — documented as beats since session start, double precision
- JUCE 8.0.4 `juce::Time::getMillisecondCounterHiRes()` — wall-clock milliseconds, but not used (sample-counting preferred)
- JUCE 8.0.4 `AudioParameterFloat` NormalisableRange — adding new params with changed ranges

### Tertiary (LOW confidence)

- Whether all target DAWs (Reaper, Ableton) call processBlock continuously when transport is stopped — Reaper confirmed (project history); Ableton unconfirmed (still blocked on instantiation crash)

---

## Metadata

**Confidence breakdown:**
- Codebase gap analysis: HIGH — all source files read directly
- TriggerSystem logic changes: HIGH — deterministic state machine patterns
- APVTS parameter additions: HIGH — same pattern used throughout the codebase
- ppqPosition alignment: HIGH — standard JUCE beat-sync pattern
- UI layout: MEDIUM — fitting 4 per-voice subdiv combos requires layout adjustment, exact pixel measurements need testing
- DAW transport-stopped behavior: LOW for Ableton (crash blocker), HIGH for Reaper (confirmed)

**Research date:** 2026-02-22
**Valid until:** 2026-03-22 (JUCE 8.0.4 pinned; no API churn expected; Ableton crash remains an active blocker but does not block Phase 04 implementation)
