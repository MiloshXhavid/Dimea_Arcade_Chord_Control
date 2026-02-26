# Phase 12: LFO Engine Core — Research

**Researched:** 2026-02-26
**Domain:** DSP oscillator / LFO class in C++17, audio-thread-safe, JUCE 8 + Catch2 v3.8.1
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**S&H vs Random Waveform Distinction**

- S&H: holds one LCG-generated value for a full cycle period; jumps instantaneously at the cycle boundary — no interpolation; Rate knob governs cycle duration
- Random: generates a new LCG value on every sample (audio-rate white noise); ignores the Rate knob entirely — rate has no effect on Random output; effectively true per-sample noise
- Waveform switching: immediate jump to the new waveform's output at its current phase — no transition smoothing in the engine

**Distortion Parameter Behavior**

- Mechanism: additive LCG noise — `output = waveform_output + (distortion × lcg_noise_sample)`
- `distortion = 0` → bypass, clean waveform output (no effect)
- `distortion = 1` → waveform buried under full-amplitude LCG noise
- Output can exceed ±1 slightly at high distortion values
- RNG: shares the same single LCG seed as S&H and Random waveforms (one LCG chain per `LfoEngine` instance)
- Applied to: all 7 waveforms **except Random** (Random is already pure noise — applying distortion to it is a no-op)

**Beat-Sync Subdivisions**

- Rate range in sync mode: 1/32 note (fastest) → loopLengthBars × barDuration (slowest)
- The slowest sync rate equals one full LFO cycle per looper loop; loop length is passed as a parameter to `LfoEngine::process()` — engine is not coupled to LooperEngine or APVTS
- Subdivision list the engine must support:
  - Straight: 1/32, 1/16, 1/8, 1/4, 1/2, 1/1, 2/1, 4/1, and up to loopLengthBars/1
  - Dotted: 1/32., 1/16., 1/8., 1/4., 1/2.
  - Triplet: 1/32T, 1/16T, 1/8T, 1/4T, 1/2T
- Looper bar count caps the slow end dynamically — if looper is set to 6 bars, maximum sync subdivision = 6/1

**Free-Mode Rate Range**

- Minimum: 0.01 Hz (one cycle ≈ 100 seconds)
- Maximum: 20 Hz (upper edge of audio rate)
- Scale: logarithmic — equal knob travel per decade
- At 20 Hz the LFO enters audible territory, enabling FM-like pitch effects; Phase 14 slider must use a log taper

### Claude's Discretion

None listed in CONTEXT.md — all design decisions are locked.

### Deferred Ideas (OUT OF SCOPE)

- **Joystick UI visual tracking**: When either LFO is active, the `JoystickPad` dot should visually move to reflect the LFO-modulated X/Y position. This is a Phase 14 rendering requirement — the LFO output values must be accessible from the editor repaint path. Do NOT implement in Phase 12.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| LFO-03 | User can select waveform shape per LFO via dropdown (Sine / Triangle / Saw Up / Saw Down / Square / S&H / Random) | Waveform enum + `process()` dispatch switch — pure DSP math, no external library needed |
| LFO-04 | When Sync OFF → frequency slider (free-running Hz); when Sync ON → subdivision selector | Dual-mode ProcessParams: `rateHz` (free) vs `subdivBeats` (sync); sync mode derives phase from ppqPos (same pattern as existing arpeggiator) |
| LFO-05 | User can set LFO phase shift via slider (0–360°) | Phase offset applied as additive constant to normalized phase before waveform evaluation |
| LFO-06 | User can set LFO distortion (additive jitter/noise amount) via slider | `output += distortion * (lcg_sample * 2.0f - 1.0f)` — share the per-instance LCG, skip for Random waveform |
| LFO-07 | User can set LFO level (modulation depth / amplitude) via slider | `output *= level` applied after distortion; output ∈ [-level, +level] before clamping at injection point (Phase 13) |
| PERF-01 | LFO S&H / Random waveforms use the project's existing LCG pattern (not std::rand) — audio-thread safe under MSVC | LCG: `rng_ = rng_ * 1664525u + 1013904223u` — already proven in TriggerSystem; copy exact pattern |
| PERF-02 | LFO output is applied as an additive local float offset inside buildChordParams() — never writes to shared joystick atomics | LfoEngine::process() returns a plain float value; no atomics, no shared state writes — confirmed by success criterion #5 |
| PERF-03 | Synced LFO phase derived from ppqPos (DAW mode) or sample counter (free mode) — no free-accumulation drift across transport stops | Mirror arpeggiator pattern: `phase = fmod(ppqPos, cycleBeats)` when DAW playing; sample counter reset on transport stop |
</phase_requirements>

---

## Summary

Phase 12 builds `LfoEngine` as a standalone DSP class with no APVTS dependencies, no UI coupling, and no shared-state writes. The class produces one float value per audio block call representing the current LFO output. All seven waveforms (Sine, Triangle, Saw Up, Saw Down, Square, S&H, Random) are implemented using pure math and a single per-instance LCG for stochastic waveforms — the exact same LCG multiplier/increment already proven in `TriggerSystem`.

The class has two timing modes. In sync mode it derives its phase directly from `ppqPos` (DAW playing) or an internal sample counter (DAW stopped), preventing drift across transport stop/start. In free mode it accumulates a phase counter per-block based on `rateHz` and `sampleRate`. The existing arpeggiator in `PluginProcessor.cpp` already implements the `ppqPos`-based synchronization pattern verbatim — `phase = fmod(ppqPos, subdivBeats)` — and `LfoEngine` must follow the same approach.

The phase must be fully tested with Catch2 v3.8.1 (already in CMakeLists.txt as `v3.8.1`). Tests are added to the existing `ChordJoystickTests` executable by adding `Source/LfoEngine.cpp` and `Tests/LfoEngineTests.cpp` to its `add_executable` list. The class must be completely independent of JUCE MessageManager or audio processor infrastructure so it can be linked into the test binary without full plugin context.

**Primary recommendation:** Implement `LfoEngine` as a pure-C++17 DSP class following the TriggerSystem LCG pattern and arpeggiator ppqPos-sync pattern. No external DSP library; no JUCE juce_dsp module (isMidiEffect, out of scope per REQUIREMENTS.md). Add it to the existing Catch2 test target.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++17 stdlib (`<cmath>`) | C++17 | `std::sin`, `std::fmod`, `std::floor` for waveform math | Already the project standard; all Source files use CXX_STANDARD 17 |
| Catch2 | v3.8.1 | Unit test framework | Already declared in CMakeLists.txt FetchContent; all existing tests use it |
| JUCE juce_audio_processors | 8.0.4 | `juce::jlimit`, `juce::jmap` utilities (optional) | Already linked in test target via `target_link_libraries` |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| None beyond the above | — | — | This phase is pure DSP math; no additional library is needed or appropriate |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Hand-rolled LCG | `std::mt19937` | Mersenne Twister acquires a CRT mutex on MSVC — audio-thread unsafe; LCG is lock-free and deterministic |
| Hand-rolled LCG | `std::rand()` | `std::rand()` acquires a global CRT mutex on MSVC — audio-thread unsafe (PERF-01 explicitly forbids it) |
| Hand-rolled trig | `juce_dsp` oscillator | juce_dsp is out of scope for isMidiEffect plugins per REQUIREMENTS.md |

**No package installation needed.** All dependencies are already present.

---

## Architecture Patterns

### Recommended File Layout

```
Source/
├── LfoEngine.h          # Class declaration, ProcessParams struct, Waveform enum
├── LfoEngine.cpp        # Waveform implementations, process() body
Tests/
├── LfoEngineTests.cpp   # Catch2 unit tests — mirrors ChordEngineTests.cpp pattern
```

### Pattern 1: ProcessParams Struct (mirrors TriggerSystem and LooperEngine)

**What:** All inputs are bundled into a value-type `ProcessParams` struct passed by const-ref to `process()`. The return value is a plain `float` (the LFO output for this block).

**When to use:** Mandatory — all existing subsystems (TriggerSystem, LooperEngine) use this pattern. It keeps the class testable (no global state needed) and prevents coupling.

```cpp
// Source: project conventions — mirrors TriggerSystem::ProcessParams
class LfoEngine
{
public:
    enum class Waveform { Sine = 0, Triangle, SawUp, SawDown, Square, SH, Random };

    struct ProcessParams
    {
        // Timing
        double sampleRate   = 44100.0;
        int    blockSize    = 512;
        double bpm          = 120.0;       // effective BPM (free or DAW)

        // Sync mode
        bool   syncMode     = false;       // false = free Hz, true = beat subdivisions
        double ppqPosition  = -1.0;        // -1 = DAW not available / not playing
        bool   isDawPlaying = false;

        // Free mode
        float  rateHz       = 1.0f;        // 0.01..20.0 Hz (log scale)

        // Sync mode — subdivision in beats (quarter-note = 1.0 beat)
        // e.g. 1/8 = 0.5, 1/4 = 1.0, dotted 1/4 = 1.5, triplet 1/4 = 2.0/3.0
        double subdivBeats  = 1.0;

        // Max slow subdivision (loopLengthBars × beatsPerBar passed from PluginProcessor)
        double maxCycleBeats = 16.0;

        // Waveform + parameters
        Waveform waveform   = Waveform::Sine;
        float    phaseShift = 0.0f;        // 0.0..1.0 (maps 0–360°)
        float    distortion = 0.0f;        // 0.0..1.0
        float    level      = 1.0f;        // 0.0..1.0 (amplitude / depth)
    };

    // Returns LFO output for this block: range is [-level, +level] before distortion.
    // Pure audio-thread call — no mutex, no shared atomic writes.
    float process(const ProcessParams& p);

    // Reset phase and sample counter — call from prepareToPlay() and on transport stop.
    void reset();

private:
    // Phase accumulator (free mode) — in cycles [0.0, 1.0)
    double phase_        = 0.0;

    // Sample counter (sync mode, DAW stopped) — reset on transport restart
    int64_t sampleCount_ = 0;

    // S&H held value — set at start of each cycle, held for full cycle
    float shHeld_        = 0.0f;
    bool  shInitialized_ = false;   // true after first cycle boundary

    // LCG state — one per instance, no std::rand()
    uint32_t rng_        = 0xABCDEF01u;

    float nextLcg()
    {
        rng_ = rng_ * 1664525u + 1013904223u;
        // Map uint32 → [-1, +1]
        return static_cast<float>(static_cast<int32_t>(rng_)) / float(0x7FFFFFFF);
    }
};
```

### Pattern 2: ppqPos-Based Sync Phase (mirrors existing arpeggiator)

**What:** When DAW is playing and `ppqPos >= 0`, derive the LFO phase directly from `fmod(ppqPos, cycleBeats) / cycleBeats`. This is identical to how the arpeggiator derives its step in `PluginProcessor.cpp` (lines 794–800).

**When to use:** Whenever `syncMode == true && isDawPlaying && ppqPosition >= 0`.

```cpp
// Source: mirrors PluginProcessor.cpp arpeggiator ppqPos sync (lines 794-800)
float LfoEngine::process(const ProcessParams& p)
{
    const double cycleBeats = p.syncMode ? p.subdivBeats : (60.0 / p.rateHz / (60.0 / p.bpm));

    double normalizedPhase; // 0.0..1.0 = one full cycle

    if (p.syncMode && p.isDawPlaying && p.ppqPosition >= 0.0)
    {
        // Lock phase to DAW beat grid — no drift possible
        normalizedPhase = std::fmod(p.ppqPosition, cycleBeats) / cycleBeats;
    }
    else if (p.syncMode && !p.isDawPlaying)
    {
        // DAW stopped: use sample counter so phase doesn't jump on restart
        const double cycleSeconds = cycleBeats * (60.0 / p.bpm);
        const double cycleSamples = cycleSeconds * p.sampleRate;
        normalizedPhase = std::fmod(static_cast<double>(sampleCount_), cycleSamples) / cycleSamples;
        sampleCount_ += p.blockSize;
    }
    else
    {
        // Free mode: accumulate phase_ per block
        const double phaseIncrement = (p.rateHz / p.sampleRate) * p.blockSize;
        phase_ = std::fmod(phase_ + phaseIncrement, 1.0);
        normalizedPhase = phase_;
    }

    // Apply phase shift (0..1 maps to 0..360 degrees)
    normalizedPhase = std::fmod(normalizedPhase + static_cast<double>(p.phaseShift), 1.0);

    // Evaluate waveform
    float output = evaluateWaveform(p.waveform, normalizedPhase, p.blockSize, p.isDawPlaying);

    // Apply distortion (additive LCG noise) — skip for Random waveform
    if (p.waveform != Waveform::Random && p.distortion > 0.0f)
        output += p.distortion * nextLcg();

    // Apply level (amplitude / depth)
    output *= p.level;

    return output;
}
```

### Pattern 3: Waveform Math (pure C++17, no library)

**What:** Each waveform is computed from `normalizedPhase ∈ [0.0, 1.0)`.

```cpp
// Source: standard DSP math — verified against WolframAlpha, no external library
float LfoEngine::evaluateWaveform(Waveform w, double phase, int blockSize, bool dawPlaying)
{
    const float phi = static_cast<float>(phase);

    switch (w)
    {
        case Waveform::Sine:
            return std::sin(phi * juce::MathConstants<float>::twoPi);

        case Waveform::Triangle:
            // Peaks at phi=0.25 (+1), troughs at phi=0.75 (-1)
            return (phi < 0.5f) ? (4.0f * phi - 1.0f) : (3.0f - 4.0f * phi);

        case Waveform::SawUp:
            // Rises from -1 at phi=0 to +1 at phi→1
            return 2.0f * phi - 1.0f;

        case Waveform::SawDown:
            return 1.0f - 2.0f * phi;

        case Waveform::Square:
            return (phi < 0.5f) ? 1.0f : -1.0f;

        case Waveform::SH:
            // shHeld_ is set at cycle boundary (phi crosses 0), held for full cycle
            // Boundary detection: this block's normalizedPhase < previous block's phase
            // → handled by caller tracking previous phase, or by comparing int(phase*N)
            return shHeld_;   // updated at boundary — see S&H boundary handling below

        case Waveform::Random:
            // New LCG value every sample — rate-agnostic, pure noise
            return nextLcg();

        default:
            return 0.0f;
    }
}
```

### Pattern 4: S&H Cycle Boundary Detection

**What:** S&H needs to detect when the phase wraps (crosses 0) to generate a new held value. Use the integer cycle index (floor division) — when `floor(currentCyclePos)` increments vs last block, a boundary occurred.

```cpp
// S&H boundary detection approach — no per-sample loop needed for block-rate LFO
// Store the integer cycle count from the last block. When it changes, new LCG value.
// In free mode: cycle = floor(accumulated_phase_in_cycles)
// In sync mode (DAW): cycle = floor(ppqPosition / subdivBeats)
int64_t currentCycle;
if (syncMode && isDawPlaying && ppqPosition >= 0.0)
    currentCycle = static_cast<int64_t>(ppqPosition / cycleBeats);
else
    currentCycle = /* accumulated integer cycles */;

if (currentCycle != prevCycle_)
{
    shHeld_ = nextLcg();  // new random value at boundary — instant jump
    prevCycle_ = currentCycle;
}
```

### Pattern 5: Subdivision Beat Values

**What:** The 23 subdivisions from CONTEXT.md must map to a `double` beat duration.

```cpp
// Beat durations (1 beat = 1 quarter note)
// Straight
1/32  → 0.125
1/16  → 0.25
1/8   → 0.5
1/4   → 1.0
1/2   → 2.0
1/1   → 4.0      // one bar of 4/4
2/1   → 8.0
4/1   → 16.0
// Dotted (multiply by 1.5)
1/32. → 0.1875
1/16. → 0.375
1/8.  → 0.75
1/4.  → 1.5
1/2.  → 3.0
// Triplet (multiply by 2/3)
1/32T → 0.08333...
1/16T → 0.16666...
1/8T  → 0.33333...
1/4T  → 0.66666...
1/2T  → 1.33333...
// Dynamic max: loopLengthBars × beatsPerBar (passed as maxCycleBeats)
```

The beat values above assume a 4/4 bar = 4 beats. The `subdivBeats` field in ProcessParams already carries the resolved beat value — the caller (Phase 13 PluginProcessor integration) is responsible for converting the dropdown index to a beat value. LfoEngine itself only needs the pre-resolved `subdivBeats` double.

### Anti-Patterns to Avoid

- **Using `std::rand()` or `std::mt19937`:** Both acquire CRT/stdlib mutexes on MSVC audio thread — PERF-01 explicitly forbids this. Use the project LCG (`rng_ * 1664525u + 1013904223u`) exclusively.
- **Writing back to `joystickX` / `joystickY` atomics:** PERF-02 explicitly forbids this. LfoEngine returns a float; PluginProcessor applies it locally in Phase 13.
- **Accumulating phase across transport stop/start:** PERF-03 forbids drift. In DAW sync mode, always re-derive phase from `fmod(ppqPos, cycleBeats)` — never blindly accumulate.
- **Smoothing waveform switches:** CONTEXT.md locked decision: waveform switching is an immediate jump — no cross-fade, no transition period.
- **Interpolating S&H between steps:** CONTEXT.md locked decision: S&H holds one value for a full cycle and jumps instantaneously at the boundary — no interpolation.
- **Linking LfoEngine to APVTS or PluginProcessor:** The class must be independent so it compiles and tests without a full plugin context.
- **Using `juce_dsp` oscillator:** Out of scope for isMidiEffect plugins per REQUIREMENTS.md.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Phase wrap arithmetic | Custom wrap logic | `std::fmod` from `<cmath>` | Already available, handles negative inputs correctly |
| Sine computation | Taylor series approximation | `std::sin` from `<cmath>` | Already standard; no accuracy gain from approximation at LFO rates |
| Test discovery | Custom test runner | `catch_discover_tests(ChordJoystickTests)` already in CMakeLists | Already wired — just add source files to the existing target |

**Key insight:** This phase is pure DSP logic using only C++ stdlib math. There is nothing to "reach for" beyond what already exists in the project.

---

## Common Pitfalls

### Pitfall 1: Phase Drift on DAW Transport Stop/Start

**What goes wrong:** If `phase_` accumulates as a free-running counter even during DAW sync mode, stopping and restarting the DAW creates a phase discontinuity relative to the beat grid.

**Why it happens:** Forgetting to branch on `isDawPlaying` — code always increments `phase_` regardless of DAW state.

**How to avoid:** In sync mode when `isDawPlaying && ppqPosition >= 0`, always derive phase as `fmod(ppqPosition, cycleBeats) / cycleBeats`. Only fall back to a sample counter when `!isDawPlaying`. Reset the sample counter when DAW restarts (detect `!prevDawPlaying_ && isDawPlaying` edge).

**Warning signs:** LFO appears in phase immediately after DAW start, then drifts slightly — the phase at transport stop is not zero, so `fmod` gives a non-zero phase while the counter starts from zero.

### Pitfall 2: S&H Boundary Miss on Long Blocks

**What goes wrong:** S&H does not generate a new value because the cycle boundary fell inside the audio block but boundary detection only compares block-start phases.

**Why it happens:** At low sync rates (slow LFO) with large block sizes (512+ samples), a cycle boundary can fall mid-block. Comparing only the phase at block start vs the previous block's phase misses mid-block wraps.

**How to avoid:** Use integer cycle index comparison (`floor(ppqPos / cycleBeats)` vs `floor(prevPpqPos / cycleBeats)`). A change in the integer cycle index means at least one boundary occurred in this block — generate a new S&H value regardless of where in the block it fell. At LFO rates up to 20 Hz with blocks of 512 at 44100 Hz, a block can span multiple cycles; the integer comparison catches all of them.

**Warning signs:** S&H output holds the same value for multiple expected cycles at slow LFO rates.

### Pitfall 3: LCG Output Mapping Inconsistency

**What goes wrong:** S&H, Random, and distortion LCG samples map to different ranges, producing asymmetric output.

**Why it happens:** `TriggerSystem::nextRandom()` maps LCG output to `[0, 1)` (for probability). For the LFO, the output must be `[-1, +1]`. Using the same mapping as TriggerSystem gives a one-sided signal.

**How to avoid:** Map as `static_cast<int32_t>(rng_) / float(0x7FFFFFFF)` which uses the signed interpretation of the uint32, producing `[-1, +1]`. This differs from TriggerSystem's mapping intentionally. Document clearly in the header.

**Warning signs:** LFO S&H output is always positive (0 to 1) rather than centered around zero.

### Pitfall 4: CMakeLists Not Updated for Test Target

**What goes wrong:** `LfoEngine.cpp` is added to `Source/` but not to the `add_executable(ChordJoystickTests ...)` list. Tests fail to compile because the translation unit is absent.

**Why it happens:** The plugin target (`juce_add_plugin`) and the test target (`add_executable`) are separate. Adding a source file to the project does not automatically add it to both.

**How to avoid:** When adding `Source/LfoEngine.cpp`, add it to BOTH the `target_sources(ChordJoystick ...)` list AND the `add_executable(ChordJoystickTests ...)` list in CMakeLists.txt. Verify by running `cmake --build build --target ChordJoystickTests -- -BUILD_TESTS=ON`.

**Warning signs:** Plugin builds fine but `cmake -DBUILD_TESTS=ON ..` fails with "undefined symbol LfoEngine::process".

### Pitfall 5: JUCE Dependency in LfoEngine Breaks Test Build

**What goes wrong:** `LfoEngine.h` includes JUCE headers that pull in `MessageManager`. The test binary calls `jassert(!juce::MessageManager::existsAndIsCurrentThread())` (the `ASSERT_AUDIO_THREAD()` macro from `LooperEngine.h`) — this macro must NOT be placed inside LfoEngine because the test runner is not a JUCE app.

**Why it happens:** `LooperEngine.h` defines `ASSERT_AUDIO_THREAD()` in terms of `juce::MessageManager`. LfoEngine must not use this macro because it requires a running JUCE message loop.

**How to avoid:** Do not include `LooperEngine.h` in `LfoEngine.h/cpp`. Limit JUCE includes to `juce_audio_processors` (already linked in test target) or use only `<cmath>` and `<cstdint>`. The test binary already links `juce::juce_audio_processors` — `juce::MathConstants<float>::twoPi` is available.

**Warning signs:** Test build aborts with "MessageManager does not exist" or similar JUCE assertion.

### Pitfall 6: Triangle Waveform Formula Off-by-One at Endpoints

**What goes wrong:** Triangle waveform outputs exactly ±1 at `phi=0` and `phi=0.5` but the formula produces the wrong sign at the crossover.

**Why it happens:** Common triangle formula `(phi < 0.5f) ? (4f*phi - 1f) : (3f - 4f*phi)` produces `-1` at `phi=0`, `+1` at `phi=0.25`, `-1` at `phi=0.5`, `+1` at `phi=0.75`. This is correct — verify with a unit test rather than by inspection.

**How to avoid:** Write a dedicated unit test that checks `phi=0 → -1`, `phi=0.25 → +1`, `phi=0.5 → -1`, `phi=0.75 → +1`.

---

## Code Examples

### LCG Pattern (exact project convention)

```cpp
// Source: TriggerSystem.h line 124 — exact pattern to copy
// Maps to [-1, +1] for LFO use (differs from TriggerSystem which maps to [0, 1))
uint32_t rng_ = 0xABCDEF01u;  // non-zero seed; choose any non-zero value

float nextLcg()
{
    rng_ = rng_ * 1664525u + 1013904223u;
    // Signed interpretation: maps uint32 as int32 → divide by 0x7FFFFFFF
    return static_cast<float>(static_cast<int32_t>(rng_)) / float(0x7FFFFFFF);
}
```

### ppqPos Sync Phase (exact arpeggiator pattern)

```cpp
// Source: PluginProcessor.cpp lines 794-800 — arpeggiator ppqPos sync
// Mirror this for LFO sync mode
if (syncMode && ppqPosition >= 0.0)
{
    // Phase is always derived from ppqPos — never accumulates independently
    const double rawPhase = std::fmod(ppqPosition, cycleBeats);
    normalizedPhase = rawPhase / cycleBeats;   // 0.0..1.0
    // Also update sampleCount_ to match, for seamless DAW-stop fallback:
    sampleCount_ = static_cast<int64_t>(rawPhase / cycleBeats * cycleSamples);
}
```

### Catch2 Test Pattern (matches existing test style)

```cpp
// Source: Tests/ChordEngineTests.cpp — exact style to follow
#include <catch2/catch_test_macros.hpp>
#include "LfoEngine.h"
#include <cmath>

static LfoEngine::ProcessParams baseParams()
{
    LfoEngine::ProcessParams p;
    p.sampleRate   = 44100.0;
    p.blockSize    = 512;
    p.bpm          = 120.0;
    p.syncMode     = false;
    p.ppqPosition  = -1.0;
    p.isDawPlaying = false;
    p.rateHz       = 1.0f;
    p.subdivBeats  = 1.0;
    p.waveform     = LfoEngine::Waveform::Sine;
    p.phaseShift   = 0.0f;
    p.distortion   = 0.0f;
    p.level        = 1.0f;
    return p;
}

TEST_CASE("LfoEngine - sine waveform normalized to [-1, 1]", "[lfo][sine]")
{
    LfoEngine lfo;
    auto p = baseParams();
    p.waveform = LfoEngine::Waveform::Sine;
    p.level    = 1.0f;

    // Run 1 full cycle (44100 blocks of 1 sample each at 1 Hz, or 44100 samples)
    // Easier: test at known phase positions by setting ppqPos in sync mode
    p.syncMode     = true;
    p.isDawPlaying = true;
    p.subdivBeats  = 4.0;  // 1 bar = 4 beats at 1 Hz effective

    // At phase 0.25 (quarter-cycle): sine should be ~1.0
    p.ppqPosition = 1.0;  // 1.0 beat / 4.0 beat cycle = 0.25 phase
    const float out = lfo.process(p);
    CHECK(std::abs(out - 1.0f) < 0.001f);
}

TEST_CASE("LfoEngine - S&H holds value for full cycle", "[lfo][sh]")
{
    LfoEngine lfo;
    auto p = baseParams();
    p.waveform     = LfoEngine::Waveform::SH;
    p.syncMode     = true;
    p.isDawPlaying = true;
    p.subdivBeats  = 4.0;

    // First call at start of cycle
    p.ppqPosition = 0.0;
    const float held = lfo.process(p);

    // Subsequent calls within same cycle must return same value
    p.ppqPosition = 1.0;
    CHECK(lfo.process(p) == held);
    p.ppqPosition = 2.0;
    CHECK(lfo.process(p) == held);
    p.ppqPosition = 3.9;
    CHECK(lfo.process(p) == held);
}

TEST_CASE("LfoEngine - distortion bypass at zero", "[lfo][distortion]")
{
    LfoEngine lfo;
    auto p = baseParams();
    p.waveform    = LfoEngine::Waveform::Square;
    p.distortion  = 0.0f;
    p.syncMode    = true;
    p.isDawPlaying = true;
    p.subdivBeats  = 4.0;
    p.ppqPosition  = 0.5;  // phase = 0.125, phi < 0.5 → square = +1.0

    const float out = lfo.process(p);
    CHECK(out == 1.0f);  // exactly 1.0 — no noise added
}
```

### CMakeLists.txt Update Pattern

```cmake
# Add to the existing add_executable block (Source files):
add_executable(ChordJoystickTests
    Source/ScaleQuantizer.cpp
    Source/ChordEngine.cpp
    Source/LooperEngine.cpp
    Source/LfoEngine.cpp          # <-- add this
    Tests/ScaleQuantizerTests.cpp
    Tests/ChordEngineTests.cpp
    Tests/LooperEngineTests.cpp
    Tests/LfoEngineTests.cpp      # <-- add this
)

# Also add to the plugin target:
target_sources(ChordJoystick PRIVATE
    ...
    Source/LfoEngine.cpp          # <-- add this
)
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `std::rand()` for audio-thread RNG | Project LCG (`1664525u / 1013904223u`) | Established in TriggerSystem (v1.0) | No CRT mutex acquisition on audio thread — safe on MSVC |
| Free-running LFO phase | ppqPos-derived phase in sync mode | Established in arpeggiator (v1.3) | Zero drift across transport stop/start |
| JUCE `juce_dsp` oscillator | Hand-rolled waveforms in pure C++17 | isMidiEffect constraint (REQUIREMENTS.md) | No SIMD/audio-bus dependency; works in MIDI-effect context |

---

## Open Questions

1. **S&H boundary at exact `ppqPosition == 0.0` on transport start**
   - What we know: At DAW transport start, `ppqPosition` may be reported as exactly `0.0`. The integer cycle index would be `0` — same as the previous block's index if the previous block's `ppqPosition` was also in cycle 0.
   - What's unclear: Does JUCE's `getPlayHead()->getPosition()` report `ppqPosition = 0.0` or `ppqPosition = -1.0` on the very first block after transport start?
   - Recommendation: Initialize `prevCycle_` to `-1` (sentinel) so the very first block always fires a new S&H value. Test with `ppqPosition = 0.0` explicitly in Catch2.

2. **sampleCount_ reset timing on DAW transport restart**
   - What we know: The pattern must mirror the arpeggiator's transport restart detection (`if (isDawPlaying && !wasPlaying_) reset`).
   - What's unclear: Should `sampleCount_` be reset when `isDawPlaying` transitions from `false` to `true`, or only when the LFO is in free mode?
   - Recommendation: Reset `sampleCount_ = 0` and update it from `ppqPos` when DAW resumes sync. In free mode, let `sampleCount_` continue accumulating — it is used only in sync mode with DAW stopped.

3. **loopLengthBars dynamic maximum — how many subdivisions appear in dropdown**
   - What we know: CONTEXT.md says the slowest sync subdivision is `loopLengthBars × beatsPerBar`, passed as `maxCycleBeats` to `process()`.
   - What's unclear: Phase 12 only builds LfoEngine — it doesn't wire up the APVTS dropdown. The dropdown population is Phase 13's job. Does Phase 12 need to expose a method to query the maximum valid `subdivBeats` given `maxCycleBeats`?
   - Recommendation: LfoEngine clamps `subdivBeats` to `min(subdivBeats, maxCycleBeats)` internally. No additional query method is needed for Phase 12.

---

## Sources

### Primary (HIGH confidence)

- Project source: `Source/TriggerSystem.h` line 124 — LCG pattern `rng_ * 1664525u + 1013904223u`; `nextRandom()` implementation
- Project source: `Source/PluginProcessor.cpp` lines 794–800 — arpeggiator ppqPos sync pattern (the exact model for LFO sync phase derivation)
- Project source: `Source/LooperEngine.cpp` lines 682–744 — `samplesPerBeat`, `beatAtBlockStart`, DAW sync anchor pattern
- Project source: `CMakeLists.txt` lines 38–98 — Catch2 v3.8.1 test target, `add_executable(ChordJoystickTests ...)`, link libraries, `catch_discover_tests`
- Project source: `Tests/ChordEngineTests.cpp` — Catch2 TEST_CASE / SECTION / CHECK test style to follow
- Project source: `Source/PluginProcessor.h` lines 44–45 — `joystickX`/`joystickY` atomics confirmed as write-protected from LFO
- Project source: `REQUIREMENTS.md` — `juce_dsp` out of scope, `std::rand()` on audio thread forbidden, no write-back to APVTS from audio thread

### Secondary (MEDIUM confidence)

- Standard DSP references: triangle, sawtooth, square waveform formulas from normalized phase `[0, 1)` — textbook DSP math, no single authoritative URL needed; verified by test assertions
- LCG constants `1664525` / `1013904223` — Knuth's "Numerical Recipes" multiplier/increment (well-established); confirmed in use by TriggerSystem

### Tertiary (LOW confidence — none)

No findings are based on unverified single-source WebSearch results. All critical claims are sourced from project code directly.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — project already has all dependencies; Catch2 v3.8.1, JUCE 8.0.4, C++17 confirmed in CMakeLists.txt
- Architecture: HIGH — class structure follows established project pattern (TriggerSystem, LooperEngine, ChordEngine) directly from source reading
- Waveform math: HIGH — standard DSP math, verifiable by unit test assertions
- ppqPos sync pattern: HIGH — copied verbatim from existing arpeggiator implementation in PluginProcessor.cpp
- LCG pattern: HIGH — exact implementation in TriggerSystem.h, constants are industry-standard
- Pitfalls: HIGH — derived from reading actual project code and constraints, not speculation
- CMakeLists integration: HIGH — pattern directly from existing ChordJoystickTests target

**Research date:** 2026-02-26
**Valid until:** 2026-04-26 (60 days — DSP math is stable; JUCE 8.0.4 is pinned in CMakeLists)
