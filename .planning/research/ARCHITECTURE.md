# Architecture Patterns: LFO Integration

**Project:** ChordJoystick v1.4 — Dual LFO Engine
**Domain:** JUCE VST3 MIDI Plugin — audio-thread-safe modulation subsystem
**Researched:** 2026-02-26
**Overall confidence:** HIGH — based on direct source inspection of all relevant files

---

## Scope

This document answers the architectural integration questions for ChordJoystick v1.4:

1. Where does LfoEngine sit in the processBlock() call sequence?
2. Where do LFO outputs feed into the joystick pipeline?
3. How does LfoEngine maintain the lock-free guarantee?
4. How do sync and phase reset work on the audio thread?
5. How does BeatClock work at the block boundary level?
6. What is new vs. modified vs. untouched?
7. Recommended build order across phases.

---

## Integration Point Map

```
processBlock() — annotated execution order
────────────────────────────────────────────────────────────────────
1.  gamepad_.setDeadZone(...)
2.  [GAMEPAD] Poll triggers / buttons / D-pad / stick axes
3.  [DAW]     getPlayHead() -> ppqPos, dawBpm, isDawPlaying
4.  [NEW] >>> lfoEngine_.process(lp) <<<  ← insert here
             Advances phase accumulators.
             Writes outputX_, outputY_ (audio-thread plain floats).
             Increments beatFired_ atomic on beat boundaries.
5.  [LOOPER]  looper_.process(lp)
             May write joystickX / joystickY atomics from playback.
6.  [CHORD]   buildChordParams()       ← LFO offset applied INSIDE here
7.  [CHORD]   ChordEngine::computePitch(v, chordP) x 4
8.  [TRIGGER] trigger_.processBlock(tp)
9.  [ARP]     arpeggiator logic
10. [FILTER]  filter CC section (CC74 / CC71)
11. [LOOPER]  config sync (subdiv / length / quantize push)
────────────────────────────────────────────────────────────────────
```

LfoEngine::process() runs **after DAW playhead query** (needs ppqPos for sync) and **before looper_.process()** (looper may override joystickX/Y atomics; LFO does not touch atomics, so order relative to looper is safe either way — but before is cleaner and avoids any future confusion).

---

## Where LFO Output Feeds Into the Joystick Pipeline

### Key Observation

Reading `buildChordParams()` in full: `joystickX`/`joystickY` atomics are loaded into **local floats** at line:

```cpp
const float jx  = joystickX.load();
const float jy  = joystickY.load();
```

These local floats are composed with gamepad values into `p.joystickX` and `p.joystickY` and passed by value to `ChordEngine::Params`. The atomics themselves are never written inside `buildChordParams()`. This is the correct application site for LFO.

### Application Site — Inside buildChordParams()

```cpp
// Existing (paraphrased):
p.joystickX = gpActive ? gpXs : jx;   // jx = joystickX.load()
p.joystickY = gpActive ? gpYs : jy;   // jy = joystickY.load()

// After LFO integration — add these two lines immediately after:
p.joystickX = juce::jlimit(-1.0f, 1.0f, p.joystickX + lfoEngine_.getOutputX());
p.joystickY = juce::jlimit(-1.0f, 1.0f, p.joystickY + lfoEngine_.getOutputY());
```

`jlimit` is required: LFO can push the combined value outside -1..+1, and ChordEngine expects normalized joystick range.

### Why NOT Write LFO Into the Atomics

`joystickX` / `joystickY` atomics serve three consumers beyond the chord engine:

1. **LooperEngine::recordJoystick()** — records the raw gesture. If LFO is baked into the atomic, playback of the loop re-applies LFO to an already-modulated signal (double modulation).
2. **UI joystick dot display** — the PluginEditor reads joystickX/Y to paint the crosshair position. LFO baked into the atomic would move the on-screen dot in a way that misrepresents the actual held position.
3. **TriggerSystem deltaX / deltaY computation** — deltaX = `chordP.joystickX - prevJoyX_`. Since chordP is already LFO-modified (after the change to buildChordParams), the delta calculation reflects post-LFO movement. This is **intentional**: LFO motion can drive joystick-source voice retriggering.

---

## Data Flow Diagram

```
[UI drag / gamepad stick]
         |
         v
   joystickX (atomic<float>)   joystickY (atomic<float>)
         |                           |
         |   [looper playback may override atomics — step 5]
         |                           |
         v                           v
   buildChordParams()  <---  lfoEngine_.getOutputX/Y()
                              (audio-thread plain floats,
                               written by lfoEngine_.process())
         |
         v
   ChordEngine::Params { joystickX, joystickY }  (clamped -1..+1)
         |
         v
   ChordEngine::computePitch(v, params) x 4
         |
         v
   heldPitch_[4]  ->  MIDI note-on / note-off
```

```
lfoEngine_.process()
         |
         +--- outputX_, outputY_  (plain floats, audio thread only)
         |
         +--- beatFired_ (atomic<int>, incremented on beat crossing)
                  |
                  v
         PluginEditor::timerCallback() at 30 Hz
         reads beatFired_, detects increment -> flash beat dot
```

---

## Component Boundaries

### New Components

| Component | File | Responsibility | Thread |
|-----------|------|---------------|--------|
| `LfoEngine` | `Source/LfoEngine.h` / `LfoEngine.cpp` | Dual X+Y LFO: phase accumulator, 7 waveforms, distortion, beat-sync, S&H | Audio thread only (except `beatFired_` atomic) |

### Modified Components

| Component | File | What Changes |
|-----------|------|-------------|
| `PluginProcessor` | `PluginProcessor.h` | Add `LfoEngine lfoEngine_` member; expose `beatFired_` read accessor for UI |
| `PluginProcessor` | `PluginProcessor.cpp` | Call `lfoEngine_.process()` at step 4; apply LFO offsets in `buildChordParams()`; register new APVTS params; call `lfoEngine_.prepare()` in `prepareToPlay()` |
| `PluginEditor` | `PluginEditor.h` / `.cpp` | Add LFO UI section (left of joystick); read `beatFired_` in `timerCallback()` for beat dot flash |
| APVTS layout | `PluginProcessor.cpp` `createParameterLayout()` | Register LFO parameters (see APVTS section below) |

### Unmodified Components

| Component | Reason |
|-----------|--------|
| `ChordEngine` | Pure static function; `Params.joystickX/Y` arrive already LFO-modified |
| `ScaleQuantizer` | No dependency on joystick position |
| `LooperEngine` | Reads/writes raw atomics; unaware of LFO |
| `TriggerSystem` | Receives `chordP.joystickX/Y` (already LFO-modified); delta is computed from the post-LFO value, which is correct |
| `GamepadInput` | Provides raw stick values; LFO is downstream |

---

## LfoEngine Class Design

### Recommended Interface

```cpp
class LfoEngine
{
public:
    // --- Params struct (built by processBlock, passed to process()) ---
    // Follows same pattern as TriggerSystem::ProcessParams.
    // All values read from APVTS raw parameters in processBlock.
    struct LfoParams {
        bool  enabled;
        int   shape;       // 0=Sine 1=Tri 2=Saw+ 3=Saw- 4=Square 5=S&H 6=Random
        float rate;        // Hz (free mode) or beats-per-cycle (sync mode)
        float level;       // 0..1 depth multiplier
        float phaseOffset; // 0..1 (0 = 0 degrees)
        float distortion;  // 0..1 jitter amount
        bool  sync;        // true = rate in beats, false = Hz
    };

    struct Params {
        LfoParams x;
        LfoParams y;
        double bpm;
        double ppqPosition; // -1 if not available
        int    blockSize;
        double sampleRate;
    };

    // Called from prepareToPlay (audio thread, guaranteed before first processBlock)
    void prepare(double sampleRate) noexcept;

    // Called every processBlock (audio thread).
    // Advances phase accumulators. Updates outputX_, outputY_. Detects beat crossings.
    void process(const Params& p) noexcept;

    // Audio-thread outputs — plain floats, read only in buildChordParams() (audio thread).
    float getOutputX() const noexcept { return outputX_; }
    float getOutputY() const noexcept { return outputY_; }

    // Beat clock — incremented on audio thread, read on message thread (30 Hz timer).
    // Same pattern as flashLoopReset_ / flashPanic_ in PluginProcessor.
    std::atomic<int> beatFired_ { 0 };

    // Phase reset — called from processBlock when dawJustStarted is true.
    // Resets free-running accumulators only; sync-mode derives phase from ppq.
    void resetPhase() noexcept;

private:
    double sampleRate_ = 44100.0;

    struct LfoState {
        double   phase     = 0.0;        // accumulator, 0..1
        float    shHeld    = 0.0f;       // sample-and-hold output
        uint32_t rng       = 0xDEADBEEF; // LCG seed (Random waveform)
        double   prevPhase = 0.0;        // for crossing detection (S&H trigger)
    };

    LfoState stateX_, stateY_;
    float    outputX_ = 0.0f;
    float    outputY_ = 0.0f;

    // Beat clock state (audio-thread-only except beatFired_)
    double prevPpq_        = -1.0;  // for PPQ-based beat crossing detection
    double freeBeatPhase_  = 0.0;   // free-running beat accumulator

    static float computeWave(LfoState& s, const LfoParams& p,
                             double bpm, double ppq,
                             double sampleRate, int blockSize) noexcept;
};
```

### Design Rationale: Params Struct (Not Internal Setters)

All existing subsystems in processBlock build a local params struct and pass it by const reference: `TriggerSystem::ProcessParams`, `LooperEngine::ProcessParams`. LfoEngine follows the identical pattern. `buildChordParams()` is called `const` on `PluginProcessor` — LfoEngine's process() is not called there, only `getOutputX/Y()`.

This avoids:
- Atomic setters for every LFO config field (7 fields x 2 axes = 14 atomics — noisy).
- LfoEngine holding an APVTS reference (would create an ownership coupling).

The tradeoff is that processBlock must construct `LfoEngine::Params` each block, reading 14 APVTS `getRawParameterValue()` calls. This is identical in cost to existing APVTS reads in processBlock (arpeggiator reads 3 params, filter reads 6+ params per block). No measurable overhead.

### Waveform Computation (per-block, not per-sample)

LFO produces one output value per block. This is sufficient because:
- Joystick pitch is quantized to semitones by ScaleQuantizer; sub-semitone pitch changes have no effect.
- The arpeggiator in the codebase uses per-block beat accumulation as the established precedent.
- Per-sample LFO would add complexity with no audible benefit for discrete-pitch MIDI output.

Exception: if pitch-bend (continuous, not quantized) is ever driven by the LFO, reconsider per-sample computation. Not in scope for v1.4.

---

## BeatClock Design

BeatClock is not a separate class — it is a small section inside `LfoEngine::process()`. It detects beat boundary crossings and increments the `beatFired_` atomic.

### PPQ-Based Detection (DAW sync on)

```cpp
// Inside LfoEngine::process(), when ppqPosition >= 0:
const long long beatAtStart = static_cast<long long>(prevPpq_);
const long long beatAtEnd   = static_cast<long long>(p.ppqPosition
                              + p.bpm * p.blockSize / (p.sampleRate * 60.0));
if (beatAtEnd > beatAtStart)
    beatFired_.fetch_add(1, std::memory_order_relaxed);
prevPpq_ = p.ppqPosition + p.bpm * p.blockSize / (p.sampleRate * 60.0);
```

This is the same beat-grid integer-crossing method used for the arpeggiator's DAW-sync step counting in processBlock (`stepsAtStart` / `stepsAtEnd` pattern) — proven correct in v1.0.

### Free-Running Detection (no DAW playhead)

```cpp
// When ppqPosition < 0:
const double beatsThisBlock = p.bpm * p.blockSize / (p.sampleRate * 60.0);
freeBeatPhase_ += beatsThisBlock;
while (freeBeatPhase_ >= 1.0) {
    freeBeatPhase_ -= 1.0;
    beatFired_.fetch_add(1, std::memory_order_relaxed);
}
```

### UI Consumption in timerCallback (30 Hz)

```cpp
// PluginEditor::timerCallback() — existing pattern for flash_ counters:
const int nowBeat = processor_.getBeatFiredCount();  // forwarding accessor
if (nowBeat != lastBeatCount_) {
    lastBeatCount_ = nowBeat;
    // trigger beat dot flash: set a short repaint countdown
    beatDotLit_ = true;
    beatDotFrames_ = 2;  // 2 timer ticks at 30 Hz = ~66 ms visible
    beatDotComponent_.repaint();
}
if (beatDotFrames_ > 0) {
    --beatDotFrames_;
    if (beatDotFrames_ == 0) {
        beatDotLit_ = false;
        beatDotComponent_.repaint();
    }
}
```

This is identical to the `flashLoopReset_` / `flashLoopDelete_` / `flashPanic_` pattern already implemented in PluginProcessor.h and PluginEditor. No new mechanism is needed.

---

## Sync and Phase Reset

### Sync Mode: Phase Derived from ppqPosition

When `LfoParams::sync = true`, the LFO phase is not accumulated — it is derived directly from the DAW playhead position. This gives sample-accurate beat-locked modulation with automatic alignment.

```cpp
// Sync mode — inside computeWave():
if (p.sync && ppq >= 0.0) {
    const double lfoBeats = 1.0 / (double)p.rate; // rate=2 -> half-bar cycle
    s.phase = std::fmod(ppq / lfoBeats, 1.0) + p.phaseOffset;
    if (s.phase >= 1.0) s.phase -= 1.0;
    // No accumulation — phase is re-derived each block from ppq
}
```

**Key property:** When DAW loops, repositions, or starts from a different position, the LFO phase automatically follows — no explicit reset call is needed in sync mode. This eliminates the most common LFO sync bug.

### Free Mode: Phase Accumulator

```cpp
// Free mode — inside computeWave():
const double phaseInc = p.rate * blockSize / sampleRate;
s.phase = std::fmod(s.phase + phaseInc + p.phaseOffset, 1.0);
```

Phase offset is applied per-block (additive to accumulated phase). For persistent phase offset, it should be subtracted from the starting accumulator when the user sets it — or, simpler: apply it only at output sample point, not in the accumulator. Decision to make at implementation time.

### DAW Start Phase Reset

When the DAW starts playing (dawJustStarted = true in processBlock), free-running LFO accumulators reset to 0. This is already computed in processBlock:

```cpp
// In processBlock, after computing dawJustStarted (already exists):
if (dawJustStarted)
    lfoEngine_.resetPhase();
```

`resetPhase()` sets `stateX_.phase = stateY_.phase = freeBeatPhase_ = 0.0`. Sync-mode LFOs are unaffected (they derive phase from ppq, not the accumulator).

### Sync-to-Free Toggle Mid-Playback

When the user switches a sync LFO to free mode, the accumulator starts from 0 (default). To avoid a phase jump, `resetPhase()` can seed the accumulator from the current ppq phase before the toggle. Implementation detail for the phase; not an architectural constraint.

---

## APVTS Parameters (New for v1.4)

```cpp
// In createParameterLayout():

// ── LFO X (pitch X axis) ──────────────────────────────────────────────────
addBool  ("lfoXEnabled",     "LFO X Enable",       false);
addChoice("lfoXShape",       "LFO X Shape",
          { "Sine","Tri","Saw+","Saw-","Square","S&H","Random" }, 0);
addFloat ("lfoXRate",        "LFO X Rate",         0.01f, 20.0f, 1.0f);
addFloat ("lfoXLevel",       "LFO X Level",        0.0f, 1.0f, 0.0f);
addFloat ("lfoXPhase",       "LFO X Phase",        0.0f, 1.0f, 0.0f);
addFloat ("lfoXDistortion",  "LFO X Distortion",   0.0f, 1.0f, 0.0f);
addBool  ("lfoXSync",        "LFO X Sync",         false);

// ── LFO Y (pitch Y axis) ──────────────────────────────────────────────────
addBool  ("lfoYEnabled",     "LFO Y Enable",       false);
addChoice("lfoYShape",       "LFO Y Shape",
          { "Sine","Tri","Saw+","Saw-","Square","S&H","Random" }, 0);
addFloat ("lfoYRate",        "LFO Y Rate",         0.01f, 20.0f, 1.0f);
addFloat ("lfoYLevel",       "LFO Y Level",        0.0f, 1.0f, 0.0f);
addFloat ("lfoYPhase",       "LFO Y Phase",        0.0f, 1.0f, 0.0f);
addFloat ("lfoYDistortion",  "LFO Y Distortion",   0.0f, 1.0f, 0.0f);
addBool  ("lfoYSync",        "LFO Y Sync",         false);
```

14 new parameters. Defaults are all disabled (lfoXEnabled=false, lfoYEnabled=false, level=0) so existing saved state loads correctly — LFO is a no-op until explicitly enabled.

---

## Anti-Patterns to Avoid

### Anti-Pattern 1: Writing LFO Output Into joystickX/Y Atomics

**What goes wrong:** LooperEngine reads `joystickX`/`joystickY` atomics in `recordJoystick()` to record gestures. Baking LFO into the atomics causes the looper to capture modulated values. On playback, LFO would modulate a loop that already has LFO embedded — double modulation and broken looper semantics. The UI joystick dot would also move under LFO control, misrepresenting the physical stick position.

**Prevention:** Apply LFO only inside `buildChordParams()` as a local float addition. Never store LFO-modulated values back to the atomics.

### Anti-Pattern 2: Any Blocking Synchronisation in LfoEngine::process()

**What goes wrong:** std::mutex, std::condition_variable, or any non-lock-free primitive on the audio thread causes xruns and DAW-level issues. LooperEngine.h already documents this with the ASSERT_AUDIO_THREAD() macro.

**Prevention:** LfoEngine has no mutex. All cross-thread data uses atomics (beatFired_ only) or the params-struct pattern (all other data is audio-thread-local or passed in by value). The APVTS read is done in processBlock before calling process(); LfoEngine::process() receives only a plain Params struct.

### Anti-Pattern 3: Per-Sample Transcendental Functions with Excessive Cost

**What goes wrong:** Calling std::sin() / std::cos() per sample at typical block sizes (64–512 samples) is not inherently wrong on modern CPUs, but it is unnecessary for this use case since pitch is quantized to discrete semitones. It also adds complexity to the implementation.

**Prevention:** Compute one LFO output value per block. Per-block LFO is established precedent in this codebase (arpeggiator uses per-block beat counting). Revisit only if sub-semitone pitch-bend modulation is added in a future version.

### Anti-Pattern 4: Heap Allocation in LfoEngine::process()

**What goes wrong:** Any allocation (std::vector, std::function, juce::String) on the audio thread causes non-deterministic latency spikes. JUCE's memory allocator is not real-time-safe.

**Prevention:** LfoEngine holds all state as plain value members (two LfoState structs, two floats, one double for beat phase). The Params struct is passed by const reference. No dynamic allocation anywhere in the process() path.

### Anti-Pattern 5: Making TriggerSystem Delta Computation Independent of LFO

**What goes wrong:** If the joystick delta (`chordP.joystickX - prevJoyX_`) were computed from the pre-LFO raw value, joystick-source voices would not be retriggered by LFO motion — LFO would only affect pitch of already-held notes, not create new notes. This may or may not be desired.

**Clarification needed:** The current implementation (post-LFO delta) means LFO motion **will** retrigger joystick-mode voices. This is an intentional trade-off to decide before implementing: does the musician want LFO to drive the joystick trigger, or only modulate pitch of triggered notes?

**Recommendation:** Default to using post-LFO position for deltas (drives joystick gates) since it makes the LFO feel like "moving the joystick." If the other behaviour is preferred, compute `tp.deltaX = rawJx - prevJoyX_` (before adding LFO offset) and update `prevJoyX_` from raw position.

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| APVTS param registration | New params change saved state format. Old presets use defaults silently. | Set defaults to no-op (enabled=false, level=0). v1.3 presets load fine. |
| S&H waveform | S&H must hold the same value until the phase wraps 1.0, then sample a new random. Incorrectly resampling every block produces noise, not S&H. | Detect 1.0 crossing via `s.prevPhase` vs `s.phase` in the per-block waveform. |
| Sync mode rate = 0 | Division by zero when computing lfoBeats = 1.0 / rate. | Clamp rate minimum to 0.01 in APVTS or in computeWave() before division. |
| Beat dot at high BPM | At 240 BPM the beat interval is 250 ms. 30 Hz timer tick is 33 ms. Each beat fires beatFired_ once; UI sees it within 33 ms. Acceptable latency. | No action needed; 30 Hz timer is sufficient. |
| prepareToPlay ordering | lfoEngine_.prepare(sampleRate) must run before first processBlock. | Add call in PluginProcessor::prepareToPlay() alongside existing sampleRate_ = sr assignment. Trivial. |
| Phase offset UX | Phase offset slider adjusts waveform start angle. If it feeds directly into the accumulator each block it will be re-added on every block instead of acting as an initial offset. | Apply phase offset only at the output evaluation point (added to accumulated phase before looking up waveform value), not to the accumulator state itself. |
| buildChordParams() is const | `buildChordParams()` is declared `const` on PluginProcessor. `lfoEngine_.getOutputX/Y()` must be `const noexcept`. | LfoEngine::getOutputX/Y() returns `outputX_` which is a plain float member — fine for const. |

---

## Build Order

```
Phase A: LfoEngine core (new file, no modification to existing files)
  - LfoEngine.h + LfoEngine.cpp
  - All 7 waveforms implemented
  - Beat boundary detection (beatFired_)
  - Unit testable in isolation (ChordEngineTests.cpp pattern)
  - No APVTS, no UI, no processor changes
  Deliverable: LfoEngine compiles and waveforms produce correct output

Phase B: ProcessBlock integration (modifies PluginProcessor only)
  - Add LfoEngine lfoEngine_ member to PluginProcessor.h
  - Register 14 LFO APVTS params in createParameterLayout()
  - Call lfoEngine_.prepare(sr) in prepareToPlay()
  - Build LfoEngine::Params from APVTS in processBlock, call lfoEngine_.process()
  - Apply getOutputX/Y in buildChordParams()
  - Add dawJustStarted -> lfoEngine_.resetPhase() call
  Depends on: Phase A
  Risk: APVTS param registration must be complete before any UI attaches

Phase C: BeatClock UI dot (modifies PluginEditor only)
  - Add beatDotComponent_ (small circle Component) near Free BPM knob
  - Read beatFired_ in timerCallback(), edge-detect, flash dot
  Depends on: Phase B (beatFired_ available via processor)
  Note: Can develop in parallel with Phase D (different UI area)

Phase D: LFO UI section (modifies PluginEditor only)
  - Add LFO X and Y panels left of joystick
  - Shape dropdown (AudioParameterChoice attachment)
  - Rate, Level, Phase, Distortion sliders (AudioParameterFloat attachments)
  - Enabled toggle, Sync button
  Depends on: Phase B (APVTS params registered)

Phase E: Distortion waveform post-processing
  - Add jitter noise to LfoEngine::computeWave() based on distortion param
  - Use existing LfoState::rng LCG
  Depends on: Phase A (adds to computeWave())
  Risk: LOW — additive change, does not touch processor or UI
```

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Integration point (step 4 in processBlock) | HIGH | Verified by reading full processBlock source; ppqPos available before LFO call |
| Application site (buildChordParams local floats) | HIGH | Read buildChordParams() in full; p.joystickX/Y are local floats, perfect injection point |
| Lock-free guarantee | HIGH | Entire codebase uses atomics + APVTS raw reads; LfoEngine follows identical pattern |
| beatFired_ pattern | HIGH | flashPanic_ / flashLoopReset_ in PluginProcessor.h and PluginEditor confirm pattern works |
| TriggerSystem delta interaction | MEDIUM | Post-LFO delta will drive joystick gates — need deliberate decision on whether that is desired |
| Per-block vs per-sample LFO | MEDIUM | Per-block sufficient for semitone-quantized pitch; would need revisiting if pitch-bend LFO added |
| Phase offset implementation detail | MEDIUM | Accumulator vs. lookup-time application — two correct approaches; implementation choice at build time |

---

## Sources

- Direct inspection: `Source/PluginProcessor.cpp` — full processBlock() and buildChordParams() (all 1044 lines)
- Direct inspection: `Source/PluginProcessor.h` — member layout, atomics, flash counter pattern, thread annotations
- Direct inspection: `Source/LooperEngine.h` — ASSERT_AUDIO_THREAD macro, lock-free design comments, BlockOutput struct
- Direct inspection: `Source/TriggerSystem.h` — ProcessParams struct, deltaX/Y fields, audio-thread-only contract
- Direct inspection: `Source/ChordEngine.h` — Params struct, joystickX/Y field semantics
- Direct inspection: `Source/GamepadInput.h` — 60 Hz timer, atomic stick values
- Direct inspection: `.planning/PROJECT.md` — v1.4 requirements, key decisions log
- Confidence: HIGH — all findings based on actual source code, no training-data inference used
