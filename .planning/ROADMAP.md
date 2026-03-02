# ChordJoystick — Roadmap

## Milestones

- ✅ **v1.0 MVP** — Phases 01-07 (shipped 2026-02-23)
- ✅ **v1.3 Polish & Quantization** — Phases 09-11 (shipped 2026-02-25)
- ✅ **v1.4 LFO + Clock** — Phases 12-16 (shipped 2026-02-26)
- ✅ **v1.5 Routing + Expression** — Phases 17-25 (shipped 2026-03-02)

## Phases

<details>
<summary>✅ v1.0 MVP (Phases 01-07) — SHIPPED 2026-02-23</summary>

- [x] Phase 01: Build Foundation and JUCE Version Lock (2/2 plans) — completed 2026-02-23
- [x] Phase 02: Core Engine Validation (2/2 plans) — completed 2026-02-23
- [x] Phase 03: Core MIDI Output and Note-Off Guarantee (2/2 plans) — completed 2026-02-23
- [x] Phase 04: Per-Voice Trigger Sources and Random Gate (2/2 plans) — completed 2026-02-23
- [x] Phase 05: LooperEngine Hardening and DAW Sync (3/3 plans) — completed 2026-02-23
- [x] Phase 06: SDL2 Gamepad Integration (4/4 plans) — completed 2026-02-23
- [x] Phase 07: DAW Compatibility, Distribution, and Release (2/2 plans) — completed 2026-02-23

Full details: `.planning/milestones/v1.0-ROADMAP.md`

</details>

<details>
<summary>✅ v1.3 Polish & Quantization (Phases 09-11) — SHIPPED 2026-02-25</summary>

- [x] Phase 09: MIDI Panic and Mute Feedback (2/2 plans) — completed 2026-02-25
- [x] Phase 10: Trigger Quantization Infrastructure (5/5 plans) — completed 2026-02-25
- [x] Phase 11: UI Polish and Installer (4/4 plans) — completed 2026-02-25

Full details: `.planning/milestones/v1.3-ROADMAP.md`

</details>

<details>
<summary>✅ v1.4 LFO + Clock (Phases 12-16) — SHIPPED 2026-02-26</summary>

- [x] **Phase 12: LFO Engine Core** — Isolated LFO DSP class: all 7 waveforms, dual-axis phase accumulators, LCG RNG, distortion, beat detection, audio-thread safety (completed 2026-02-26)
- [x] **Phase 13: processBlock Integration and APVTS** — Wire LfoEngine into the processor: 16 APVTS params, process() call in processBlock, LFO output applied as additive offset in buildChordParams(), phase reset on DAW start/stop (completed 2026-02-26)
- [x] **Phase 14: LFO UI and Beat Clock** — Full LFO panel left of joystick + beat clock dot on Free BPM knob face, all controls attached to APVTS (completed 2026-02-26)
- [x] **Phase 15: Gamepad Preset Control** — Option button toggles preset-scroll mode; D-pad Up/Down send MIDI Program Change; UI shows OPTION indicator (completed 2026-02-26)
- [x] **Phase 16: Distribution** — GitHub v1.4 release with installer binary + release notes; desktop backup (completed 2026-02-26)

Full details: Phase Details section below (Phases 12–16).

</details>

<details>
<summary>✅ v1.5 Routing + Expression (Phases 17-25) — SHIPPED 2026-03-02</summary>

- [x] Phase 17: Bug Fixes (3/3 plans) — completed 2026-02-28
- [x] Phase 18: Single-Channel Routing (3/3 plans) — completed 2026-03-01
- [x] Phase 19: Sub Octave Per Voice (2/2 plans) — completed 2026-03-01
- [x] Phase 20: Random Trigger System Extensions (3/3 plans) — completed 2026-03-01
- [x] Phase 21: Left Joystick Modulation Expansion (2/2 plans) — completed 2026-03-01
- [x] Phase 22: LFO Recording (3/3 plans) — completed 2026-03-01
- [x] Phase 23: Arpeggiator (2/2 plans) — completed 2026-03-01
- [x] Phase 24: Gamepad Option Mode 1 (3/3 plans) — completed 2026-03-01
- [x] Phase 24.1: LFO Joystick Visual Tracking (2/2 plans) — completed 2026-03-02
- [x] Phase 25: Distribution (2/2 plans) — completed 2026-03-02

Full details: `.planning/milestones/v1.5-ROADMAP.md`

</details>

## Progress

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 01–07 | v1.0 | 17/17 | Complete | 2026-02-23 |
| 09–11 | v1.3 | 11/11 | Complete | 2026-02-25 |
| 12–16 | v1.4 | 9/9 | Complete | 2026-02-26 |
| 17–25 | v1.5 | 25/25 | Complete | 2026-03-02 |
