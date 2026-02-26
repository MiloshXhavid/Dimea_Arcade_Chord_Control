# Stack Research — v1.4 LFO + Beat Clock

**Project:** ChordJoystick MK2 (existing JUCE 8 VST3 MIDI generator)
**Domain:** JUCE VST3 MIDI effect plugin — LFO engine + beat clock indicator additions only
**Researched:** 2026-02-26
**Confidence:** HIGH (verified against repo source, CMakeLists.txt, and JUCE 8.0.4 headers in `build/_deps/juce-src`)

> **Scope:** This file covers ONLY the new v1.4 features — dual LFO engine and beat clock
> indicator. The full locked stack (JUCE 8.0.4, SDL2 2.30.9 static, CMake FetchContent,
> C++17, MSVC, Catch2, Inno Setup) is documented in prior milestone STACK files.
> No new external libraries are needed for any v1.4 feature.

---

## Decision: No New Dependencies

All v1.4 features are implemented with existing locked stack only. Zero new libraries, zero
new JUCE modules, zero new CMake targets.

| Feature | Why No New Library Needed |
|---------|--------------------------|
| LFO engine (all 7 waveforms) | Pure `<cmath>`: `std::sin`, `std::fmod`, `std::floor`, `std::copysign` — already in every TU |
| LFO BPM sync | `effectiveBpm_` atomic already exposed on `PluginProcessor` — no new clock source |
| LFO S&H waveform | LCG RNG pattern identical to `TriggerSystem::nextRandom()` — copy-paste, no lib |
| Beat clock indicator | `juce::Component::paint()` + `repaint()` in existing 30 Hz `juce::Timer` — core JUCE |
| LFO APVTS parameters | `AudioParameterFloat`, `AudioParameterBool`, `AudioParameterChoice` — already in `juce_audio_processors` |

**Confirmed absent from CMakeLists.txt:** `juce_dsp` is NOT linked and NOT needed. Adding it
would pull in SIMD DSP infrastructure (ProcessorChain, IIR filters, convolution) that this
plugin has zero use for. The `juce::dsp::Oscillator<float>` class requires a `ProcessSpec`
prepare step, a `ProcessContext`, and a `dsp::AudioBlock<float>` — all audio-buffer
abstractions that are meaningless in a MIDI-only `isMidiEffect()=true` plugin.

---

## LFO Engine: Pure Math in processBlock

### Architecture Decision

The LFO engine is a self-contained `LfoEngine` class (`Source/LfoEngine.h` + `LfoEngine.cpp`)
with all state as plain non-atomic members, called exclusively from the audio thread
inside `processBlock`. The result (two floats: lfoX, lfoY) is added to the joystick values
inside `buildChordParams()` before pitches are computed.

**Rationale:** The existing codebase follows this exact pattern — `ChordEngine`, `TriggerSystem`,
and `LooperEngine` are all audio-thread-only objects with non-atomic internal state. Adding
a second pattern (atomic-per-field) would break the established design and add unnecessary
overhead. LFO parameters flow from APVTS through `getRawParameterValue()->load()` at the
top of `processBlock` (relaxed, same as every other APVTS read in this plugin).

### Phase Accumulator (Confirmed sufficient from TriggerSystem.cpp pattern)

```cpp
// Audio-thread-only — no atomics needed for phase state
double phaseX_ = 0.0;  // 0..1
double phaseY_ = 0.0;  // 0..1

// Each processBlock call (not per-sample — LFO is block-rate):
const double phaseIncrementX = (freqHz / sampleRate_) * blockSize;
phaseX_ = std::fmod(phaseX_ + phaseIncrementX, 1.0);
```

Block-rate LFO update is correct here: this plugin has no audio output, so per-sample LFO
aliasing is irrelevant. Block-rate update matches the resolution of MIDI note-on events
(already block-granular). The same approach is used by `TriggerSystem::randomPhase_[v]`
(see `TriggerSystem.cpp` line 116: `std::fmod(randomPhase_[v], samplesPerSubdiv)`).

### Waveform Math — All 7 Waveforms

All functions take `phase` in [0..1) and return output in [-1..+1].

| Waveform | Implementation | Headers needed |
|----------|---------------|----------------|
| Sine | `std::sin(phase * 2.0 * M_PI)` | `<cmath>` — already included in TU |
| Triangle | `1.0f - 4.0f * std::abs(phase - 0.5f)` | `<cmath>` |
| Saw Up | `2.0f * phase - 1.0f` | none — pure arithmetic |
| Saw Down | `1.0f - 2.0f * phase` | none — pure arithmetic |
| Square | `phase < 0.5f ? 1.0f : -1.0f` | none — ternary |
| S&H | Hold last value; trigger new random on phase wrap | LCG, `<cstdint>` |
| Random (smooth) | Lerp between adjacent random values using phase fraction | LCG + lerp |

`<cmath>` is already `#include`-d in `PluginProcessor.cpp` (line 3), `TriggerSystem.cpp`
(line 2), `LooperEngine.cpp` (line 2), `ScaleQuantizer.cpp` (line 2). No new includes needed.

### S&H Implementation Pattern

Identical pattern to `TriggerSystem::nextRandom()` (TriggerSystem.h line 124):

```cpp
uint32_t rngX_ = 0xDEADBEEFu;   // audio-thread-only, no atomic
uint32_t rngY_ = 0xCAFEBABEu;

float nextRandomLfo(uint32_t& rng)
{
    rng = rng * 1664525u + 1013904223u;   // LCG — identical to TriggerSystem
    // Map to [-1..+1]:
    return (static_cast<float>(rng >> 1) / static_cast<float>(0x7FFFFFFFu)) * 2.0f - 1.0f;
}
```

On phase wrap (phaseX_ crosses 1.0), sample a new random value from `nextRandomLfo(rngX_)`.
Hold it until the next wrap. Zero heap allocation, zero branching in steady state.

### BPM Sync

When LFO sync is active: frequency is derived from `effectiveBpm_` (already an
`std::atomic<float>` on `PluginProcessor`, line 176 of `PluginProcessor.h`). Sync
subdivisions are a rate multiplier on the beat:

```cpp
// Sync mode: freq = (bpm / 60.0) * rateMultiplier
// where rateMultiplier = beats-per-LFO-cycle (e.g., 0.25 = 1/4 note rate means 4 cycles/bar)
const double syncedHz = (effectiveBpm / 60.0) * rateMultiplier;
```

The `effectiveBpm_` value is already computed and stored once per block in `processBlock`
(line 465 of `PluginProcessor.cpp`). LfoEngine reads it as a parameter passed in, not
via direct atomic — no new shared state required.

### Thread Safety Pattern for LFO Parameters

Parameters (freq, depth, waveform index, phase offset, distortion amount, on/off) are
read from APVTS via `getRawParameterValue()->load()` at the top of `processBlock` and
passed to `LfoEngine::process()` as a plain struct. This is the exact same pattern used
for every other parameter in this plugin (joystickXAtten, randomDensity, etc.).

The only cross-thread value is `lfoOutputX_` and `lfoOutputY_` — the current LFO output
exposed to the UI for display (waveform preview or LFO LED). These follow the
`effectiveBpm_` / `filterCutDisplay_` pattern: `std::atomic<float>` members on
`PluginProcessor`, written relaxed by audio thread, read relaxed by message thread in
`timerCallback`. Two new atomics, identical to existing `filterCutDisplay_` (line 162
of `PluginProcessor.h`).

**No mutex, no lock, no FIFO required for LFO.** The LFO state (phase accumulator, RNG)
is audio-thread-only private state inside `LfoEngine`. Parameters are read from APVTS
per-block. UI display uses two `std::atomic<float>` exactly like existing display values.

### Integration Point: Where LFO Modulates the Joystick

LFO output is added to joystick values inside `buildChordParams()`, after the gamepad
override logic (line 326 of `PluginProcessor.cpp`), before `ChordEngine::computePitch()`.
This is the single correct injection point — it ensures:

1. LFO affects pitch (via ChordEngine)
2. LFO does NOT interfere with looper playback joystick override (loopOut.hasJoystickX
   check happens before buildChordParams() is called)
3. LFO does NOT get recorded into the looper (looper records the raw joystickX/Y atomics,
   not the modulated chord params)
4. Filter CC is unaffected (filter CC uses separate joystick axis reads, not chordP)

```cpp
// In buildChordParams(), after line 327 (p.joystickY = ...):
if (lfoEngine_.isXEnabled())
    p.joystickX = juce::jlimit(-1.0f, 1.0f, p.joystickX + lfoEngine_.getOutputX());
if (lfoEngine_.isYEnabled())
    p.joystickY = juce::jlimit(-1.0f, 1.0f, p.joystickY + lfoEngine_.getOutputY());
```

`juce::jlimit` is in `juce_core` (transitively included via `juce_audio_processors`).
No new dependency.

### Distortion (Jitter/Noise)

"Distortion" for an LFO means adding random noise to the output before clamping. This is
pure arithmetic — multiply a noise sample by a distortion depth parameter and add to the
clean waveform output. Same `nextRandomLfo()` RNG, different output path. No additional
library needed.

---

## Beat Clock Indicator

### Architecture Decision

A single `juce::Component`-derived dot (e.g., `BeatClockDot`) drawn as a filled circle.
`paint()` reads a cached bool `lit_`. The `timerCallback()` in `PluginEditor` (already
running at 30 Hz) reads `getEffectiveBpm()` (already on `PluginProcessor`, line 119 of
`PluginProcessor.h`) plus a new `getBeatPhase()` accessor that returns the current
[0..1) beat phase, and sets `lit_` = `(phase < 0.15f)` to create a flash at each beat.

### Beat Phase Source

The beat phase is computed from `effectiveBpm_` and a sample counter maintained inside
`PluginProcessor`. No new clock source, no new thread. The sample counter is an
`int64_t` advanced in `processBlock` by `blockSize` samples, and the beat phase is:

```cpp
// In processBlock (audio thread writes):
const double beatsElapsed = static_cast<double>(sampleCounter_) / sampleRate_ / 60.0 * effectiveBpm;
beatPhase_.store(static_cast<float>(std::fmod(beatsElapsed, 1.0)), std::memory_order_relaxed);
```

When DAW sync is active and `ppqPos >= 0.0`, use `std::fmod(ppqPos, 1.0)` directly for
phase instead of the internal counter — this gives sample-accurate DAW beat alignment.

`beatPhase_` is a new `std::atomic<float>` member on `PluginProcessor`, same type and
same pattern as `effectiveBpm_`, `filterCutDisplay_`, etc.

### UI Flash Pattern

```cpp
// In PluginEditor::timerCallback():
const float phase = proc_.getBeatPhase();
beatClockDot_.setLit(phase < 0.15f);   // on for first 15% of each beat

// BeatClockDot::setLit():
void setLit(bool lit) { if (lit_ != lit) { lit_ = lit; repaint(); } }

// BeatClockDot::paint():
g.setColour(lit_ ? Clr::highlight : Clr::gateOff);
g.fillEllipse(getLocalBounds().toFloat().reduced(2.0f));
```

`repaint()` is called only on state change (lit/unlit transition) — not every timer tick.
At 30 Hz timer with 120 BPM (2 beats/sec), transitions fire at most 4 times/sec. No
performance concern. This is the JUCE-recommended blinking pattern confirmed by the JUCE
forum thread "Blinking button in LookAndFeel / timerCallback?" (forum.juce.com).

**No additional timer needed.** The existing `startTimerHz(30)` in `PluginEditor` is
sufficient. 30 Hz gives 33 ms resolution; a flash threshold of 15% of a beat at 120 BPM
= 125 ms, which is 3.75 timer ticks — comfortably visible.

---

## APVTS Parameters (New for v1.4)

Follows the exact existing pattern from `createParameterLayout()` in `PluginProcessor.cpp`.

| Parameter ID | Type | Range | Default | Purpose |
|-------------|------|-------|---------|---------|
| `lfoXEnabled` | Bool | false/true | false | LFO X axis on/off |
| `lfoYEnabled` | Bool | false/true | false | LFO Y axis on/off |
| `lfoXWave` | Choice | 0..6 (Sine/Tri/Saw+/Saw-/Sq/S&H/Rand) | 0 | X LFO waveform |
| `lfoYWave` | Choice | 0..6 | 0 | Y LFO waveform |
| `lfoXFreq` | Float | 0.01..20.0 Hz | 1.0 | X LFO rate (free mode) |
| `lfoYFreq` | Float | 0.01..20.0 Hz | 1.0 | Y LFO rate (free mode) |
| `lfoXDepth` | Float | 0.0..1.0 | 0.5 | X LFO output level (scales [-1..1] output) |
| `lfoYDepth` | Float | 0.0..1.0 | 0.5 | Y LFO output level |
| `lfoXPhase` | Float | 0.0..1.0 | 0.0 | X LFO phase offset (initial phase) |
| `lfoYPhase` | Float | 0.0..1.0 | 0.0 | Y LFO phase offset |
| `lfoXDistort` | Float | 0.0..1.0 | 0.0 | X LFO noise/jitter amount |
| `lfoYDistort` | Float | 0.0..1.0 | 0.0 | Y LFO noise/jitter amount |
| `lfoXSync` | Bool | false/true | false | X LFO BPM sync |
| `lfoYSync` | Bool | false/true | false | Y LFO BPM sync |
| `lfoXSyncRate` | Choice | 0..7 (1/16 to 4 bars) | 2 | X LFO sync subdivision |
| `lfoYSyncRate` | Choice | 0..7 | 2 | Y LFO sync subdivision |

All use `addFloat`, `addBool`, `addChoice` helpers already defined in `createParameterLayout()`.
No new APVTS infrastructure — just more parameter registrations.

### Sync Rate Subdivisions

| Index | Description | Rate multiplier (beats per cycle) |
|-------|-------------|----------------------------------|
| 0 | 1/16 | 0.25 |
| 1 | 1/8 | 0.5 |
| 2 | 1/4 (quarter) | 1.0 |
| 3 | 1/2 (half) | 2.0 |
| 4 | 1 bar | 4.0 |
| 5 | 2 bars | 8.0 |
| 6 | 4 bars | 16.0 |
| 7 | 8 bars | 32.0 |

---

## What NOT to Add

| Do Not Add | Why |
|------------|-----|
| `juce_dsp` module | `juce::dsp::Oscillator<float>` requires `ProcessSpec` + `AudioBlock<float>` — audio buffer abstractions meaningless in an isMidiEffect plugin. Adding `juce_dsp` pulls in SIMD filter and convolution infrastructure with zero benefit. Six lines of `<cmath>` replace it completely. |
| Any third-party LFO library (moodycamel, signalsmith-dsp, etc.) | The LFO math is 7 waveforms totaling ~30 lines of `<cmath>`. A library adds build complexity, a new FetchContent entry, and potential static-CRT conflicts. |
| Per-sample LFO update | This plugin has no audio output. Block-rate LFO update is indistinguishable from per-sample for MIDI output. Per-sample update would cost 512x more sin() calls per block with zero audible benefit. |
| Second JUCE Timer for beat clock | The existing 30 Hz `timerCallback` is sufficient. Multiple timers add latency jitter and complexity. The looper position bar already uses this timer successfully. |
| `std::mutex` or `std::lock_guard` for LFO state | The LFO phase accumulator and RNG are audio-thread-only. Parameters come from APVTS (lock-free atomic reads). UI display uses two `std::atomic<float>` (same as `effectiveBpm_`). No mutex ever touches the audio thread in this codebase. |
| Pitch bend for LFO output | LFO modulates the joystick value, which goes through `ChordEngine::computePitch()` and scale quantization. Scale quantization already handles the musical clamping. Pitch bend would bypass the scale and produce out-of-scale notes, which contradicts the plugin's core value proposition. |
| Separate LFO thread | LFO computation is trivial (one sin + one fmod per block). Running it on any thread other than the audio thread would require atomic handoff for no benefit. The audio thread is the correct place. |

---

## File Plan for v1.4

| New File | Role |
|----------|------|
| `Source/LfoEngine.h` | LfoEngine class: dual-axis phase accumulators, 7 waveforms, LCG RNG, `process()` called from audio thread |
| `Source/LfoEngine.cpp` | Implementation of all waveform math, sync logic, distortion |

| Modified File | Change |
|---------------|--------|
| `Source/PluginProcessor.h` | Add `LfoEngine lfoEngine_` member; add `std::atomic<float> lfoOutputX_`, `lfoOutputY_`, `beatPhase_`; add `getBeatPhase()` accessor; add `int64_t sampleCounter_` |
| `Source/PluginProcessor.cpp` | Add LFO APVTS parameters; call `lfoEngine_.process()` in processBlock; inject LFO output in `buildChordParams()`; advance `sampleCounter_`; compute/store `beatPhase_` |
| `Source/PluginEditor.h` | Add `BeatClockDot beatClockDot_` member |
| `Source/PluginEditor.cpp` | Add LFO UI section (sliders, dropdown, sync button, on/off); add `BeatClockDot`; update `timerCallback()` to drive beat flash |
| `CMakeLists.txt` | Add `Source/LfoEngine.cpp` to `target_sources()` |

No other files require modification.

---

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| LFO implementation | Pure `<cmath>` phase accumulator in `LfoEngine` class | `juce::dsp::Oscillator<float>` | Requires `juce_dsp` module not currently linked; `ProcessSpec`/`AudioBlock` abstractions add complexity for zero benefit in MIDI-only plugin |
| LFO injection point | `buildChordParams()` after gamepad override, before ChordEngine | Directly modifying `joystickX` atomic | Writing to `joystickX` atomic from audio thread while looper may be overriding it creates a data-race on `store()` vs the looper's `store()` in the same block |
| Beat phase source | Internal `sampleCounter_` + DAW `ppqPos` fallback | Dedicated beat-phase class | Samplecounter is 2 lines in processBlock; a separate class adds 50+ lines for identical functionality |
| Beat clock flash threshold | 15% of beat duration | 50% (half-beat on) | 15% produces a crisp visual pulse; 50% produces a wide blink that looks like a toggle, not a clock |
| LFO update rate | Block-rate (once per processBlock) | Per-sample (inside sample loop) | Block-rate is sufficient for MIDI modulation; per-sample costs 512x more `std::sin` calls with no audible difference since MIDI resolution is also block-granular |
| LFO parameter count | 16 parameters (X and Y fully independent) | Shared freq/wave per axis pair | Independent control per axis is the explicit v1.4 spec ("dual LFOs (X + Y axis)"); shared params would eliminate the "dual" aspect |

---

## Sources

### PRIMARY — HIGH Confidence (verified from source in repo)

- `CMakeLists.txt` lines 141–148 — confirmed `juce_dsp` is NOT in `target_link_libraries`; only `juce_audio_processors`, `juce_audio_utils`, `juce_gui_basics`, `juce_gui_extra`, `SDL2-static`
- `Source/PluginProcessor.cpp` line 3 — `#include <cmath>` confirmed; lines 465, 176 — `effectiveBpm_` atomic pattern confirmed
- `Source/PluginProcessor.h` lines 119, 162–163 — `getEffectiveBpm()` and `filterCutDisplay_`/`filterResDisplay_` atomic display pattern confirmed
- `Source/TriggerSystem.h` line 124 — LCG RNG pattern (`rng_ = rng_ * 1664525u + 1013904223u`) confirmed — reuse for S&H waveform
- `Source/TriggerSystem.cpp` line 116 — `std::fmod(randomPhase_[v], samplesPerSubdiv)` block-rate phase accumulator pattern confirmed
- `Source/LooperEngine.h` lines 1–5 — `<cmath>` include and `std::atomic<float>` lock-free guarantee comment confirmed
- `Source/PluginEditor.cpp` — `std::sin`, `std::cos` already used for joystick knob arc drawing; `<cmath>` already included
- `Source/PluginProcessor.cpp` lines 306–352 — `buildChordParams()` confirmed as correct LFO injection point
- `build/_deps/juce-src/` — JUCE 8.0.4 tag `8.0.4` (Nov 18 2024) confirmed from CMakeLists.txt line 13

### SECONDARY — MEDIUM Confidence (web verified)

- JUCE Forum "Blinking button in LookAndFeel / timerCallback?" — `repaint()` on state-change-only is the JUCE-recommended pattern for blink components; aligns with JUCE's own `TimersAndEventsDemo.h` example
- JUCE Forum thread on audio thread safety — `std::atomic<float>` for audio→UI display values is the established JUCE community standard pattern; matches existing codebase usage

---

*Stack research for: ChordJoystick MK2 v1.4 — LFO + Beat Clock*
*Researched: 2026-02-26*
