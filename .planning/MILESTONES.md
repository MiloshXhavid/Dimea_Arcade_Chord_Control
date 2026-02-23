# Milestones

## v1.0 MVP (Shipped: 2026-02-23)

**Phases completed:** 7 phases, 17 plans, 2 tasks

**Key accomplishments:**
- Reproducible JUCE 8.0.4 build with static CRT — no MSVC runtime dependency, loads in Reaper and Ableton
- 26 Catch2 tests passing — ScaleQuantizer, ChordEngine (15 unit tests), LooperEngine (11 stress/boundary tests)
- 4-voice MIDI note-on/off with note-off guaranteed on all exit paths (bypass, releaseResources, transport stop) — DAW-verified in Reaper
- Per-voice trigger sources: TouchPlate, JOY retrigger (scale-degree boundary model), DAW-synced random gate with density 1–8 hits/bar
- Lock-free LooperEngine (AbstractFifo double-buffer) + Ableton MIDI effect fix — plugin loads correctly on MIDI tracks in Ableton 11
- SDL2 PS4/Xbox gamepad integration — process-level singleton, CC74/CC71 gating, disconnect all-notes-off, full DAW verification
- pluginval strictness level 5 PASSED (19/19 suites) + Inno Setup 3.5 MB installer clean-machine approved — v1.0 release candidate ready

---

