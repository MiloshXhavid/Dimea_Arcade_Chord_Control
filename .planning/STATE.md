# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-24)

**Core value:** XY joystick mapped to harmonic space — per-note trigger gates, scale quantization, gesture looper, gamepad control — no competitor provides this as a unified instrument.
**Current focus:** v1.1 — Phase 10: Trigger Quantization Infrastructure

## Current Position

Phase: 10 of 11 (Trigger Quantization Infrastructure)
Plan: 2 of 4 — In Progress
Status: Phase 10 in progress — Plans 10-01 and 10-02 complete
Last activity: 2026-02-25 — Plan 10-02 complete (quantizeMode + quantizeSubdiv APVTS params + 7 PluginProcessor passthrough methods, build verified)

```
v1.0 MVP    [██████████] SHIPPED 2026-02-23
v1.1 Polish [████░░░░░░] 30% (4/TBD plans)
  Phase 08  [██████████]   Patch Verification    Complete (1 plan done)
  Phase 09  [██████████]   MIDI Panic + Mute     Complete (2 plans done)
  Phase 10  [████░░░░░░]   Quantize Infra        In Progress (2/4 plans done)
  Phase 11  [░░░░░░░░░░]   UI Polish + Installer Not started
```

## Performance Metrics

**v1.0 Velocity:**
- Total plans completed: 17
- Total phases: 7
- Avg plans/phase: 2.4

**v1.1 Velocity:**
- Plans completed: 3
- Phases started: 2
- Phase 09 duration: ~23 min total (2 plans: 09-01 impl ~15 min + 09-02 verify ~8 min)

*Updated after each plan completion*

## Accumulated Context

### Decisions

See .planning/PROJECT.md Key Decisions table — full log with outcomes.

Recent decisions affecting v1.1:
- **NEVER send CC121 in panic sweep** — downstream VST3 instruments (Kontakt, Waves CLA-76) map CC121 to plugin parameters via JUCE VST3 wrapper; use CC64=0 + CC120 + CC123 only
- **Panic is one-shot, not a mute toggle** — press sends 48 CC events (16ch x CC64=0+CC120+CC123), button immediately returns to pressable; no persistent output-blocking state
- **Post-record quantize uses pendingQuantize_ flag** — playbackStore_[] has no mutex; audio thread reads it every block; flag set on message thread, serviced in LooperEngine::process() on audio thread
- **std::fmod(quantized, loopLen) required** — rounding near loop end produces beatPosition == loopLen; fmod prevents silent miss or double-trigger at wrap
- **Single 30 Hz timer only** — no second timer for animations; panic flash and progress bar driven from existing startTimerHz(30) in PluginEditor via phase counters
- **CC64=127 on voice channel only (08-01)** — Each injection uses ch0+1 (voice's own MIDI channel), not filterMidiCh; sustain must be per-voice for downstream synths on channels 1-4
- **Panic sweep is flat for(ch=1..16) loop (09-01)** — no channel deduplication, always hits all 16 channels; old killCh/sent[]/fCh approach removed
- **R3 gamepad calls triggerPanic() directly in processBlock (09-01)** — audio-thread safe, only writes atomics; replaces silent consume
- **quantizeMode/quantizeSubdiv use string literals in PluginProcessor.h (10-02)** — ParamID namespace is file-scope in .cpp, inaccessible from .h; string literals "quantizeMode"/"quantizeSubdiv" used directly in passthrough methods
- **Stub methods return safe defaults before LooperEngine wiring (10-02)** — looperQuantizeIsActive() returns false, stubs are no-ops; allows UI plan (10-04) to compile ahead of LooperEngine plan (10-03)

### Pending Todos

None.

### Blockers/Concerns

- Phase 10 quantize implementation requires Catch2 tests for snapToGrid wrap edge case BEFORE integrating into LooperEngine — do not skip
- Phase 10 QUANTIZE button disabled only while **recording** (enabled during playback — user can preview toggle mid-loop; SC3 updated from discussion)
- Phase 11 progress bar must NOT use juce::ProgressBar — runs its own internal timer; use custom component repainted from editor timerCallback

## Session Continuity

Last session: 2026-02-25 (10-02 complete)
Stopped at: Completed 10-02-PLAN.md — quantizeMode + quantizeSubdiv APVTS params + 7 processor methods; ready for 10-03 (LooperEngine backend) and 10-04 (UI)
Resume file: None
