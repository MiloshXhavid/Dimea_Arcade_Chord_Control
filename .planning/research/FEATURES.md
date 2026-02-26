# Feature Landscape: Dual LFO Modulation + Beat Clock

**Domain:** Plugin LFO engine for a JUCE VST3 MIDI performance instrument
**Researched:** 2026-02-26
**Milestone:** ChordJoystick v1.4 — LFO + Clock
**Overall confidence:** HIGH for waveform math and S&H/Random distinction; HIGH for sync/phase reset behavior; MEDIUM for distortion interpretation

---

> **Scope note:** This document covers only the NEW features for v1.4.
> Previous milestone feature research (v1.1 MIDI Panic, Quantize, Looper Position Bar,
> Gamepad Detection) is preserved at the bottom as a historical appendix.

---

## Context

The two LFOs modulate the plugin's virtual joystick X and Y position values internally,
before those values reach the chord engine and scale quantizer. Both LFOs are structurally
identical; one drives X (fifth/tension), the other drives Y (root/third). Each LFO has 7
controls and 2 boolean toggles. A beat clock indicator provides visual tempo feedback.

---

## Table Stakes

Features that any professional LFO in a live performance instrument must have.
Missing = product feels amateur or broken.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Sine waveform | Default LFO shape in virtually every instrument; smooth, organic | Low | `sin(2π * φ)` where φ is a [0,1) phasor |
| Triangle waveform | Standard LFO shape; linear and predictable | Low | `(φ < 0.5) ? (4φ - 1) : (3 - 4φ)` |
| Sawtooth Up waveform | Classic filter sweep; gradual rise with instant reset | Low | `2φ - 1.0` |
| Sawtooth Down waveform | Reverse sweep; instant jump then gradual fall | Low | `1.0 - 2φ` |
| Square waveform | Rhythmic gating; hard binary modulation | Low | `φ < 0.5 ? +1.0 : -1.0` |
| Sample & Hold (S&H) | Iconic stepped random; staircase voltage; users expect it by name | Medium | New random value at each cycle boundary; held constant until next boundary |
| Random (smooth) | Interpolated S&H; organic drift without abrupt steps | Medium | Linear interpolation between random targets over one cycle |
| Frequency slider | Controls rate; the most fundamental LFO parameter | Low | Hz in Free mode; derived from BPM + subdivision in Sync mode |
| Level slider (depth) | Controls how much the LFO offsets the axis value | Low | Scales LFO output ∈ [-1,+1] before adding to joystick position |
| On/Off toggle | Enable/bypass per LFO without losing parameter state | Low | Boolean gate; outputs 0.0 when off |
| Sync toggle | Locks LFO rate to DAW BPM (or internal free BPM) | Medium | Hard phase reset at DAW play start; PPQ-derived phase for seek-safe alignment |
| Phase preserved across processBlock | Phasor is a class member; not recalculated each call | Low | Required to avoid tearing artifacts between blocks |
| No memory allocation in audio thread | Real-time safety is a hard requirement for VST3 | Low | All LFO state in pre-allocated POD struct members; use JUCE Random::nextFloat() |
| Beat clock indicator (one flash per beat) | Visual confirmation that tempo is live; essential in live performance | Medium | Flashing dot at 30 Hz polling; reads PPQ position or elapsed time |

---

## Waveform Reference: Mathematical Definitions

All waveforms use a normalized phasor `φ ∈ [0.0, 1.0)` that increments once per sample.
Output range before Level scaling is `[-1.0, +1.0]`.

### Phase Accumulator — Shared by All Waveforms

```cpp
// Per sample in processBlock:
φ += frequency / sampleRate;
if (φ >= 1.0f) φ -= 1.0f;   // wrap without fmod (faster, no branch misprediction)
```

When sync is active and DAW is playing, φ can alternatively be derived from PPQ:
```cpp
double beatsPerCycle = noteDivisionInBeats;   // e.g. 4.0 for 1 bar at 4/4
φ = (float)fmod(ppqPosition / beatsPerCycle, 1.0);
φ += phaseOffset;
if (φ >= 1.0f) φ -= 1.0f;
```
PPQ-derived phase handles seek, loop, and tempo automation without drift.

### Sine
```
output = sin(2π * φ)
```
Smooth, continuous, never abrupt. Feels "organic." Recommended default shape.

### Triangle
```
output = (φ < 0.5f) ? (4.0f * φ - 1.0f) : (3.0f - 4.0f * φ)
```
Linear rise then linear fall, symmetric. Sounds more mechanical than sine.
Tip: triangle is perceived as gentler than square but more defined than sine.

### Sawtooth Up (Ramp Up)
```
output = 2.0f * φ - 1.0f
```
Gradual rise from -1 to +1, then instant reset. Classic filter sweep shape.

### Sawtooth Down (Ramp Down)
```
output = 1.0f - 2.0f * φ
```
Inverted saw. Instant jump to +1 then gradual fall. Reverse sweep feel.

### Square
```
output = (φ < 0.5f) ? 1.0f : -1.0f
```
Binary: fully up or fully down. Hard rhythmic gating feel.
Duty cycle fixed at 50% for v1.4. Variable pulse width is a differentiator (see below).

### Sample & Hold (S&H) — Stepped Random

A new random value is drawn at each cycle boundary (when φ wraps) and held for the
entire next cycle. There is no interpolation; transitions are instantaneous.

```cpp
// State (class members):
float sAndHValue = 0.0f;      // the held value
float prevPhase  = 0.0f;      // to detect wrap

// Per sample:
if (φ < prevPhase) {          // wrap detected
    sAndHValue = rng.nextFloat() * 2.0f - 1.0f;   // new random in [-1, +1]
}
prevPhase = φ;
output = sAndHValue;
```

Key behaviors users expect from S&H:
- Each cycle produces a **different** output value with no relation to the previous.
- The transition is **instantaneous** — no portamento or glide between steps.
- The held value lasts exactly one cycle (duration controlled by Frequency).
- Tempo-synced S&H produces rhythmically quantized random jumps — a classic sound.

### Random (Smooth / Interpolated)

Identical trigger logic to S&H, but output glides linearly toward the new target
over the duration of the cycle. φ doubles as the interpolation parameter.

```cpp
// State (class members):
float prevTarget    = 0.0f;
float currentTarget = 0.0f;
float prevPhase     = 0.0f;

// Per sample:
if (φ < prevPhase) {          // wrap detected
    prevTarget    = currentTarget;
    currentTarget = rng.nextFloat() * 2.0f - 1.0f;
}
prevPhase = φ;
output = prevTarget + φ * (currentTarget - prevTarget);   // linear interpolation
```

Optional upgrade: cosine interpolation for a rounder curve.
Linear is acceptable and CPU-trivial for v1.4.

---

## S&H vs Random — Distinction Summary

| Property | S&H (Stepped Random) | Random (Smooth) |
|----------|----------------------|-----------------|
| Output shape | Staircase — flat horizontal steps | Flowing ramps between values |
| Transition | Instantaneous jump | Gradual glide over one full cycle |
| Interpolation | None | Linear (or cosine) |
| New target chosen | At cycle boundary | At cycle boundary (same moment) |
| CPU cost per sample | One comparison + occasional RNG call | Above + one multiply-add |
| Sound character | Staccato, quantized, nervous, vintage | Drifting, organic, breath-like |
| Historical name | "S&H" (Moog usage), "stepped random" | "Smooth random", "glide random" |
| Live use case | Rhythmic arpeggio randomization | Slow filter drift, humanization |
| Sync behavior | Aligned jumps on beat boundaries | Ramps arrive at beat boundaries |

Both shapes draw a new random target at the same moment (cycle wrap).
The only distinction is whether that value is applied immediately (S&H) or interpolated
toward linearly (Random). This distinction is not always intuitive to users; label clearly.

---

## Differentiators

Features that go beyond a minimum viable LFO. Valuable for live performance and
sound design. Not strictly expected, but elevate the product's quality.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Phase slider (offset) | User sets where in the cycle the LFO starts; aligns X/Y LFOs for Lissajous-style motion | Low | Add `phaseOffset` to φ after phasor update; range 0.0–1.0 (= 0°–360°) |
| Distortion / Jitter slider | Adds controlled randomness to waveform; prevents repetitive, robotic modulation | Medium | See Distortion Algorithms section below |
| Sync subdivisions | Musical note division when sync is on; makes LFO period match musical bars | Low | 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 dropdown; freq = BPM / 60 / noteValue |
| Free BPM for internal clock | When DAW not playing, plugin generates its own tempo for LFO sync | Medium | Already partially exists (randomFreeTempo); LFO reuses same BPM param |
| Variable pulse width | Square wave duty cycle > 50%; allows asymmetric on/off times | Low | Replace 0.5 threshold with `pulseWidth` parameter (0.0–1.0) |
| Triplet and dotted subdivisions | Extended rhythmic variety in sync mode | Low | Base subdivision × 2/3 (triplet) or × 1.5 (dotted) |
| LFO-to-LFO phase relationship display | Visual indicator showing relative phase of X vs Y LFOs | Medium | Lissajous preview mini-display; potentially useful but UI-heavy |

---

## Distortion Slider — Algorithm Options

The distortion slider "adds noise/jitter" to the waveform shape. Three implementable
approaches, ordered by recommendation:

### Option A: Additive White Noise (Recommended for v1.4)

Mix a fraction of white noise into the LFO output after the waveform formula:

```cpp
float noise  = rng.nextFloat() * 2.0f - 1.0f;   // JUCE Random, audio-thread safe
output = lfoWaveform + distortion * noise;
output = juce::jlimit(-1.0f, 1.0f, output);
```

Pros: trivial to implement, zero latency, no additional state, sounds good.
Behavior: at distortion=0 the waveform is clean; at distortion=1 it becomes pure noise.
Complexity: Low.

This is the standard behavior of the "Jitter" slider documented in Ableton's Max for
Live LFO device. It is the correct interpretation for this parameter.

### Option B: Phase Jitter

Add a small random offset to the phase accumulator each sample:

```cpp
φ += distortion * (rng.nextFloat() - 0.5f) * 0.02f;   // ±1% per sample
if (φ >= 1.0f) φ -= 1.0f;
if (φ < 0.0f)  φ += 1.0f;
```

Produces subtle wavering of the period length — sounds like clock instability.
Different character from Option A: it shifts waveform timing, not amplitude.
Complexity: Low.

### Option C: Wavefolder / Soft Clip Saturation

Apply tanh saturation to shape the waveform before level scaling:

```cpp
float drive = 1.0f + distortion * 4.0f;
output = std::tanh(output * drive) / std::tanh(drive);
```

Rounds and clips waveform edges; creates a more driven, saturated shape.
This does NOT add randomness. It changes waveshape, not "jitter."
Semantically wrong for the "distortion = noise" intent in the milestone spec.
Complexity: Low — but incorrect framing for a "distortion" knob.

**Decision: Use Option A.** Matches the Ableton M4L LFO "Jitter" documented behavior,
maps well to the knob label "Distortion / Jitter," is CPU-free, and is immediately
audible. Use JUCE's `juce::Random` class, seeded once at construction; `nextFloat()`
is safe to call on the audio thread.

---

## Sync Behavior: Phase Reset Options

### Hard Phase Reset — Recommended for v1.4

When DAW transitions from stopped to playing, reset φ to `phaseOffset`.
LFO always starts from the same position relative to bar 1.

This is the standard behavior in Serum, Vital, Massive, and u-he DIVA.

```cpp
// In processBlock, check isPlaying transition:
if (positionInfo.isPlaying && !wasPlaying) {
    lfoX.phase = lfoX.phaseOffset;
    lfoY.phase = lfoY.phaseOffset;
    lfoX.sAndHValue = rng.nextFloat() * 2.0f - 1.0f;   // reseed S&H
    lfoY.sAndHValue = rng.nextFloat() * 2.0f - 1.0f;
}
wasPlaying = positionInfo.isPlaying;
```

For seek/jump robustness, derive phase from PPQ position on every block:
```cpp
if (syncEnabled && positionInfo.isPlaying) {
    double ppqPerCycle = noteDivisionInBeats;    // e.g. 1.0 for 1-beat LFO
    φ = (float)fmod(positionInfo.ppqPosition / ppqPerCycle, 1.0) + phaseOffset;
    if (φ >= 1.0f) φ -= 1.0f;
}
```
PPQ derivation eliminates drift even after timeline jumps or BPM automation.

### Soft Sync — Not Recommended for v1.4

Adjusts cycle length incrementally toward the clock period (Batumi SYNC mode).
More complex, rarely expected by DAW plugin users. Defer to v2 if ever needed.

### Free Mode (No DAW Sync)

LFO runs from its own phase accumulator at the specified Hz rate.
No reset occurs. Phase drifts relative to song position — expected behavior.
Uses `randomFreeTempo` (existing APVTS param) as the BPM source when a "1/4 note"
subdivision is selected, or the explicit frequency slider value in Hz.

---

## Beat Clock Indicator

### Behavior Users Expect

- One brief flash per quarter note (one beat in 4/4).
- Flash duration: approximately 80–120ms — long enough to see, short enough to feel like a pulse.
- When DAW is playing with sync enabled: reads `ppqPosition` from `AudioPlayHead`.
  Flash fires whenever `fmod(ppqPosition, 1.0) < threshold`.
- When DAW is stopped or sync is off: reads internal free BPM; flash is time-based.
- No flash when plugin has no tempo source at all (DAW stopped, no free BPM).
- Visual: small circle or filled dot. Not a large animation. One element.

### Implementation Pattern

The existing 30 Hz `timerCallback` in `PluginEditor` is sufficient for this.
At 120 BPM, a beat lasts 500ms; 30 Hz gives ~15 frames per beat — smooth enough.

```cpp
// In PluginEditor::timerCallback() — message thread:
const bool dawPlaying = processorRef_.getDawIsPlaying();
const double freeBPM  = processorRef_.getFreeBPM();   // from APVTS randomFreeTempo

bool beatOn = false;

if (dawPlaying) {
    double ppq  = processorRef_.getLastPpqPosition();
    double frac = std::fmod(ppq, 1.0);
    beatOn = (frac < 0.08);   // on for ~8% of beat = ~96ms at 120 BPM
} else if (freeBPM > 0.0) {
    double now         = juce::Time::getMillisecondCounterHiRes() * 0.001;
    double beatPeriod  = 60.0 / freeBPM;
    double frac        = std::fmod(now, beatPeriod) / beatPeriod;
    beatOn = (frac < 0.08);
}

beatDot_.setColour(beatOn
    ? juce::Colour(0xFF00FFFF)      // bright cyan on beat
    : juce::Colour(0xFF1A1A1A));    // near-black off
beatDot_.repaint();
```

This reuses existing infrastructure:
- `getDawIsPlaying()` — PluginProcessor already reads `AudioPlayHead` in processBlock.
- `getLastPpqPosition()` — PluginProcessor stores last PPQ as an atomic double.
- `getFreeBPM()` — wraps the existing `randomFreeTempo` APVTS parameter.
- 30 Hz timer — already exists in PluginEditor.

**No new audio thread infrastructure is needed for the beat clock.**

---

## Anti-Features

Features to explicitly NOT build in v1.4.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| LFO-to-MIDI CC output | These LFOs are position modulators, not CC generators | Keep LFO as internal axis offset; no CC emission |
| Custom LFO shape drawing | Weeks of scope; Serum-tier feature | Use fixed 7 waveforms; defer custom shapes to v2 |
| LFO envelope (attack/decay on LFO itself) | Adds complexity; position modulation doesn't need per-trigger envelope | Level slider covers depth; no envelope needed |
| Variable pulse width | Nice-to-have but adds a parameter; 50% is sufficient for v1.4 | 50% fixed duty cycle |
| LFO-to-velocity or note length | Outside the axis-position modulation model | Not applicable to this design |
| Retrigger on MIDI note | Plugin has no MIDI input (generates MIDI) | N/A |
| Wavetable LFO shapes | Massive infrastructure; out of scope | Defer to v2 |
| Per-voice LFO (4 separate LFOs) | Overcomplicated; 4 voices share 2 axes which share 2 LFOs | Two global LFOs is the correct design |
| Modulation routing matrix | ChordJoystick has exactly 2 modulation destinations (X and Y) | Fixed routing: LFO X → X axis, LFO Y → Y axis |
| Second beat clock (one per LFO) | One dot near BPM knob is sufficient; two would be confusing | One beat clock dot total |
| LFO-to-LFO modulation | Not needed for a 2-destination instrument | Out of scope |

---

## Feature Dependencies

```
Beat clock indicator
    └── AudioPlayHead (already in PluginProcessor.processBlock)
    └── getLastPpqPosition() atomic (already stored or trivially added)
    └── getDawIsPlaying() (already available)
    └── 30 Hz timerCallback (already exists in PluginEditor)
    └── getFreeBPM() — wraps existing randomFreeTempo APVTS parameter

LFO engine (both LFOs share identical structure)
    └── Phase accumulator: float class member, initialized to phaseOffset
    └── Random source: juce::Random seeded at construction (one per LFO)
    └── APVTS parameters (new, 14 total for 2 LFOs):
        └── lfoXShape       (int 0–6)
        └── lfoXFreq        (float, Hz)
        └── lfoXPhase       (float 0.0–1.0)
        └── lfoXDistortion  (float 0.0–1.0)
        └── lfoXLevel       (float 0.0–1.0)
        └── lfoXSync        (bool)
        └── lfoXEnabled     (bool)
        └── lfoYShape .. lfoYEnabled (mirror of above)

LFO sync → AudioPlayHead::PositionInfo.bpm (already read in processBlock)
LFO sync → AudioPlayHead::PositionInfo.ppqPosition (already read in processBlock)
LFO sync → wasPlaying_ bool state (new member in PluginProcessor)

LFO output injection
    └── Joystick X float modified before ChordEngine reads it
    └── Joystick Y float modified before ChordEngine reads it
    └── Output clamped to [0.0, 1.0] (joystick position range)
    └── Level scale: final = joystickValue + lfoOutput * lfoLevel * attenuatorRange
```

---

## APVTS Parameter Additions

| Parameter ID | Type | Range | Default | Purpose |
|--------------|------|-------|---------|---------|
| lfoXEnabled | bool | off/on | off | Bypass LFO X |
| lfoXShape | int | 0–6 | 0 (Sine) | Waveform selector |
| lfoXFreq | float | 0.05–20.0 Hz | 1.0 | Rate in free mode |
| lfoXPhase | float | 0.0–1.0 | 0.0 | Phase offset at start/reset |
| lfoXDistortion | float | 0.0–1.0 | 0.0 | Additive noise amount |
| lfoXLevel | float | 0.0–1.0 | 0.5 | Output depth |
| lfoXSync | bool | off/on | off | Lock to BPM |
| lfoXSyncDiv | int | 0–5 | 2 (1/4) | Sync subdivision (1/1, 1/2, 1/4, 1/8, 1/16, 1/32) |
| lfoYEnabled | bool | off/on | off | Mirror of X set |
| lfoYShape | int | 0–6 | 0 | — |
| lfoYFreq | float | 0.05–20.0 Hz | 1.0 | — |
| lfoYPhase | float | 0.0–1.0 | 0.0 | — |
| lfoYDistortion | float | 0.0–1.0 | 0.0 | — |
| lfoYLevel | float | 0.0–1.0 | 0.5 | — |
| lfoYSync | bool | off/on | off | — |
| lfoYSyncDiv | int | 0–5 | 2 | — |

16 new APVTS parameters total. All are UI-exposed; all persist via APVTS state save/load.

---

## Sync Subdivision Reference

| Subdivision | Beats (1 beat = 1 quarter note) | LFO frequency at 120 BPM |
|-------------|--------------------------------|--------------------------|
| 1/1 (whole) | 4.0 | 0.500 Hz |
| 1/2 (half) | 2.0 | 1.000 Hz |
| 1/4 (quarter) | 1.0 | 2.000 Hz |
| 1/8 (eighth) | 0.5 | 4.000 Hz |
| 1/16 (sixteenth) | 0.25 | 8.000 Hz |
| 1/32 (thirty-second) | 0.125 | 16.000 Hz |

Calculation: `freqHz = (BPM / 60.0) / beatsPerCycle`

---

## MVP Build Order

Recommended implementation sequence to minimize risk and enable early testing:

1. **LFO DSP struct** — Phase accumulator + 7 waveform outputs, Level scaling, Distortion
   noise. No APVTS hookup yet. Write a unit test: feed known frequencies, verify output
   range and waveform shape at cycle boundaries. (Low complexity)

2. **APVTS registration** — Add 16 new parameters to PluginProcessor::createParameters().
   No UI yet; verify state saves/loads without crash. (Low complexity)

3. **Joystick position injection** — In processBlock, add LFO output to joystickX_ and
   joystickY_ after reading from sliders, before ChordEngine uses them. Clamp result.
   (Low complexity — 4 lines of code)

4. **Sync implementation** — wasPlaying_ transition detection, hard phase reset, PPQ-derived
   phase when sync is active. (Medium complexity)

5. **LFO UI section** — Shape dropdown, Freq/Phase/Distortion/Level sliders, On/Off toggle,
   Sync toggle, Sync subdivision dropdown. Left of joystick pad. (Medium complexity — layout)

6. **Beat clock indicator** — Flashing dot in timerCallback using existing 30 Hz timer.
   (Low complexity — 15 lines of code)

**Defer to v2:**
- Triplet and dotted subdivisions (adds UI complexity without high user demand)
- Smooth random cosine interpolation (linear is indistinguishable in practice)
- Variable pulse width for Square waveform

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Waveform math (sine, tri, saw, square) | HIGH | Standard DSP; verified against multiple independent sources including musicdsp.org and JUCE forum |
| S&H vs Random distinction | HIGH | Well-documented in synth literature; Sweetwater, KVR, modwiggler all agree |
| Distortion as additive noise (Option A) | MEDIUM | Ableton M4L LFO "Jitter" parameter documented as "adds randomness"; interpretation is conventional, not formally specified for this plugin |
| Sync hard reset behavior | HIGH | Standard industry behavior across Serum, Vital, DIVA, confirmed in JUCE forum and KVR discussions |
| PPQ-derived phase approach | HIGH | Documented in JUCE forum (getting-bpm-and-beats thread) and JUCE step-by-step blog |
| Beat clock indicator (30 Hz timer) | HIGH | Uses existing confirmed infrastructure in the codebase |
| APVTS parameter count and naming | MEDIUM | Convention follows existing plugin patterns; count is additive and manageable |

---

## Sources

- [A Simple Guide to Modulation: Sample & Hold — Sweetwater InSync](https://www.sweetwater.com/insync/a-simple-guide-to-modulation-sample-and-hold/)
- [Smooth Random LFO Generator — Musicdsp.org (Algorithm #269)](https://www.musicdsp.org/en/latest/Synthesis/269-smooth-random-lfo-generator.html)
- [Classic Waveforms as LFOs — cmp.music.illinois.edu](https://dobrian.github.io/cmp/topics/control-signals/1.Classic-Waveforms-as-LFOs.html)
- [5 Essential LFO Parameters You Should Know — TheProAudioFiles](https://theproaudiofiles.com/essential-lfo-parameters/)
- [Synth Modulation Sources: S&H, Jitter and More — TheProAudioFiles](https://theproaudiofiles.com/synth-modulation-sources-and-controls/)
- [Sample & Hold — Synthesizer Wiki (sequencer.de)](https://www.sequencer.de/synth/index.php/Sample_&_Hold)
- [How to code a random LFO? — KVR Audio DSP Forum](https://www.kvraudio.com/forum/viewtopic.php?t=456482)
- [Tempo sync'd LFO? — JUCE Forum](https://forum.juce.com/t/tempo-syncd-lfo/4496)
- [Host SYNC (LFO/ADSR) — KVR Audio DSP Forum](https://www.kvraudio.com/forum/viewtopic.php?t=270213)
- [7b. LFO Sync — JUCE Step by Step Blog](https://jucestepbystep.wordpress.com/7b-lfo-sync/)
- [Getting BPM and Beats — JUCE Forum](https://forum.juce.com/t/getting-bpm-and-beats/13151)
- [LFO waveform ruler beat display — Surge Synthesizer GitHub Issue #1388](https://github.com/surge-synthesizer/surge/issues/1388)
- [Max for Live LFO Devices with Jitter slider — Ableton Reference Manual v12](https://www.ableton.com/en/live-manual/12/max-for-live-devices/)
- [Noise Engineering: Introducing LFOs to free plugins](https://noiseengineering.us/blogs/loquelic-literitas-the-blog/introducing-lfos/)
- [LFO Like a Boss: Complete Beginner's Guide 2025 — EDMProd](https://www.edmprod.com/lfo/)
- [ModPlay MIDI LFO Plugin (2025) — Bedroom Producers Blog](https://bedroomproducersblog.com/2025/07/23/fsk-audio-modplay/)
- [Digital Generation of LFOs — GEOfex](http://www.geofex.com/article_folders/lfos/psuedorandom.htm)

---

---

## Appendix: v1.1 Feature Research (Historical — 2026-02-24)

> Preserved for roadmap continuity. All v1.1 features were validated and shipped in v1.3.

**Domain:** MIDI Chord Generator / Performance Controller VST Plugin — v1.1 Additions
**Researched:** 2026-02-24

### Feature Domain 1: MIDI Panic Full Reset

**Standard MIDI channel mode messages (MIDI 1.0 spec, HIGH confidence):**

| CC | Name | Value | Effect |
|----|------|-------|--------|
| 120 | All Sound Off | 0 | Immediate silence |
| 121 | Reset All Controllers | 0 | Resets pitch bend, mod wheel, sustain, expression |
| 123 | All Notes Off | 0 | Transitions all notes to released state |
| 64 | Sustain Pedal | 0 | Explicitly releases hold pedal |

De-facto industry panic sequence on ALL 16 channels:
CC64=0, CC120=0, CC123=0, CC121=0, PitchBend center (8192).

**v1.1 gap filled:** Previous v1.0 panic sent allNotesOff on 4 channels only.
v1.1 expanded to all 16 channels + full CC sweep + filter CC reset.

### Feature Domain 2: Trigger / Gate Quantization

Two modes: Live quantize (snap as events are recorded) and Post-record quantize
(snap existing loop). Algorithm: `round(beatPos / subdivBeats) * subdivBeats`.
Apply only to Gate events, not JoystickX/Y or FilterX/Y events.

Subdivisions: 1/4 = 1.0 beats, 1/8 = 0.5, 1/16 = 0.25, 1/32 = 0.125.

### Feature Domain 3: Looper Playback Position Bar

Horizontal progress bar (Pattern 1). Ratio = `getPlaybackBeat() / getLoopLengthBeats()`.
Cyan when playing, amber when recording. Updated at 30 Hz in timerCallback.

### Feature Domain 4: Gamepad Controller Type Detection

`SDL_GameControllerGetType(controller)` (available since SDL 2.0.12).
Returns enum 0–13 covering PS3/PS4/PS5, Xbox 360/One, Switch Pro, Joy-Con, etc.
Display string: "{Type} Connected" or "No Controller".

---
*Feature research for: ChordJoystick v1.4 LFO + Clock (primary) and v1.1 additions (appendix)*
*Primary research date: 2026-02-26*
