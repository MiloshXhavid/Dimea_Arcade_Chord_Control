# ChordJoystick

## What This Is

ChordJoystick is a paid JUCE VST3 MIDI generator plugin for Windows that sends 4-voice chord MIDI data to an external synthesizer. Musicians control chord voicings in real time by moving an XY joystick (Y axis = Root + Third, X axis = Fifth + Tension), quantizing to a selected scale. The plugin can be driven by mouse/keyboard or optionally by a PS4/PS5/Xbox gamepad, and includes a DAW-synced looper for recording and replaying joystick gestures and trigger events with live or post-record quantization.

## Core Value

An XY joystick mapped to harmonic space — combined with per-note trigger gates, scale quantization, a gesture looper with trigger quantization, and gamepad control — that no existing MIDI tool provides as a unified instrument.

## Current State (v1.4)

- **Shipped:** 2026-02-26
- **GitHub:** https://github.com/MiloshXhavid/Dima_Plugin_Chrdmachine/releases/latest
- **Codebase:** ~5,500+ C++ LOC, 19 source files + Catch2 test suite
- **Build:** JUCE 8.0.4 + CMake FetchContent + SDL2 2.30.9 static, VS 18 2026, Release
- **Tests:** Catch2 — TC 12 quantize 233/233 assertions pass; 5 pre-existing `hasContent()` failures (not regressions)
- **Installer:** `installer/Output/DimaChordJoystickMK2-Setup.exe` (Inno Setup 6, AppVersion=1.4)
- **Known limitation:** COPY_PLUGIN_AFTER_BUILD requires elevation — use `fix-install.ps1` manually after rebuild

## Requirements

### Validated — v1.0

- ✓ Plugin compiles to VST3 and loads in a DAW without crashes — v1.0
- ✓ Plugin generates MIDI note data (pure MIDI generator, no audio output) — v1.0
- ✓ XY joystick: Y axis = Root + Third, X axis = Fifth + Tension — v1.0
- ✓ 4 interval knobs (Third/Fifth/Tension relative to root, 0–12 semitones) — v1.0
- ✓ Global transpose knob (0–11 semitones, note-name display) — v1.0
- ✓ Per-voice octave offset knobs (−3 to +3 octaves) — v1.0
- ✓ Joystick X and Y attenuator knobs (0–127) — v1.0
- ✓ 12 piano-style scale buttons + auto-copy from preset — v1.0
- ✓ Scale preset selector (20+ presets) — v1.0
- ✓ Quantization: nearest note in scale, ties down, 2-octave search — v1.0
- ✓ 4 touchplates: press = note-on, release = note-off — v1.0
- ✓ Per-voice trigger source: TouchPlate, Joystick, or Random — v1.0
- ✓ Random gate: synced subdivisions with density knob — v1.0
- ✓ Looper: record/play/stop/reset/delete, DAW-synced, time signatures, 1–16 bars — v1.0
- ✓ Looper records joystick position + gate events timestamped to beat position — v1.0
- ✓ PS4/PS5/Xbox controller support via SDL2 — v1.0
- ✓ Filter CC attenuators (CC74 cutoff, CC71 resonance) — v1.0
- ✓ Per-voice MIDI channel routing (1–16) — v1.0
- ✓ Filter CC MIDI channel selector — v1.0
- ✓ State persistence (APVTS) — v1.0

### Validated — v1.3

- ✓ MIDI Panic: CC64=0 + CC120 + CC123 on all 16 channels (48 events, no CC121) — v1.3
- ✓ PANIC! button with flash feedback, one-shot (no persistent mute) — v1.3
- ✓ Trigger quantization: Off / Live (during record) / Post (re-snap existing loop) — v1.3
- ✓ Quantize subdivision selector: 1/4, 1/8, 1/16, 1/32 — v1.3
- ✓ Gamepad status label shows controller type: PS4 / PS5 / Xbox / Controller Connected / No controller — v1.3
- ✓ Looper playback position bar (10px strip, 30 Hz update) — v1.3
- ✓ LOOPER section panel with rounded border and floating knockout label — v1.3
- ✓ Installer v1.3 with feature list — v1.3

### Validated — v1.4

- ✓ Dual LFO engine (X + Y axis) — 7 waveforms, Freq/Phase/Distortion/Level, DAW-sync — v1.4
- ✓ LFO UI section left of joystick — shape dropdown, per-axis on/off, Sync toggle — v1.4
- ✓ Beat clock dot on Free BPM knob face — flashes on beat, follows DAW sync — v1.4
- ✓ Gamepad Option mode: D-pad Up/Down scrolls Program Changes, OPTION indicator — v1.4
- ✓ GitHub v1.4 release + desktop backup — v1.4

### Active — v1.5

- [ ] Single Channel routing mode — all 4 voices → one MIDI channel, no note collisions
- [ ] Per-voice Sub Octave — fires parallel note 1 oct lower; Hold/SubOct split button on pad UI
- [ ] Left Joystick X/Y target expansion — add LFO freq/shape/level/arp gate len as modulation targets
- [ ] LFO recording — arm, record 1 cycle, playback; Distort stays live; Clear button
- [ ] Option Mode 1 Arp control — Circle=Arp on/off, Triangle=Rate, Square=Order, X=RND Sync
- [ ] R3 + held pad (any mode) = toggle Sub Oct for that voice
- [ ] Remove panic from right joystick (R3 standalone)
- [ ] Bug fix: Looper wrong start position after record cycle
- [ ] Bug fix: Crash on PS4 BT reconnect

### Out of Scope

| Feature | Reason |
|---------|--------|
| Audio synthesis | Plugin sends MIDI only |
| Built-in preset browser | DAW preset management sufficient |
| MIDI input processing | Plugin generates, does not process |
| Standalone app mode | VST3 host required |
| macOS / Linux support | Windows first, cross-platform deferred to v2 |
| MPE / polyphonic expression | Standard MIDI channels only |
| Code signing | SmartScreen acceptable for v1; deferred to v2 |
| Looper joystick quantization | Only gate event quantization in scope |

## Constraints

- **Tech Stack**: JUCE 8 + CMake — locked, all MIDI plugin infrastructure follows JUCE patterns
- **Gamepad**: SDL2 static lib — must initialize with `SDL_INIT_GAMECONTROLLER` only
- **MIDI**: 4 voices across configurable channels + filter CCs (CC74 cutoff, CC71 resonance)
- **Platform**: Windows 11 first; macOS/Linux deferred
- **No audio buses**: Pure MIDI effect — `isBusesLayoutSupported` rejects all audio buses

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| JUCE 8 + CMake FetchContent | Modern JUCE build, no Projucer dependency | ✓ Good |
| SDL2 for gamepad (static) | Cross-platform controller support, normalizes PS4+Xbox | ✓ Good |
| VST3 only (no AU/Standalone) | Windows target, reduce complexity for v1 | ✓ Good |
| CC74 cutoff + CC71 resonance | MIDI standard filter CCs | ✓ Good |
| Sample-and-hold pitch | Pitch only updates at trigger, not on joystick move | ✓ Good |
| Quantize ties → down (≤ midpoint) | Deterministic at scale note equidistance | ✓ Good |
| 4 voices on channels 1–4 default | Simple routing, user-configurable | ✓ Good |
| isMidiEffect()=true + disabled stereo buses | Ableton placement + pluginval both satisfied | ✓ Good |
| Lock-free LooperEngine (AbstractFifo) | Audio-thread safety, no std::mutex in processBlock | ✓ Good |
| SdlContext process-level singleton | One SDL_Init/SDL_Quit per process | ✓ Good |
| Static CRT (MultiThreaded) | No MSVC redistributable on clean machine | ✓ Good |
| Code signing deferred to v2 | SmartScreen acceptable for v1 | ✓ Acceptable |
| NEVER send CC121 in panic | Downstream VST3 instruments map CC121 to parameters | ✓ Good |
| Panic is one-shot not a mute toggle | 48 CCs, returns to pressable immediately | ✓ Good |
| snapToGrid ties → earlier grid point | CONTEXT.md decision; deterministic behavior | ✓ Good |
| pendingQuantize_ flag for post-record | Audio-thread safe, no mutex needed | ✓ Good |
| scratchDedup_ as class member | Avoids ~49KB stack allocation in applyQuantizeToStore() | ✓ Good |
| Single 30 Hz timer (no second timer) | Position bar + animations driven from existing timerCallback | ✓ Good |
| onConnectionChangeUI passes juce::String | Richer UI (controller type) without extra API call | ✓ Good |
| Border-only section panels | Fills covered drawAbove labels at same Y coordinate | ✓ Good |
| Editor height 790 → 810 | Quantize row (20px) was clipped at 790 | ✓ Good |

## Current Milestone: v1.5 Routing + Expression

**Goal:** Add single-channel MIDI routing, per-voice sub octave, LFO recording, left-stick modulation targets, gamepad Option Mode 1 arp control, and fix known looper/BT crash bugs.

**Target features:**
- Single Channel mode — all 4 voices → configurable MIDI channel, note-collision safe
- Sub Octave per voice — parallel note 1 oct lower, Split Hold/SubOct pad button, R3+pad shortcut
- Left Joystick X/Y modulation targets expanded: LFO freq / shape / level / arp gate len
- LFO recording — arm button, records 1 loop cycle, locked playback mode, Distort stays adjustable, Clear button
- Option Mode 1 Arp: Circle = Arp on/off, Triangle = Rate, Square = Order, X = RND Sync; remove R3 panic
- Bug fixes: looper start position after rec cycle; PS4 BT reconnect crash

---
*Last updated: 2026-02-28 — v1.5 milestone started*
