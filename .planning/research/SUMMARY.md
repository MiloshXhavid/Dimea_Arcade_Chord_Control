# Project Research Summary

**Project:** ChordJoystick MK2 — v1.4 LFO + Beat Clock
**Domain:** JUCE VST3 MIDI effect plugin — dual LFO modulation engine + beat clock indicator
**Researched:** 2026-02-26
**Confidence:** HIGH

## Executive Summary

ChordJoystick v1.4 adds a dual LFO modulation system that offsets the plugin's virtual joystick X and Y axes, driving pitch variation on all four voices through the existing scale quantizer and chord engine. Research confirms this is a well-contained, low-risk addition: zero new external dependencies are required, all new code follows patterns already established in the v1.3 codebase (phase accumulators, LCG RNG, atomic display values, APVTS parameter registration), and the existing lock-free audio thread architecture accommodates the LFO cleanly without any structural changes to PluginProcessor, ChordEngine, or the other subsystems.

The recommended approach is a self-contained `LfoEngine` class with audio-thread-only state (phase accumulators, RNG, waveform output floats), called from `processBlock` at step 4 (after DAW playhead query, before looper), with its output applied as an additive offset inside `buildChordParams()` after the gamepad/looper joystick resolution — never written back to the `joystickX`/`joystickY` atomics. This preserves the looper's ability to record and play back raw gestures independently of LFO state. A beat clock indicator reuses the existing `flashPanic_`-style `std::atomic<int>` counter pattern and the existing 30 Hz `timerCallback`, adding approximately 15 lines of UI code. 14 new APVTS parameters (7 per axis) complete the feature set.

The two design decisions requiring upfront commitment before coding are: (1) using a private LCG (`uint32_t` seed) instead of `std::rand()` for the S&H waveform's PRNG — `std::rand()` acquires a CRT mutex on MSVC and must never be called from the audio thread — and (2) applying the LFO output offset inside `buildChordParams()` to the local `chordP` struct, not to the shared atomics. Both decisions are pre-made by this research and have direct code templates ready.

---

## Key Findings

### Recommended Stack

No new libraries, modules, or CMake targets are needed for v1.4. All seven LFO waveforms are implemented with `<cmath>` (already `#include`-d in every relevant translation unit) and plain arithmetic. `juce_dsp` is explicitly ruled out: its `Oscillator<float>` requires `ProcessSpec` and `AudioBlock<float>` which are audio-buffer abstractions meaningless in an `isMidiEffect()=true` plugin. The beat clock indicator uses only `juce_gui_basics` (already linked) and the existing 30 Hz `juce::Timer`. The existing locked stack — JUCE 8.0.4, SDL2 2.30.9 static, CMake FetchContent, C++17, MSVC — is fully sufficient.

**Core technologies used (all existing, no additions):**
- `<cmath>` (std::sin, std::fmod, std::floor): all 7 LFO waveform shapes — already included in every relevant TU
- `juce_audio_processors` APVTS: 14 new parameters following the exact existing registration pattern
- `juce::jlimit`: joystick clamp after LFO offset addition (in scope via `juce_core`)
- `std::atomic<float/int>`: LFO display output and beat-fired counter (same pattern as `effectiveBpm_`, `filterCutDisplay_`)
- `juce::ScopedNoDenormals`: already at line 359 of `processBlock` — all LFO computation runs inside this scope

**New files required:**
- `Source/LfoEngine.h` — LfoEngine class, dual LfoState structs, Params struct, interface
- `Source/LfoEngine.cpp` — all waveform math, sync logic, beat boundary detection, distortion

**Modified files:** `PluginProcessor.h`, `PluginProcessor.cpp`, `PluginEditor.h`, `PluginEditor.cpp`, `CMakeLists.txt` (one `target_sources` line addition)

### Expected Features

All features are fully specified and unambiguous. The waveform math is standard DSP with multiple verified implementations.

**Must have (table stakes):**
- All 7 waveforms: Sine, Triangle, Saw Up, Saw Down, Square, Sample & Hold (stepped), Random (smooth) — any omission makes the LFO feel unfinished
- Frequency slider (0.01–20 Hz free mode) — the most fundamental LFO parameter
- Level/depth slider (0.0–1.0) — scales LFO output before adding to joystick
- On/Off toggle per axis — bypass without losing parameter state
- BPM sync toggle + subdivision selector — essential for live performance context; syncs to DAW via PPQ or `randomFreeTempo` when DAW is stopped
- Phase offset (0.0–1.0) — sets start angle; enables Lissajous-style X/Y motion when set differently per axis
- Beat clock indicator — one flash per quarter note; essential visual tempo feedback in live use

**Should have (differentiators):**
- Distortion/jitter slider — additive white noise mixed into LFO output (Option A: Ableton M4L "Jitter" model); prevents robotic repetition; low complexity
- Hard phase reset on DAW play start — standard in Serum/Vital/DIVA; DAW users expect this
- PPQ-derived phase in sync mode — eliminates drift on DAW loop/seek/reposition; prevents the most common LFO sync complaint
- 8 sync subdivisions (1/16 to 8 bars) — covers standard musical use cases

**Defer to v2+:**
- Triplet and dotted subdivisions — adds UI complexity; 8 standard subdivisions covers 95% of use cases
- Variable pulse width for Square — 50% duty cycle sufficient for v1.4
- Smooth random cosine interpolation — linear interpolation is indistinguishable in practice
- Custom LFO shape drawing — weeks of scope; Serum-tier feature
- Per-voice LFO (4 separate LFOs) — 4 voices share 2 axes; global dual LFO is the correct design
- LFO envelope (attack/decay on LFO output) — no per-trigger model applicable in this plugin's architecture

**Explicit anti-features (do not build):**
- LFO-to-MIDI CC output — LFOs are internal axis modulators, not CC generators
- Modulation routing matrix — only 2 destinations exist; fixed routing is correct
- LFO-to-LFO modulation — not needed for a 2-destination instrument
- Second beat clock dot — one dot near BPM knob is standard; two would confuse

### Architecture Approach

LfoEngine is an audio-thread-only class with the same structural role as `TriggerSystem` and `LooperEngine` — it receives a `Params` struct built from APVTS reads at the top of `processBlock`, computes new state, and exposes its output via plain float getters. It does not hold APVTS references, does not write to shared atomics (except the `beatFired_` counter used for UI flash), and has no mutex. The entire LFO pipeline sits between the DAW playhead read (step 3 in `processBlock`) and the `buildChordParams()` call (step 6), with its output applied inside `buildChordParams()` as a two-line additive offset clamped to [-1, 1].

**Major components:**

1. `LfoEngine` (new) — dual X+Y phase accumulators, 7 waveforms, LCG RNG for S&H/Random, BPM sync, distortion, beat boundary detection; audio-thread-only except `beatFired_` atomic
2. `PluginProcessor` (modified) — owns `LfoEngine lfoEngine_` member; registers 14 new APVTS params; calls `lfoEngine_.process()` at block step 4; applies `getOutputX/Y()` inside `buildChordParams()`; exposes `beatFired_` accessor
3. `PluginEditor` (modified) — adds LFO X/Y panels; reads `beatFired_` in existing 30 Hz `timerCallback()` to flash the beat dot; no second timer
4. `BeatClockDot` (new small component) — filled circle that calls `repaint()` only on lit/unlit state change; positioned near BPM knob

**Unmodified components (explicitly confirmed by source inspection):** `ChordEngine`, `ScaleQuantizer`, `LooperEngine`, `TriggerSystem`, `GamepadInput`

**processBlock call sequence (LFO position annotated):**
```
1. gamepad deadzone sync
2. gamepad poll (buttons, sticks)
3. DAW playhead -> ppqPos, dawBpm, isDawPlaying
4. >>> lfoEngine_.process(lp) <<<   [NEW — advances phase, writes outputX_/Y_, detects beat]
5. looper_.process(lp)              [may override joystickX/Y atomics from playback]
6. buildChordParams()               [LFO offset applied HERE as local additive clamp]
7. ChordEngine::computePitch() x4
8. TriggerSystem::processBlock()
9. arpeggiator, filter CC, looper config sync
```

### Critical Pitfalls

1. **`std::rand()` or `std::mt19937` for S&H on the audio thread** — `std::rand()` acquires a CRT mutex on MSVC; audio thread blocking causes intermittent xruns. Prevention: use the same LCG already in the codebase (`seed = seed * 1664525u + 1013904223u`) with a private `uint32_t lfoXRandSeed_` member. Initialize to a nonzero constant; never use 0.

2. **LFO output written into `joystickX`/`joystickY` atomics** — the looper also writes to these atomics during playback; source conflict causes either LFO to silently do nothing during looper playback, or LFO to erase recorded joystick gestures. Prevention: apply LFO as additive offset to the local `chordP.joystickX/Y` floats inside `buildChordParams()` — never write back to the atomics.

3. **Beat-synced LFO phase drift on DAW stop/start cycles** — a free-running accumulator in sync mode drifts out of alignment with the DAW grid. Prevention: when `sync=true` and DAW is playing, derive phase directly from `ppqPos`: `lfoPhase = fmod(ppqPos / lfoBeatsPerCycle, 1.0)`. Use the free-running accumulator only when DAW is stopped or sync is off.

4. **APVTS parameter ID collision** — JUCE silently allows duplicate IDs and produces unpredictable runtime behavior; no crash, no assertion. Prevention: prefix all new LFO params with `"lfo"` (e.g., `"lfoXShape"`, `"lfoXFreq"`); grep for duplicates after registration: `grep -o '"[a-zA-Z0-9_]*"' Source/PluginProcessor.cpp | sort | uniq -d`. Verify v1.3 presets load with LFO params at safe defaults (enabled=false, level=0).

5. **Second UI timer added for LFO display** — confirmed 40% CPU spike from prior research. Prevention: read LFO phase from `std::atomic<float> lfoPhaseDisplay_` in the existing 30 Hz `timerCallback()`. Hide the phase cursor at rates above ~15 Hz where the display aliases anyway. Verify: `grep "startTimerHz\|startTimer" Source/PluginEditor.cpp` returns exactly 1 result after the LFO UI is added.

---

## Implications for Roadmap

Research divides v1.4 into five natural phases following the established build order. All phases are within the existing architecture's capabilities; none require a new subsystem or architectural change to the two-thread model.

### Phase A: LfoEngine Core (DSP foundation)

**Rationale:** The LFO DSP is the irreducible foundation — nothing else can be tested until waveform output is correct. Building this as an isolated new file with no processor or UI changes keeps the first phase low-risk and fully unit-testable using the existing Catch2 test pattern.

**Delivers:** `Source/LfoEngine.h` + `Source/LfoEngine.cpp` with all 7 waveforms, dual-axis phase accumulators, LCG RNG for S&H/Random, beat boundary detection via `beatFired_` atomic, `prepare()` and `process()` interface, distortion (additive noise) post-processing.

**Addresses features:** All 7 waveforms, distortion slider behavior, S&H stepped behavior (instantaneous jump at cycle wrap), smooth Random interpolation (linear lerp toward new target), beat crossing detection.

**Avoids:** Pitfall 1 (use LCG not `std::rand()`), Pitfall 9 (initialize RNG seed to nonzero), Pitfall 10 (`fmod` not while-loop for phase wrap), Pitfall 11 (normalize all waveforms to [-1, 1] before level scaling to prevent DC offset).

**No research-phase needed:** Waveform math is standard; LCG pattern is verbatim from existing codebase.

### Phase B: processBlock Integration + APVTS Parameters

**Rationale:** APVTS parameter registration must happen before any UI control can attach. Integrating `lfoEngine_.process()` into `processBlock` alongside parameter reads validates the audio-thread contract and makes the LFO audible via MIDI output change before the UI exists.

**Delivers:** 14 new APVTS parameters in `createParameterLayout()`; `LfoEngine lfoEngine_` member on `PluginProcessor`; `lfoEngine_.prepare()` in `prepareToPlay()`; `LfoEngine::Params` struct built and passed per block; `getOutputX/Y()` applied inside `buildChordParams()` as two additive clamped lines; `dawJustStarted -> lfoEngine_.resetPhase()` call; `beatFired_` forwarding accessor; `lfoPhaseDisplay_` and `lfoOutputX_/Y_` atomics for UI reads.

**Addresses features:** BPM sync, PPQ-derived phase, phase reset on DAW start, joystick axis modulation, preset compatibility (defaults are no-op).

**Avoids:** Pitfall 2 (injection point — apply in `buildChordParams()` local scope, not atomics), Pitfall 4 (beat sync drift — PPQ derivation when DAW is playing), Pitfall 5 (APVTS smoother collision — additive local offset, no write-back to params), Pitfall 8 (param ID collision — `"lfo"` prefix, grep dedup after registration).

**No research-phase needed:** APVTS registration pattern is verbatim from the existing ~60-parameter layout.

### Phase C: Beat Clock UI

**Rationale:** The beat clock dot is the simplest UI change and can run concurrently with Phase D (different UI region). It reuses two confirmed patterns: the `flashPanic_`/`flashLoopReset_` edge-detection idiom and the existing 30 Hz `timerCallback`.

**Delivers:** `BeatClockDot` component (filled circle, `setLit()` calls `repaint()` on state change only); `timerCallback()` reads `beatFired_`, detects increment, sets `beatDotLit_` for 2 timer ticks (~66ms flash); positioned near BPM knob.

**Addresses features:** Beat clock indicator (one flash per quarter note), free-BPM fallback when DAW is stopped.

**Avoids:** Pitfall 7 (no second timer — uses existing `timerCallback` frame countdown pattern).

**No research-phase needed:** Pattern is identical to existing `flashLoopReset_` / `flashPanic_` implementation.

### Phase D: LFO UI Section

**Rationale:** UI is the highest-effort phase and has the most layout decisions. Must come after Phase B because `AudioParameterFloat/Bool/Choice` attachments require registered APVTS params. Phase C and Phase D can be developed concurrently since they are in different UI regions.

**Delivers:** Two LFO panels (X-axis and Y-axis, each with shape dropdown, Rate/Level/Phase/Distortion sliders, Enabled toggle, Sync button, sync subdivision dropdown); beat clock dot component.

**Addresses features:** Full per-axis parameter control with host automation support; preset save/load via APVTS.

**Avoids:** Pitfall 7 (UI display rate aliasing — hide phase cursor when rate exceeds ~15 Hz; no 60 Hz timer).

**Needs attention during planning:** The existing editor is space-constrained (joystick pad, 4 touchplates, scale keyboard, control rows). A layout decision (side panel, collapsible section, or tabbed area) should be made before writing `resized()` to avoid rework. This is a planning decision, not a research gap.

### Phase E: Testing + Validation

**Rationale:** Cross-cutting validation before the milestone is complete. Covers the correctness conditions that are silent bugs (no crash, no assertion) if missed.

**Delivers:** Unit tests for LFO waveform output ranges and S&H single-value-per-cycle behavior; backward-compatibility test (load v1.3 preset — all existing params preserved, LFO params at safe defaults); grep-based APVTS ID dedup confirmation; manual verification that LFO and looper joystick playback combine correctly (both modulations visible in MIDI output simultaneously); pluginval scan.

**Addresses:** "Looks Done But Isn't" checklist from PITFALLS.md (12 items covering injection point, PRNG safety, seed value, ScopedNoDenormals position, accumulator precision, sync derivation, timer count, phase reset flag, ID uniqueness, display handling, APVTS read pattern, write-back absence).

**No research-phase needed:** Test patterns established by existing `ChordEngineTests.cpp` and `ScaleQuantizerTests.cpp`.

### Phase Ordering Rationale

- Phase A before Phase B: `LfoEngine::Params` struct and interface must be stable before `processBlock` builds it.
- Phase B before Phase C and D: `beatFired_` accessor (C) and APVTS parameter attachments (D) require Phase B's additions.
- Phase C and D are mutually independent and can run concurrently.
- Phase E last: integration tests require all prior phases complete.

The five-phase build order matches the MVP build order recommendation in FEATURES.md exactly.

### Research Flags

**Phases with standard patterns — no research-phase needed:**
- **Phase A (LfoEngine core):** All waveform formulas verified against multiple independent sources. LCG is verbatim from codebase. Block-rate update is established precedent.
- **Phase B (processBlock integration):** Every integration point verified by direct source inspection of all 1044 lines. APVTS registration is mechanical.
- **Phase C (beat clock UI):** Pattern identical to 3 existing flash counters already implemented.
- **Phase E (testing):** Catch2 patterns established by existing test files.

**Phase warranting pre-coding design decision (not research):**
- **Phase D (LFO UI layout):** No technical uncertainty. A layout sketch or explicit decision about panel placement should be made before writing `resized()` to avoid rework in a space-constrained editor.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All claims verified from CMakeLists.txt lines 141–148 and JUCE 8.0.4 source in `build/_deps/juce-src`. Zero new dependencies confirmed by inspection, not assumption. |
| Features | HIGH | Waveform math verified against musicdsp.org, JUCE forum, and KVR. S&H vs Random distinction verified across Sweetwater, KVR, and synth literature. Distortion as additive noise rated MEDIUM — Ableton M4L interpretation is conventional but not formally spec'd for this plugin. |
| Architecture | HIGH | All integration points verified by full source read of `PluginProcessor.cpp` (1044 lines), `PluginProcessor.h`, `LooperEngine.h`, `TriggerSystem.h`, `ChordEngine.h`. No inference — findings based on actual source. |
| Pitfalls | HIGH | Critical pitfalls (std::rand CRT mutex, LFO injection conflict, PPQ drift) verified from both source code and JUCE forum/KVR community. Minor pitfalls (zero seed, fmod vs while, DC offset) verified from well-sourced DSP community references. |

**Overall confidence:** HIGH

### Gaps to Address

- **LFO UI layout:** The only unresolved question is where the LFO panels fit in the existing editor. This is a design decision for Phase D planning, not a research gap.

- **TriggerSystem delta behavior with LFO active:** ARCHITECTURE.md flags this as a deliberate design decision requiring confirmation. Post-LFO `chordP.joystickX/Y` values drive the `deltaX`/`deltaY` computation in TriggerSystem, meaning LFO motion will retrigger joystick-source voices ("LFO feels like moving the joystick"). This is the recommended default, but should be confirmed before Phase B is finalized. The alternative (pre-LFO delta, LFO modulates pitch only without retriggering) requires a 2-line change to the delta computation.

- **Distortion parameter interpretation:** FEATURES.md rates this MEDIUM confidence. "Additive white noise" (Option A) matches Ableton M4L "Jitter" documentation. Confirm whether the v1.4 spec intends noise injection or waveform shaping before Phase A's `computeWave()` distortion path is written.

---

## Sources

### Primary (HIGH confidence — direct source inspection)

- `CMakeLists.txt` lines 141–148 — confirmed juce_dsp NOT linked; full linked library list verified
- `Source/PluginProcessor.cpp` — full processBlock (1044 lines), buildChordParams, createParameterLayout, arpRandSeed_ LCG at lines 850–851, ScopedNoDenormals at line 359, looper joystick override at lines 469–471
- `Source/PluginProcessor.h` — member layout, atomic pattern for effectiveBpm_/filterCutDisplay_, flash counter pattern, joystickX/Y atomics at lines 44–45, arpRandSeed_ at line 234
- `Source/TriggerSystem.h` — LCG RNG at line 124 (`seed * 1664525u + 1013904223u`), ProcessParams struct with deltaX/Y fields
- `Source/LooperEngine.h` — ASSERT_AUDIO_THREAD macro, pendingQuantize_ flag pattern, lock-free comments
- `Source/ChordEngine.h` — Params struct, joystickX/Y field semantics
- `build/_deps/juce-src/` — JUCE 8.0.4 tag confirmed

### Secondary (MEDIUM confidence — community and documentation)

- JUCE Forum: "State of the Art Denormal Prevention" — ScopedNoDenormals FTZ/DAZ confirmation; RAII MXCSR pattern
- JUCE Forum: "Sync to host and LFO" — PPQ-derived phase as drift-free approach
- JUCE Forum: "Blinking button in LookAndFeel / timerCallback?" — repaint-on-state-change as JUCE-recommended blink pattern
- JUCE Forum: "AudioProcessorValueTreeState scalability" — 14 new params is negligible; thousands of params degrade flushParameterValues
- KVR Audio: "random generator for musical domain — xorshift vs LCG" — LCG safe on audio thread; std::rand CRT mutex confirmed
- musicdsp.org Algorithm #269 — smooth random LFO generator (Random waveform interpolation)
- Sweetwater InSync: "A Simple Guide to Modulation: Sample & Hold" — S&H vs smooth distinction
- Ableton Reference Manual v12: Max for Live LFO Devices — Jitter slider as additive noise (Option A rationale)
- JUCE Step by Step Blog "7b. LFO Sync" — BPM derivation and PPQ phase calculation
- KVR Audio DSP Forum: "How to code a random LFO?" — waveform implementations

---

*Research completed: 2026-02-26*
*Ready for roadmap: yes*
