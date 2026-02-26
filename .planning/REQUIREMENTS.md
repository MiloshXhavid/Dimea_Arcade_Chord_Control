# Requirements: ChordJoystick MK2

**Defined:** 2026-02-26
**Core Value:** XY joystick mapped to harmonic space — per-note trigger gates, scale quantization, gesture looper with trigger quantization, and gamepad control — no competitor provides this as a unified instrument.

## v1.4 Requirements

### LFO System

- [ ] **LFO-01**: User can enable/disable X-axis LFO independently (On/Off toggle)
- [ ] **LFO-02**: User can enable/disable Y-axis LFO independently (On/Off toggle)
- [ ] **LFO-03**: User can select waveform shape per LFO via dropdown (Sine / Triangle / Saw Up / Saw Down / Square / S&H / Random)
- [ ] **LFO-04**: When Sync OFF → frequency slider (free-running rate in Hz); when Sync ON → same control becomes subdivision selector (1/1, 1/2, 1/4, 1/8, 1/16, 1/32)
- [ ] **LFO-05**: User can set LFO phase shift via slider (0–360°)
- [ ] **LFO-06**: User can set LFO distortion (additive jitter/noise amount) via slider
- [ ] **LFO-07**: User can set LFO level (modulation depth / amplitude) via slider
- [ ] **LFO-08**: User can sync LFO to tempo via a single Sync button — syncs to Free BPM; when DAW sync is also active, follows DAW BPM
- [ ] **LFO-09**: LFO X output modulates joystick X axis as additive offset (output clamped to -1..1)
- [ ] **LFO-10**: LFO Y output modulates joystick Y axis as additive offset (output clamped to -1..1)
- [ ] **LFO-11**: LFO section is positioned to the left of the joystick pad; joystick shifted right to accommodate

### Beat Clock

- [ ] **CLK-01**: A flashing dot next to the Free BPM knob pulses once per beat (internal free tempo)
- [ ] **CLK-02**: Beat dot follows DAW clock (pulses on DAW beat) when DAW sync is active

### Performance & Stability

- [ ] **PERF-01**: LFO S&H / Random waveforms use the project's existing LCG pattern (not std::rand) — audio-thread safe under MSVC
- [ ] **PERF-02**: LFO output is applied as an additive local float offset inside buildChordParams() — never writes to shared joystick atomics
- [ ] **PERF-03**: Synced LFO phase derived from ppqPos (DAW mode) or sample counter (free mode) — no free-accumulation drift across transport stops

### Distribution

- [ ] **DIST-01**: GitHub v1.4 release created with built installer binary and release notes
- [ ] **DIST-02**: Full plugin copy backed up to Desktop/Dima_plug-in

## Future Requirements (v2+)

### LFO Extensions

- **LFO-EXT-01**: LFO modulation of filter CC (CC74/CC71) as a second injection point
- **LFO-EXT-02**: Triplet and dotted note sync subdivisions (1/4T, 1/8T, etc.)
- **LFO-EXT-03**: LFO-to-LFO modulation (X LFO modulates Y LFO rate)

### Beat Clock

- **CLK-EXT-01**: Audible metronome click (MIDI note on dedicated channel) — requires routing decision

## Out of Scope

| Feature | Reason |
|---------|--------|
| juce_dsp module | isMidiEffect()=true — SIMD audio buffer abstractions meaningless |
| LFO modulation written back to APVTS | Illegal from audio thread; would impose 20ms smoother |
| Continuous pitch vibrato from LFO | Existing pitch is sample-and-hold at trigger time — LFO varies pitch at next trigger |
| Soft sync (LFO gradually drifts to phase) | Hard reset on transport start is industry standard; soft sync adds complexity with no UX benefit |

## Traceability

*Populated by roadmapper.*

| Requirement | Phase | Status |
|-------------|-------|--------|
| LFO-01 | — | Pending |
| LFO-02 | — | Pending |
| LFO-03 | — | Pending |
| LFO-04 | — | Pending |
| LFO-05 | — | Pending |
| LFO-06 | — | Pending |
| LFO-07 | — | Pending |
| LFO-08 | — | Pending |
| LFO-09 | — | Pending |
| LFO-10 | — | Pending |
| LFO-11 | — | Pending |
| CLK-01 | — | Pending |
| CLK-02 | — | Pending |
| PERF-01 | — | Pending |
| PERF-02 | — | Pending |
| PERF-03 | — | Pending |
| DIST-01 | — | Pending |
| DIST-02 | — | Pending |

**Coverage:**
- v1.4 requirements: 18 total
- Mapped to phases: 0 (roadmap pending)
- Unmapped: 18 ⚠️

---
*Requirements defined: 2026-02-26*
*Last updated: 2026-02-26 — initial v1.4 definition*
