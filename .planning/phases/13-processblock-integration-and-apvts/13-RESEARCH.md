# Phase 13: processBlock Integration and APVTS — Research

**Researched:** 2026-02-26
**Domain:** JUCE APVTS parameter wiring + audio-thread LFO injection in C++17
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Phase Boundary:** Wire `LfoEngine` into `PluginProcessor`: 14 APVTS parameters, `process()` call in
`processBlock`, LFO output applied in `buildChordParams()`, phase reset on DAW
transport start. No UI in this phase — that is Phase 14.

**LFO Mixing Model:**
- Formula: `effective_axis = clamp(joystick_pos + lfo_output * level, -1.0f, 1.0f)`
- LFO is an additive offset; joystick position is the center of oscillation.
- Level is per-axis: separate depth parameter for X-LFO and Y-LFO.
- At level = 0: LFO is fully bypassed — no phase accumulation, no CPU cost. Phase does
  NOT advance while bypassed; re-enabling starts from phase 0 (or synced position if
  DAW sync is active).

**Boundary / Clamping Behavior:**
- Hard clamp at ±1 — symmetric on both edges.
- No soft saturation or wrapping.
- Level range is 0.0 to 2.0 — over-modulation is allowed; clamping keeps output in range.
- Disabling the LFO mid-phrase: output ramps to raw joystick value over ~10 ms (linear
  ramp sufficient). No hard snap; no audible click or pitch jump.

**Sample-and-Hold + Trigger Interaction:**
- S&H captures the post-LFO modulated value at trigger time — not the raw joystick.
- After trigger, pitch freezes at the sampled value. LFO does NOT modulate the
  sustained note.
- Applies uniformly to all trigger sources: joystick-trigger, random, and touchplate.

**APVTS Parameter Specifications (14 parameters):**

| Parameter      | Range         | Default  | Notes                                       |
|----------------|---------------|----------|---------------------------------------------|
| lfoXEnabled    | bool          | false    | Bypass at false — no phase advance          |
| lfoYEnabled    | bool          | false    | Bypass at false — no phase advance          |
| lfoXWaveform   | 0–6 (enum)    | 0 (Sine) | Sine/Tri/SawUp/SawDown/Square/S&H/Random   |
| lfoYWaveform   | 0–6 (enum)    | 0 (Sine) |                                             |
| lfoXRate       | 0.01–20.0 Hz  | 1.0 Hz   | Free mode only                              |
| lfoYRate       | 0.01–20.0 Hz  | 1.0 Hz   |                                             |
| lfoXLevel      | 0.0–2.0       | 0.0      | 0 = no effect; 1.0 = full axis sweep        |
| lfoYLevel      | 0.0–2.0       | 0.0      |                                             |
| lfoXPhase      | 0.0–360.0 deg | 0.0      | Displayed in degrees in UI (Phase 14)       |
| lfoYPhase      | 0.0–360.0 deg | 0.0      |                                             |
| lfoXDistortion | 0.0–1.0       | 0.0      | Subtle drive/shaping only                   |
| lfoYDistortion | 0.0–1.0       | 0.0      |                                             |
| lfoXSync       | bool          | false    | Sync to DAW beat                            |
| lfoYSync       | bool          | false    |                                             |

**Preset compatibility:** New parameters default to `enabled=false, level=0` — loading a
v1.3 preset produces identical behavior to before (no modulation).

### Claude's Discretion

- Beat-sync subdivision options — pick standard musical subdivisions.
- Internal parameter ID naming convention — match existing APVTS style in PluginProcessor.
- Ramp duration for LFO disable transition: ~10 ms target; exact sample count is
  implementation detail.
- Whether `lfoXSync` / `lfoYSync` replace or override the rate parameter — implement
  whichever is cleaner given LfoEngine's existing beat detection API.

### Deferred Ideas (OUT OF SCOPE)

- LFO visualization / waveform preview in UI → Phase 14
- Per-voice LFO routing (different LFO settings per voice) → possible future phase
- Stereo LFO (independent L/R phase offset) → not applicable; MIDI effect only
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| LFO-01 | User can enable/disable X-axis LFO independently (On/Off toggle) | APVTS bool `lfoXEnabled`; bypass path in processBlock skips LfoEngine::process() when false |
| LFO-02 | User can enable/disable Y-axis LFO independently (On/Off toggle) | APVTS bool `lfoYEnabled`; same bypass pattern as LFO-01 |
| LFO-08 | User can sync LFO to tempo via Sync button — follows free BPM or DAW BPM | `lfoXSync`/`lfoYSync` bool params; LfoEngine ProcessParams already has syncMode, ppqPosition, isDawPlaying fields |
| LFO-09 | LFO X output modulates joystick X axis as additive offset (clamped to -1..1) | Apply after `buildChordParams()`, before TriggerSystem delta; clamp with std::clamp |
| LFO-10 | LFO Y output modulates joystick Y axis as additive offset (clamped to -1..1) | Same injection point as LFO-09 but on Y axis |
</phase_requirements>

---

## Summary

Phase 13 has a single clear task: wire `LfoEngine` (built and tested in Phase 12) into
`PluginProcessor`. The work is entirely in `PluginProcessor.h` and `PluginProcessor.cpp`
— no new files, no UI changes. The LfoEngine API is already finalized and tested with 15
passing test cases.

The injection point is immediately after `buildChordParams()` returns and before
`freshPitches` are computed. The existing `chordP` struct is not const — it can be
modified in-place with the LFO additive offset and clamp. The delta tracking
(`prevJoyX_` / `prevJoyY_`) must use the post-LFO values so joystick-source triggers
respond to LFO motion correctly (this behavior is intentional per STATE.md).

Preset compatibility is automatic: APVTS assigns default values to new parameters when
loading an older preset that doesn't contain them. The defaults `enabled=false, level=0`
guarantee zero behavior change when opening a v1.3 preset.

**Primary recommendation:** Add two `LfoEngine` members to `PluginProcessor`, add 14
APVTS parameters in `createParameterLayout()`, call `lfoX_.process()` / `lfoY_.process()`
inside `processBlock()` immediately after `buildChordParams()`, apply clamped additive
offsets to `chordP.joystickX` / `chordP.joystickY`, and call `lfoX_.reset()` /
`lfoY_.reset()` in `prepareToPlay()` and on transport stop.

---

## Standard Stack

### Core

| Component | Version | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| LfoEngine | Phase 12 | LFO DSP computation | Already built, 15 tests passing; self-contained, no JUCE headers |
| JUCE APVTS | JUCE 8.0.4 | Parameter storage, DAW automation, preset serialization | Already in use; `getRawParameterValue()` is audio-thread safe |
| `AudioParameterBool` | JUCE 8.0.4 | Boolean APVTS parameters (enabled, sync) | Already used in project (e.g., `useCustomScale`, `randomClockSync`) |
| `AudioParameterFloat` | JUCE 8.0.4 | Float APVTS parameters (rate, level, phase, distortion) | Already used for `joystickXAtten`, `filterXAtten`, etc. |
| `AudioParameterChoice` | JUCE 8.0.4 | Enum APVTS parameters (waveform) | Already used for `triggerSource0..3`, `looperSubdiv`, `arpOrder` |

### No New Dependencies

This phase introduces zero new libraries. Everything required already exists in the project.

---

## Architecture Patterns

### Existing processBlock Flow (Current State)

```
processBlock()
  1. Feed gamepad threshold
  2. Poll gamepad triggers
  3. Extract DAW position (ppqPos, dawBpm, isDawPlaying)
  4. looper_.process(lp)                        ← sets joystickX/Y if looper playing
  5. buildChordParams()                          ← reads joystickX/Y atomics
  6. Compute freshPitches[4] via ChordEngine     ← uses chordP.joystickX/Y
  7. TriggerSystem.processBlock()                ← uses chordP.joystickX/Y + deltaX/Y
  8. Arpeggiator
  9. Random trigger
  10. Joystick-move → looper rec activation
  11. Looper joystick + filter recording
  12. Filter CC output
```

### New processBlock Flow (Phase 13 Target)

```
processBlock()
  1-4. [unchanged]
  5. buildChordParams()                          ← reads joystickX/Y atomics
  NEW: 5a. lfoX_.process(xParams) → lfoXOut    ← only if lfoXEnabled && lfoXLevel > 0
  NEW: 5b. lfoY_.process(yParams) → lfoYOut    ← only if lfoYEnabled && lfoYLevel > 0
  NEW: 5c. apply: chordP.joystickX = clamp(chordP.joystickX + lfoXOut, -1, 1)
  NEW: 5d. apply: chordP.joystickY = clamp(chordP.joystickY + lfoYOut, -1, 1)
  NEW: chordP is now non-const (or use a mutable local copy)
  6. Compute freshPitches[4]                     ← now uses LFO-modulated values
  7. TriggerSystem (tp.joystickX/Y from chordP) ← deltaX/Y reflect LFO motion
  8-12. [unchanged]
```

### Pattern 1: LFO Member Declaration in PluginProcessor.h

```cpp
// In PluginProcessor.h private section:
#include "LfoEngine.h"

// ...
private:
    ChordEngine   chord_;
    TriggerSystem trigger_;
    LooperEngine  looper_;
    GamepadInput  gamepad_;
    LfoEngine     lfoX_;       // X-axis LFO (fifth/tension voices)
    LfoEngine     lfoY_;       // Y-axis LFO (root/third voices)
```

### Pattern 2: 14 APVTS Parameters in createParameterLayout()

Follow the exact style already used in `PluginProcessor.cpp`. Existing parameter ID
naming uses camelCase (e.g., `joystickXAtten`, `randomClockSync`, `filterXAtten`).
Use the same convention.

```cpp
// In createParameterLayout(), following the existing ── Quantize section:

// ── LFO ───────────────────────────────────────────────────────────────────
// Namespace additions in ParamID:
// lfoXEnabled, lfoYEnabled, lfoXWaveform, lfoYWaveform,
// lfoXRate, lfoYRate, lfoXLevel, lfoYLevel,
// lfoXPhase, lfoYPhase, lfoXDistortion, lfoYDistortion,
// lfoXSync, lfoYSync

addBool("lfoXEnabled",    "LFO X Enabled",    false);
addBool("lfoYEnabled",    "LFO Y Enabled",    false);

{
    juce::StringArray waveforms { "Sine", "Triangle", "Saw Up", "Saw Down",
                                  "Square", "S&H", "Random" };
    addChoice("lfoXWaveform", "LFO X Waveform", waveforms, 0);
    addChoice("lfoYWaveform", "LFO Y Waveform", waveforms, 0);
}

// Rate: NormalisableRange with skew factor for log-scale display
// Use skewFactorFromMidPoint(1.0f) so the slider midpoint = 1 Hz
layout.add(std::make_unique<juce::AudioParameterFloat>(
    "lfoXRate", "LFO X Rate",
    juce::NormalisableRange<float>(0.01f, 20.0f, 0.0f,
        juce::NormalisableRange<float>(0.01f, 20.0f).getSkewFactorFromMidPoint(1.0f)),
    1.0f));
layout.add(std::make_unique<juce::AudioParameterFloat>(
    "lfoYRate", "LFO Y Rate",
    juce::NormalisableRange<float>(0.01f, 20.0f, 0.0f,
        juce::NormalisableRange<float>(0.01f, 20.0f).getSkewFactorFromMidPoint(1.0f)),
    1.0f));

addFloat("lfoXLevel",      "LFO X Level",      0.0f, 2.0f, 0.0f);
addFloat("lfoYLevel",      "LFO Y Level",      0.0f, 2.0f, 0.0f);
addFloat("lfoXPhase",      "LFO X Phase",      0.0f, 360.0f, 0.0f);
addFloat("lfoYPhase",      "LFO Y Phase",      0.0f, 360.0f, 0.0f);
addFloat("lfoXDistortion", "LFO X Distortion", 0.0f, 1.0f, 0.0f);
addFloat("lfoYDistortion", "LFO Y Distortion", 0.0f, 1.0f, 0.0f);
addBool("lfoXSync",        "LFO X Sync",       false);
addBool("lfoYSync",        "LFO Y Sync",       false);
```

### Pattern 3: Beat-Sync Subdivision (Claude's Discretion)

LfoEngine's `ProcessParams::subdivBeats` uses beats (quarter-notes = 1.0). The choices
follow the REQUIREMENTS note ("1/1, 1/2, 1/4, 1/8, 1/16, 1/32"). Dotted and triplet
are deferred (LFO-EXT-02). Recommended implementation:

```cpp
// Subdivision mapping (beats per cycle, quarter-note = 1.0 beat)
// 1/1=4.0, 1/2=2.0, 1/4=1.0, 1/8=0.5, 1/16=0.25, 1/32=0.125
static const double kLfoSubdivBeats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };

// As an APVTS AudioParameterChoice (no separate parameter needed — lfoXSync=true
// means the rate knob's displayed value is ignored and subdivBeats is used):
// Option A (simpler, Claude's discretion): store subdiv choice as separate param:
addChoice("lfoXSubdiv", "LFO X Subdiv",
    { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 2);  // default 1/4
addChoice("lfoYSubdiv", "LFO Y Subdiv",
    { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 2);
```

This makes the total APVTS parameter count **16 new parameters** (14 + 2 subdiv choices).
If the planner prefers to keep strictly to 14, the subdiv could be embedded as an int
param or deferred to Phase 14. The separate choice is cleaner and enables Phase 14 UI
to bind directly.

### Pattern 4: LFO Process Call in processBlock()

```cpp
// After: const ChordEngine::Params chordP = buildChordParams();
// CHANGE chordP to non-const: ChordEngine::Params chordP = buildChordParams();

// ── LFO modulation ─────────────────────────────────────────────────────────
const bool lfoXEnabled = *apvts.getRawParameterValue("lfoXEnabled") > 0.5f;
const bool lfoYEnabled = *apvts.getRawParameterValue("lfoYEnabled") > 0.5f;
const float lfoXLevel  = *apvts.getRawParameterValue("lfoXLevel");
const float lfoYLevel  = *apvts.getRawParameterValue("lfoYLevel");

if (lfoXEnabled && lfoXLevel > 0.0f)
{
    ProcessParams xp;
    xp.sampleRate   = sampleRate_;
    xp.blockSize    = blockSize;
    xp.bpm          = lp.bpm;           // same effective BPM as looper
    xp.syncMode     = *apvts.getRawParameterValue("lfoXSync") > 0.5f;
    xp.ppqPosition  = ppqPos;
    xp.isDawPlaying = isDawPlaying;
    xp.rateHz       = *apvts.getRawParameterValue("lfoXRate");
    xp.subdivBeats  = kLfoSubdivBeats[(int)*apvts.getRawParameterValue("lfoXSubdiv")];
    xp.maxCycleBeats = 16.0;            // conservative max; refine in Phase 14 if needed
    xp.waveform     = static_cast<Waveform>(
        (int)*apvts.getRawParameterValue("lfoXWaveform"));
    xp.phaseShift   = *apvts.getRawParameterValue("lfoXPhase") / 360.0f;
    xp.distortion   = *apvts.getRawParameterValue("lfoXDistortion");
    xp.level        = lfoXLevel;

    const float lfoOut = lfoX_.process(xp);
    chordP.joystickX = std::clamp(chordP.joystickX + lfoOut, -1.0f, 1.0f);
}

if (lfoYEnabled && lfoYLevel > 0.0f)
{
    ProcessParams yp;
    yp.sampleRate   = sampleRate_;
    yp.blockSize    = blockSize;
    yp.bpm          = lp.bpm;
    yp.syncMode     = *apvts.getRawParameterValue("lfoYSync") > 0.5f;
    yp.ppqPosition  = ppqPos;
    yp.isDawPlaying = isDawPlaying;
    yp.rateHz       = *apvts.getRawParameterValue("lfoYRate");
    yp.subdivBeats  = kLfoSubdivBeats[(int)*apvts.getRawParameterValue("lfoYSubdiv")];
    yp.maxCycleBeats = 16.0;
    yp.waveform     = static_cast<Waveform>(
        (int)*apvts.getRawParameterValue("lfoYWaveform"));
    yp.phaseShift   = *apvts.getRawParameterValue("lfoYPhase") / 360.0f;
    yp.distortion   = *apvts.getRawParameterValue("lfoYDistortion");
    yp.level        = lfoYLevel;

    const float lfoOut = lfoY_.process(yp);
    chordP.joystickY = std::clamp(chordP.joystickY + lfoOut, -1.0f, 1.0f);
}
// ─────────────────────────────── end LFO ────────────────────────────────────

// Existing code continues unchanged:
// Compute & sample-hold pitches
int freshPitches[4];
for (int v = 0; v < 4; ++v)
    freshPitches[v] = ChordEngine::computePitch(v, chordP);
```

**Critical:** `chordP` is declared as `const` in current code. The planner must change
the declaration to non-const before adding the LFO lines.

### Pattern 5: prepareToPlay() and Transport Stop Reset

```cpp
void PluginProcessor::prepareToPlay(double sr, int /*blockSize*/)
{
    sampleRate_ = sr;
    lfoX_.reset();   // add these two lines
    lfoY_.reset();
}
```

For transport stop: the existing DAW-stop handler at line ~500 fires `trigger_.resetAllGates()`.
Add LFO resets here:

```cpp
if (justStopped)
{
    for (int v = 0; v < 4; ++v)
        midi.addEvent(juce::MidiMessage::allNotesOff(voiceChs[v]), 0);
    trigger_.resetAllGates();
    lfoX_.reset();   // add — resets phase to 0 on transport stop
    lfoY_.reset();   // add
}
```

Also reset on transport start (DAW restart detection):
```cpp
// After: dawJustStarted = !prevIsDawPlaying_ && isDawPlaying;
if (dawJustStarted)
{
    lfoX_.reset();   // phase aligns to beat 0 when DAW starts
    lfoY_.reset();
}
```

Note: LfoEngine::process() already detects the `false → true` transition on
`isDawPlaying` and resets `sampleCount_` internally. The explicit `reset()` call in
the DAW start handler ensures `phase_` and `totalCycles_` also reset (not just
`sampleCount_`), which is the correct full state reset.

### Pattern 6: Disable Ramp (~10 ms)

The CONTEXT.md specifies a ~10 ms linear ramp when LFO is disabled mid-phrase. This
prevents a hard snap from modulated-position back to raw joystick. Implementation:

```cpp
// Add to PluginProcessor private section:
float lfoXRampOut_  = 0.0f;  // current ramped LFO X output (audio thread only)
float lfoYRampOut_  = 0.0f;  // current ramped LFO Y output (audio thread only)

// In processBlock(), replace the direct lfoOut application with:
const float lfoXTarget = (lfoXEnabled && lfoXLevel > 0.0f) ? lfoX_.process(xp) : 0.0f;
const float lfoYTarget = (lfoYEnabled && lfoYLevel > 0.0f) ? lfoY_.process(yp) : 0.0f;

// Ramp toward target at ~10 ms rate
const float rampCoeff = 1.0f / std::max(1, (int)(sampleRate_ * 0.01f));  // 10 ms in samples, 1 block
lfoXRampOut_ += (lfoXTarget - lfoXRampOut_) * rampCoeff * blockSize;
lfoYRampOut_ += (lfoYTarget - lfoYRampOut_) * rampCoeff * blockSize;

chordP.joystickX = std::clamp(chordP.joystickX + lfoXRampOut_, -1.0f, 1.0f);
chordP.joystickY = std::clamp(chordP.joystickY + lfoYRampOut_, -1.0f, 1.0f);
```

**Simpler alternative (also valid per CONTEXT):** Since we process one output value per
block (not per sample), the "ramp" is inherently block-level. A per-block coefficient of
`blockSize / (sampleRate * 0.01)` steps toward zero over ~10 ms. At 512 samples /
44100 Hz = ~11.6 ms/block, a single block is already within the target window — meaning
a one-block ramp-to-zero is sufficient and acceptable.

### Pattern 7: Preset Compatibility (Automatic via APVTS)

```cpp
void PluginProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    // No additional code needed: APVTS uses parameter defaults for any
    // attributes missing from the XML (v1.3 presets have no LFO keys).
    // Default: enabled=false, level=0 → zero modulation.
}
```

APVTS's `replaceState()` preserves default values for parameters absent in the loaded
XML. This is documented JUCE behavior and is the existing pattern used by this project.

### Anti-Patterns to Avoid

- **Writing LFO output back to `joystickX`/`joystickY` atomics:** The CONTEXT and
  REQUIREMENTS explicitly prohibit this (PERF-02). It would corrupt the looper's
  position recording and create a feedback loop. Apply LFO as a local offset to `chordP`
  only.
- **Declaring `lfoX_`/`lfoY_` without including `LfoEngine.h` in PluginProcessor.h:**
  Forward declaration is insufficient — `LfoEngine` must be a complete type for member
  storage. Add `#include "LfoEngine.h"` at the top of `PluginProcessor.h`.
- **Calling `lfoX_.reset()` from the message thread:** `LfoEngine::reset()` is NOT
  safe concurrent with `process()` (documented in the header). Reset only from
  `prepareToPlay()` or from within `processBlock()` (audio thread).
- **Using `apvts.getParameter("lfoXEnabled")->getValue()` in processBlock:** Use
  `apvts.getRawParameterValue("lfoXEnabled")->load()` — same pattern used throughout
  the existing `processBlock()`.
- **Forgetting to make `chordP` non-const:** Current code: `const ChordEngine::Params chordP = buildChordParams();`. LFO injection requires mutating `chordP.joystickX/Y`. Change to `ChordEngine::Params chordP = buildChordParams();`.
- **Applying LFO before the looper overrides joystickX/Y:** The looper stores and
  plays back raw joystick positions. LFO must be applied AFTER loopOut overrides
  `joystickX`/`joystickY`, which is AFTER `buildChordParams()` runs. The existing
  flow already does this correctly if LFO injection happens after `buildChordParams()`.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| LFO DSP | Custom accumulator in processBlock | `LfoEngine::process()` | Already built, tested (15 tests, 2222 assertions); handles sync, S&H, Random, distortion |
| DAW position extraction | Direct cast of PlayHead | Existing pattern in processBlock (lines ~443-453) | JUCE 8 Optional API already in use; copy the same `getPosition()` block |
| APVTS preset defaults | Version flag / migration code | APVTS default values system | APVTS already assigns parameter defaults for keys absent in loaded XML |
| Audio-thread-safe APVTS reads | Custom atomic cache | `getRawParameterValue("id")->load()` | Already used for every existing parameter; documented JUCE pattern |

---

## Common Pitfalls

### Pitfall 1: `const chordP` Blocks LFO Injection

**What goes wrong:** Current code declares `const ChordEngine::Params chordP = buildChordParams();`. Trying to assign `chordP.joystickX = ...` fails to compile.

**Why it happens:** The const declaration was correct before LFO — pitch computation
was read-only after the fact.

**How to avoid:** Change declaration to `ChordEngine::Params chordP = buildChordParams();` in processBlock.

**Warning signs:** Compiler error: "assignment of member '...' in read-only object".

---

### Pitfall 2: LFO Phase Not Resetting on DAW Transport Start

**What goes wrong:** LFO phase stays at whatever position it was at when transport
stopped. When user presses Play again, LFO does not start at phase 0.

**Why it happens:** `LfoEngine::process()` detects the `false→true` transition on
`isDawPlaying` and only resets `sampleCount_` (the stopped-fallback counter). The
accumulator `phase_` and `totalCycles_` persist.

**How to avoid:** Call `lfoX_.reset()` / `lfoY_.reset()` in the existing `dawJustStarted`
handler in processBlock (around line 497). The `reset()` function sets ALL state fields
to zero.

**Warning signs:** LFO appears out of phase with the beat after transport restart;
varies between sessions.

---

### Pitfall 3: LFO Motion Triggers Joystick-Source Voices Continuously

**What goes wrong:** With LFO active and voices set to Joystick trigger source,
the LFO motion causes continuous delta detection and note-on events at LFO frequency,
flooding the MIDI output.

**Why it happens:** `tp.deltaX = chordP.joystickX - prevJoyX_` — when LFO is
modulating X, the delta is non-zero every block. TriggerSystem fires note-on whenever
delta exceeds threshold for a Joystick-source voice.

**How to avoid:** This behavior is intentional — "LFO drives pitch at next trigger" is
the design for all sources. However, the planner should document this in the plan as
expected behavior, not a bug. The STATE.md concern ("Confirm this is desired") should be
resolved: YES, joystick-source voices do retrigger on LFO motion. This gives the user
an arpeggiated-by-LFO effect. Users who want sustained notes should use pad-trigger mode.

**Warning signs:** Unintended retriggering that the user thinks is a bug — mitigate by
noting in release notes that X-axis LFO + Joystick trigger source = continuous retriggering
at LFO rate.

---

### Pitfall 4: `lp.bpm` Conflicts with LFO Sync Source

**What goes wrong:** LFO sync uses `lp.bpm` (the looper's effective BPM). When DAW sync
is OFF and the looper is in free-tempo mode, `lp.bpm` is sourced from `randomFreeTempo`
APVTS parameter. This is the same free tempo knob as the random trigger system uses —
not a dedicated LFO tempo source.

**Why it happens:** CONTEXT.md says "syncs to Free BPM; when DAW sync is also active,
follows DAW BPM" — this maps exactly to the existing looper BPM logic at line 461.

**How to avoid:** Reuse `lp.bpm` directly in LFO ProcessParams. No new BPM parameter
needed for Phase 13. The shared free tempo is intentional for consistent behavior.

**Warning signs:** LFO running at different speed than random trigger system in free mode
— would only happen if a separate BPM source were erroneously used.

---

### Pitfall 5: Phase Shift Unit Mismatch

**What goes wrong:** LfoEngine accepts `phaseShift` in range [0.0, 1.0] (normalized).
APVTS parameter `lfoXPhase` stores degrees [0.0, 360.0]. Forgetting to divide by 360.

**Why it happens:** The CONTEXT.md states "Displayed in degrees in UI (Phase 14)" —
the underlying engine uses normalized phase for simpler math.

**How to avoid:** Apply the conversion in processBlock:
`xp.phaseShift = *apvts.getRawParameterValue("lfoXPhase") / 360.0f;`

**Warning signs:** Phase shift appears to have very narrow range (0.0 to 1.0 out of 360 degrees usable range).

---

### Pitfall 6: Missing `#include "LfoEngine.h"` in PluginProcessor.h

**What goes wrong:** Build fails with "use of undefined type 'LfoEngine'" or
"incomplete type is not allowed".

**Why it happens:** Forward declaration `class LfoEngine;` is insufficient for storing
member instances (not pointers). The complete type is needed.

**How to avoid:** Add `#include "LfoEngine.h"` in `PluginProcessor.h` alongside the
existing includes. `LfoEngine.h` has no JUCE dependencies (only `<cmath>` and
`<cstdint>`), so inclusion is safe.

---

### Pitfall 7: APVTS Parameter Naming Collision

**What goes wrong:** Adding a parameter with an ID that conflicts with an existing one.
JUCE logs an assertion failure in debug mode; in release, behavior is undefined.

**Why it happens:** The ParamID namespace currently has ~40 parameters. Adding 16 new
LFO parameter IDs (with lfoXSync / lfoYSync / lfoXSubdiv / lfoYSubdiv) must not repeat
any existing ID.

**How to avoid:** Audit the ParamID namespace — no existing parameter starts with "lfo".
Add all 16 new IDs to the ParamID namespace. Confirmed no collisions with existing IDs:
`globalTranspose`, `thirdInterval`, `fifthInterval`, `tensionInterval`, `rootOctave`,
`thirdOctave`, `fifthOctave`, `tensionOctave`, `joystickXAtten`, `joystickYAtten`,
`scalePreset`, `useCustomScale`, `joystickThreshold`, `joystickGateTime`,
`triggerSource0..3`, `randomDensity`, `randomSubdiv0..3`, `randomGateTime`,
`randomClockSync`, `randomFreeTempo`, `filterXAtten`, `filterYAtten`, `filterXOffset`,
`filterYOffset`, `filterMidiCh`, `filterYMode`, `filterXMode`, `voiceCh0..3`,
`looperSubdiv`, `looperLength`, `arpEnabled`, `arpSubdiv`, `arpOrder`, `arpGateTime`,
`filterCutLive`, `filterResLive`, `quantizeMode`, `quantizeSubdiv`.

---

## Code Examples

### Complete LFO Injection Block (Canonical Form)

```cpp
// Source: derived from existing processBlock pattern + LfoEngine.h API

// After: ChordEngine::Params chordP = buildChordParams();  // note: non-const
// Before: int freshPitches[4];

// ── LFO modulation ─────────────────────────────────────────────────────────
{
    const bool xEnabled = *apvts.getRawParameterValue("lfoXEnabled") > 0.5f;
    const bool yEnabled = *apvts.getRawParameterValue("lfoYEnabled") > 0.5f;
    const float xLevel  = *apvts.getRawParameterValue("lfoXLevel");
    const float yLevel  = *apvts.getRawParameterValue("lfoYLevel");

    // kLfoSubdivBeats: static constexpr in PluginProcessor.cpp or anonymous namespace
    // { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 }  maps to 1/1, 1/2, 1/4, 1/8, 1/16, 1/32

    if (xEnabled && xLevel > 0.0f)
    {
        ProcessParams xp;
        xp.sampleRate    = sampleRate_;
        xp.blockSize     = blockSize;
        xp.bpm           = lp.bpm;
        xp.syncMode      = *apvts.getRawParameterValue("lfoXSync") > 0.5f;
        xp.ppqPosition   = ppqPos;
        xp.isDawPlaying  = isDawPlaying;
        xp.rateHz        = *apvts.getRawParameterValue("lfoXRate");
        xp.subdivBeats   = kLfoSubdivBeats[
            (int)*apvts.getRawParameterValue("lfoXSubdiv")];
        xp.maxCycleBeats = 16.0;
        xp.waveform      = static_cast<Waveform>(
            (int)*apvts.getRawParameterValue("lfoXWaveform"));
        xp.phaseShift    = *apvts.getRawParameterValue("lfoXPhase") / 360.0f;
        xp.distortion    = *apvts.getRawParameterValue("lfoXDistortion");
        xp.level         = xLevel;

        chordP.joystickX = std::clamp(
            chordP.joystickX + lfoX_.process(xp), -1.0f, 1.0f);
    }

    if (yEnabled && yLevel > 0.0f)
    {
        ProcessParams yp;
        yp.sampleRate    = sampleRate_;
        yp.blockSize     = blockSize;
        yp.bpm           = lp.bpm;
        yp.syncMode      = *apvts.getRawParameterValue("lfoYSync") > 0.5f;
        yp.ppqPosition   = ppqPos;
        yp.isDawPlaying  = isDawPlaying;
        yp.rateHz        = *apvts.getRawParameterValue("lfoYRate");
        yp.subdivBeats   = kLfoSubdivBeats[
            (int)*apvts.getRawParameterValue("lfoYSubdiv")];
        yp.maxCycleBeats = 16.0;
        yp.waveform      = static_cast<Waveform>(
            (int)*apvts.getRawParameterValue("lfoYWaveform"));
        yp.phaseShift    = *apvts.getRawParameterValue("lfoYPhase") / 360.0f;
        yp.distortion    = *apvts.getRawParameterValue("lfoYDistortion");
        yp.level         = yLevel;

        chordP.joystickY = std::clamp(
            chordP.joystickY + lfoY_.process(yp), -1.0f, 1.0f);
    }
}
```

### APVTS getRawParameterValue Pattern (Existing Convention)

```cpp
// Source: existing PluginProcessor.cpp lines 329-350 (buildChordParams)
// Pattern: getRawParameterValue returns std::atomic<float>*, use ->load() or * operator
const float xAtten = apvts.getRawParameterValue(ParamID::joystickXAtten)->load();
const bool  useCustom = apvts.getRawParameterValue(ParamID::useCustomScale)->load() > 0.5f;
const int   waveIdx = (int)*apvts.getRawParameterValue("lfoXWaveform");
```

### State Reset Pattern (Existing Convention)

```cpp
// Source: existing PluginProcessor.cpp DAW-stop handler
if (justStopped)
{
    for (int v = 0; v < 4; ++v)
        midi.addEvent(juce::MidiMessage::allNotesOff(voiceChs[v]), 0);
    trigger_.resetAllGates();
    lfoX_.reset();  // new
    lfoY_.reset();  // new
}
```

---

## State of the Art

| Aspect | Current State | After Phase 13 | Notes |
|--------|--------------|----------------|-------|
| LFO | Built, tested in isolation (Phase 12) | Wired into processBlock | 14-16 new APVTS params |
| buildChordParams() | Returns const Params | Returns mutable Params (const removed) | Enables LFO injection |
| prepareToPlay() | Stores sampleRate only | Also resets lfoX_, lfoY_ | Clean startup state |
| Preset loading | v1.3 XML loaded → all params at saved values | v1.3 XML loaded → LFO params at defaults (enabled=false, level=0) | Automatic via APVTS |
| DAW automation | ~50 parameters in automation lane | ~64-66 parameters | All LFO params visible to DAW |

---

## Open Questions

1. **Disable ramp: per-block or per-sample?**
   - What we know: CONTEXT says ~10 ms is sufficient; linear ramp.
   - What's unclear: At 512 samples / 44100 Hz ≈ 11.6 ms, a single block transition
     may already be inaudible. At smaller block sizes (32-64 samples) the jump could
     be audible.
   - Recommendation: Implement a block-level ramp coefficient (`rampCoeff * blockSize`
     approach). This handles all block sizes correctly without per-sample processing.

2. **Subdivision parameter count: 14 or 16?**
   - What we know: CONTEXT says 14 parameters. Adding `lfoXSubdiv` / `lfoYSubdiv` as
     AudioParameterChoice brings total to 16.
   - What's unclear: Whether the CONTEXT's "14 parameters" count was approximate and
     subdivision was assumed to be part of the sync parameter, or strictly 14.
   - Recommendation: Include lfoXSubdiv / lfoYSubdiv as separate AudioParameterChoice
     parameters. 16 total. They are required for LFO-08 (sync to tempo with subdivision)
     and Phase 14 will need to bind them to UI controls. The CONTEXT noted this as
     Claude's discretion ("pick standard musical subdivisions").

3. **TriggerSystem delta with LFO active — confirm desired behavior**
   - What we know: STATE.md flags this as a concern. LFO modulating X will produce
     non-zero deltaX every block, triggering joystick-source voices continuously.
   - What's unclear: Whether this creates unwanted note spam for users testing joystick
     trigger source with LFO on.
   - Recommendation: Confirm this is intentional (it is per the design — LFO varies
     pitch at next trigger). Document in plan verification steps: "verify joystick-source
     voices retrigger at LFO rate (expected behavior, not a bug)."

---

## Sources

### Primary (HIGH confidence)

- `Source/LfoEngine.h` + `Source/LfoEngine.cpp` — LfoEngine API, ProcessParams fields,
  reset() contract, thread-safety notes; directly read from repo
- `Source/PluginProcessor.cpp` — existing createParameterLayout(), buildChordParams(),
  processBlock() flow, prepareToPlay(), setStateInformation(); directly read from repo
- `Source/PluginProcessor.h` — member declarations, existing includes, APVTS member;
  directly read from repo
- `.planning/phases/13-processblock-integration-and-apvts/13-CONTEXT.md` — all locked
  decisions, parameter specs, mixing formula; directly read from repo
- `.planning/REQUIREMENTS.md` — LFO-01, -02, -08, -09, -10 definitions; directly read
- `.planning/STATE.md` — accumulated decisions, delta/LFO concern, LfoEngine design
  decisions; directly read from repo

### Secondary (MEDIUM confidence)

- JUCE 8.0.4 APVTS documentation — `replaceState()` behavior with missing XML keys
  (defaults preserved); consistent with observed behavior in existing
  `setStateInformation()` code which has no version migration logic

### Tertiary (LOW confidence)

- None — all findings are grounded in project source files read directly.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all components exist in the repo; no new libraries
- Architecture: HIGH — injection point is unambiguous from reading processBlock
- Pitfalls: HIGH — pitfalls derived from reading actual source code + CONTEXT constraints
- APVTS defaults on load: MEDIUM — consistent with JUCE APVTS design and existing project
  behavior (no migration code exists), but not confirmed against JUCE 8.0.4 source

**Research date:** 2026-02-26
**Valid until:** 2026-03-28 (30 days — stable C++ project, no fast-moving dependencies)
