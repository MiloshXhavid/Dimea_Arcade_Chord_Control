# ChordJoystick

## What This Is

ChordJoystick is a paid JUCE VST3 MIDI generator plugin for Windows that sends 4-voice chord MIDI data to an external synthesizer. Musicians control chord voicings in real time by moving an XY joystick (Y axis = Root + Third, X axis = Fifth + Tension), quantizing to a selected scale. The plugin can be driven by mouse/keyboard or optionally by a PS4/PS5/Xbox gamepad, and includes a DAW-synced looper for recording and replaying joystick gestures and trigger events with live or post-record quantization.

## Core Value

An XY joystick mapped to harmonic space — combined with per-note trigger gates, scale quantization, a gesture looper with trigger quantization, and gamepad control — that no existing MIDI tool provides as a unified instrument.

## Current State (v1.7)

- **Shipped:** 2026-03-05
- **GitHub:** https://github.com/MiloshXhavid/Dima_Plugin_Chrdmachine/releases/tag/v1.7 (Latest)
- **Codebase:** ~8,000+ C++ LOC, 20+ source files + Catch2 test suite
- **Build:** JUCE 8.0.4 + CMake FetchContent + SDL2 2.30.9 static, VS 18 2026, Release
- **Installer:** `installer/Output/DimeaArcade-ChordControl-v1.7.0-Setup.exe` (Inno Setup 6, static CRT, no MSVC redist required)
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

### Validated — v1.5

- ✓ Single Channel routing mode — all 4 voices → one MIDI channel, no note collisions — v1.5
- ✓ Per-voice Sub Octave — fires parallel note 1 oct lower; Hold/SubOct split button on pad UI — v1.5
- ✓ Left Joystick X/Y target expansion — LFO freq/phase/level + gate length as modulation targets — v1.5
- ✓ LFO recording — arm, record 1 cycle, playback; Distort stays live; Clear button — v1.5
- ✓ Arpeggiator — 6 modes, 4 rates, gate length, bar-boundary launch — v1.5
- ✓ Random trigger extensions — Free/Hold modes, Population + Probability knobs, 1/64 subdiv — v1.5
- ✓ Option Mode 1 — Circle=Arp, Triangle=Rate, Square=Order, X=RND Sync; R3+pad=Sub Oct — v1.5
- ✓ LFO joystick visual tracking — sliders track joystick at 30 Hz, base+offset model — v1.5
- ✓ Bug fix: Looper anchor drift after record cycle — v1.5
- ✓ Bug fix: PS4 BT reconnect crash — v1.5

### Validated — v1.6

- ✓ DEF-01/02/03: Third=4, Fifth=4, Tension=3 default octave values — v1.6
- ✓ DEF-04: Scale preset defaults to Natural Minor on fresh load — v1.6
- ✓ BUG-03: noteCount_ reference-count clamp removed from all 13 note-off paths — v1.6
- ✓ TRIP-01: Random trigger subdivision selector adds 1/1T–1/32T triplets — v1.6
- ✓ TRIP-02: Quantize trigger subdivision selector adds same 6 triplet options — v1.6
- ✓ RND-08/09/10: Random Free redesign — Poisson (SYNC OFF), internal clock (SYNC ON + stopped), DAW grid (DAW Sync ON) — v1.6
- ✓ LOOP-01/02/03/04: Looper perimeter bar — clockwise rectangular bar, ghost ring, label exclusion — v1.6
- ✓ GitHub v1.6 release + installer + desktop backup — v1.6

### Validated — v1.7

- ✓ Space-themed joystick pad — deep-space background, milky way particle band, density-driven starfield, BPM-synced breathing glow ring — v1.7
- ✓ Semitone grid on joystick pad — in-scale notes bright, out-of-scale dimmed — v1.7
- ✓ Spring-damper cursor physics — inertia while moving, critically-damped spring-back (kDamping=0.90, ~150ms, no overshoot) — v1.7
- ✓ Perimeter arc indicator on joystick pad border + note-label compass at cardinal positions — v1.7
- ✓ LFO waveform oscilloscope in SYNC row (48-sample ring buffer, proportional scale, blink on positive) — v1.7
- ✓ Gamepad SWAP — left stick ↔ right stick routing (pitch vs filter CC), fully isolated across all 4 dispatch sites — v1.7
- ✓ Gamepad INV — X↔Y axis swap (90° rotation) on both sticks — v1.7
- ✓ SWAP/INV buttons in GamepadDisplayComponent; L3/R3 labels swap dynamically; cyan/amber lit colors — v1.7
- ✓ Left stick deadzone smooth rescaling; CC74/CC71/CC12/CC76 on both left stick X and Y axes — v1.7
- ✓ Arp step-counting root cause fixed (start-of-block ppq + backward-jump guard) — v1.7
- ✓ Gate length applies to joystick trigger source — v1.7
- ✓ Looper reset syncs all random-free voice phases — v1.7
- ✓ Cursor snaps to pad center on plugin open; INV mode Quantizer/Modulation labels and attachments swap correctly — v1.7
- ✓ Piano black key two-pass hit test — black keys win over white key rects below them — v1.7
- ✓ Battery icon redesigned as 3 vertical stripe blocks (green/orange/red/gray/cyan/"?") — v1.7
- ✓ GitHub v1.7 released as Latest; Inno Setup installer v1.7; desktop backup — v1.7

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
| Space background baked in resized() | Zero per-frame allocation; paint() is compositing only | ✓ Good |
| kDamping=0.90 (near-critical) | No overshoot, ~150ms settle, not sluggish | ✓ Good |
| LFO stick offset in normalized log space | Equal perceptual range across 3 decades | ✓ Good |
| prevInvState_ guard for INV attachment swap | Fires once on toggle, not 30x/sec | ✓ Good |
| Piano two-pass hit test (black then white) | No coordinate math changes; just iteration order | ✓ Good |

---
*Last updated: 2026-03-05 — v1.7 shipped*
