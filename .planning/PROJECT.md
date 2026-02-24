# ChordJoystick

## What This Is

ChordJoystick is a paid JUCE VST3 MIDI generator plugin for Windows that sends 4-voice chord MIDI data to an external synthesizer. Musicians control chord voicings in real time by moving an XY joystick (Y axis = Root + Third, X axis = Fifth + Tension), quantizing to a selected scale. The plugin can be driven by mouse/keyboard or optionally by a PS4/Xbox gamepad, and includes a DAW-synced looper for recording and replaying joystick gestures and trigger events.

## Core Value

An XY joystick mapped to harmonic space — combined with per-note trigger gates, scale quantization, a gesture looper, and gamepad control — that no existing MIDI tool provides as a unified instrument.

## Requirements

### Validated — v1.0

- ✓ Plugin compiles to VST3 and loads in a DAW (Ableton, Reaper) without crashes — v1.0
- ✓ Plugin generates MIDI note data (no audio output, pure MIDI generator) — v1.0
- ✓ XY joystick (mouse-driven): Y axis controls Root + Third pitch, X axis controls Fifth + Tension pitch — v1.0
- ✓ 4 individual interval knobs (Root always 0, Third/Fifth/Tension relative to root, 0–12 semitones) — v1.0
- ✓ Global transpose knob (0–11 semitones / key selector with note-name display) — v1.0
- ✓ Per-voice octave offset knobs (−3 to +3 octaves precision adder) — v1.0
- ✓ Joystick X and Y attenuator knobs (0–127 range, scales joystick output to MIDI range) — v1.0
- ✓ 12 piano-style scale buttons for custom scale entry (always-editable, auto-copies preset) — v1.0
- ✓ Scale preset selector (20+ presets: Major, Minor, Harmonic Minor, Pentatonic, Blues, modes, etc.) — v1.0
- ✓ Quantization: nearest note in scale, ties go DOWN, search over 2 octaves — v1.0
- ✓ 4 touchplates (Root / Third / Fifth / Tension): press = sample-and-hold pitch + MIDI note-on, release = note-off — v1.0
- ✓ Per-voice trigger source: TouchPlate, Joystick movement, or Random — v1.0
- ✓ Random gate: synced subdivisions (1/4, 1/8, 1/16, 1/32) with density knob — v1.0
- ✓ Looper: record/play/stop/reset/delete, DAW-synced, time signatures (3/4 4/4 5/4 7/8 9/8 11/8), 1–16 bars length — v1.0
- ✓ Looper records both joystick position AND gate events timestamped to beat position — v1.0
- ✓ PS4 / Xbox controller support via SDL2 (optional — plugin fully usable with mouse only) — v1.0
- ✓ Filter CC attenuators: separate X/Y knobs scaling gamepad left-stick output (0–127) — v1.0
- ✓ Per-voice MIDI channel routing (voices 1–4 to any MIDI channel 1–16) — v1.0
- ✓ Filter CC MIDI channel selector — v1.0
- ✓ State persistence (all APVTS parameters saved/recalled per DAW session/preset) — v1.0

### Active — v1.1

## Current Milestone: v1.1 Polish & Quantization

**Goal:** Formalize post-v1.0 patches, improve MIDI panic to a full reset sweep, add live + post-record trigger quantization, and polish the UI with better visual grouping and a playback position indicator.

**Target features:**
- Formalize 6 post-v1.0 patches with REQ-IDs (sustain CC, hold mode, filter mod, MIDI mute, filter looper, threshold fix)
- Improved MIDI panic — full CC reset sweep on all 16 channels
- Animated mute state visual feedback
- Live trigger quantization (gate events snapped to grid during recording)
- Post-record QUANTIZE button (re-snap existing loop)
- Quantize subdivision selector (independent from random gate)
- Section visual grouping in plugin UI
- Gamepad status shows controller type
- Looper playback position bar
- Installer rebuilt for v1.1

### Out of Scope

- Audio synthesis — plugin sends MIDI only, no sound engine
- Built-in preset browser — v1 relies on DAW preset management
- MIDI input processing — plugin generates MIDI, does not process incoming MIDI
- Standalone app mode — VST3 host required
- macOS / Linux support — Windows first, cross-platform deferred to v2
- MPE / polyphonic expression — standard MIDI channels only
- Physical hardware touchplates/joystick — UI controls only (gamepad is the hardware bridge)
- Code signing — SmartScreen "More info / Run anyway" acceptable for v1; deferred to v2

## Current State (v1.0)

- **Shipped:** 2026-02-23
- **Codebase:** ~3,966 C++ LOC, 14 source files in Source/
- **Build:** JUCE 8.0.4 + CMake FetchContent + SDL2 2.30.9 static, VS 18 2026, Release
- **Tests:** 26 Catch2 tests passing (ScaleQuantizer 218 assertions, ChordEngine 9 cases, LooperEngine 11 tests)
- **Validation:** pluginval strictness level 5 — 19/19 suites passed
- **Installer:** `installer/Output/DimaChordJoystick-Setup.exe` (3.5 MB, Inno Setup 6.7.1)
- **Distribution:** Gumroad (upload pending)
- **Known limitation:** COPY_PLUGIN_AFTER_BUILD requires elevation — use fix-install.ps1 manually after rebuild

## Constraints

- **Tech Stack**: JUCE 8 + CMake — locked, all MIDI plugin infrastructure follows JUCE patterns
- **Gamepad**: SDL2 static lib — must initialize with `SDL_INIT_GAMECONTROLLER` only (no video subsystem)
- **MIDI**: 4 voices across configurable channels + filter CCs (CC74 cutoff, CC71 resonance)
- **Platform**: Windows 11 first; macOS/Linux deferred
- **No audio buses**: Plugin is pure MIDI effect — `isBusesLayoutSupported` must reject all audio buses (accepts only 0,0 and 2,2 with disabled buses for pluginval)

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| JUCE 8 + CMake FetchContent | Modern JUCE build, no Projucer dependency | ✓ Good — reproducible, pinned to 8.0.4 |
| SDL2 for gamepad (static) | Cross-platform controller support, normalizes PS4+Xbox | ✓ Good — static CRT + SDL2 static = no runtime deps |
| VST3 only (no AU/Standalone) | Windows target, reduce complexity for v1 | ✓ Good — shipped cleanly |
| CC74 cutoff + CC71 resonance | MIDI standard filter CCs, maximum synth compatibility | ✓ Good |
| Sample-and-hold pitch | Pitch only updates when pad is triggered, not on joystick move | ✓ Good — core interaction model |
| Quantize ties → down | Deterministic behavior at scale note equidistance | ✓ Good |
| 4 voices on channels 1–4 default | Simple routing default, user-configurable per voice | ✓ Good |
| isMidiEffect()=true + disabled stereo buses | Ableton MIDI track placement + pluginval bus tests both satisfied | ✓ Good — key discovery |
| Lock-free LooperEngine (AbstractFifo) | Audio-thread safety, no std::mutex in processBlock | ✓ Good — 11 passing stress tests |
| SdlContext process-level singleton | One SDL_Init/SDL_Quit per process, no multi-instance crash | ✓ Good |
| JOY retrigger on scale-degree boundary | noteOff(old)+noteOn(new) — universal synth compat, no RPN/pitchBend needed | ✓ Good |
| PPQ subdivision index (int64_t) | Integer comparison avoids float equality issues; fires once per boundary | ✓ Good |
| Static CRT (MultiThreaded) | No MSVC redistributable on clean machine | ✓ Good — clean-machine validated |
| Code signing deferred to v2 | SmartScreen acceptable for v1; unsigned EXE validated on clean machine | ✓ Acceptable |

---
*Last updated: 2026-02-24 — v1.1 milestone started*
