# ChordJoystick MK2

A MIDI chord generator VST3 plugin for Windows. Plug in a gamepad or use the on-screen joystick — move it around and play chords in any scale, key, and voicing.

---

## Download

**[Latest release — v1.3](https://github.com/MiloshXhavid/Dima_Plugin_Chrdmachine/releases/latest)**

Download `DimaChordJoystickMK2-Setup.exe`, run it, rescan plugins in your DAW.

**Requirements**
- Windows 10 or 11, 64-bit
- VST3-compatible DAW (Ableton Live, REAPER, FL Studio, Bitwig, etc.)
- Xbox, PS4, or PS5 controller — optional but recommended

---

## What it does

You move a joystick (or analog stick on a gamepad) and it plays chords. The Y axis controls the root note and third, the X axis controls the fifth and tension note. Every position snaps to the scale you choose, so everything stays in key automatically.

The four chord voices go out on separate MIDI channels so you can route them to different instruments or apply different effects per voice.

---

## Features

### Chord engine
- Y axis → root + third, X axis → fifth + tension
- 20 built-in scales (major, minor, modes, pentatonic, blues, etc.) plus a custom scale editor
- Global key/transpose, per-voice octave offsets, per-voice interval adjustments
- 4 voices on MIDI channels 1–4 (configurable per voice)
- Slew (portamento) per voice

### Triggers
- **Touchplate** — 4 pads, one per voice, velocity sensitive
- **Joystick** — trigger all voices when the stick leaves center
- **Random** — generative rhythmic triggers with density, subdivision, and gate controls per voice
- Hold mode per voice

### Looper
- Record and loop chord progressions in sync with your DAW
- Record Gates / Joystick / Touchplate modes
- Adjustable time signature (3/4, 4/4, 5/4, 7/8, 9/8, 11/8) and length (1–16 bars)
- **Trigger quantization** — snap recorded MIDI to grid: Off / Live (while recording) / Post (after recording), subdivisions from 1/4 down to 1/32
- Looper position bar shows playback progress in real time

### Gamepad input
- Right stick → joystick pitch
- Left stick X/Y → CC74 cutoff + CC71 resonance (configurable channel and attenuation)
- L1/L2/R1/R2 → trigger individual voices
- L3 → all voices
- X → looper play/stop, Square → reset, Triangle → record, Circle → delete
- Status label shows detected controller type: PS4 / PS5 / Xbox / Controller Connected / No controller
- **MIDI Panic** button — sends all-notes-off on all 16 MIDI channels instantly

### Visual
- LOOPER section panel with visual grouping
- Scale keyboard display
- Arpeggiator (rate, order, gate%)

---

## Installation

1. Download `DimaChordJoystickMK2-Setup.exe` from the [releases page](https://github.com/MiloshXhavid/Dima_Plugin_Chrdmachine/releases/latest)
2. Run it — admin access required (installs to `C:\Program Files\Common Files\VST3\`)
3. Open your DAW and rescan VST3 plugins
4. Add **ChordJoystick MK2** to a MIDI track, place it before your instrument

> In Ableton Live: add the plugin on a MIDI track, then route that track's output to an instrument track.

---

## Building from source

Requires: CMake 3.22+, Visual Studio 2022, Windows SDK

```bash
git clone https://github.com/MiloshXhavid/Dima_Plugin_Chrdmachine.git
cd Dima_Plugin_Chrdmachine
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The VST3 is output to `build/ChordJoystick_artefacts/Release/VST3/`.

---

## Changelog

### v1.3
- LOOPER section panel with rounded border and floating label
- Gamepad status label shows specific controller type (PS4 / PS5 / Xbox)
- Trigger quantization (Off / Live / Post, 1/4–1/32 subdivisions)
- MIDI Panic button (full 16-channel all-notes-off sweep)

### v1.2
- DAW sync looper
- Trigger source per voice (pad / joystick / random)
- Random generative triggers with BPM sync
- Slew (portamento) per voice

### v1.1
- Gamepad support (Xbox, PS4, PS5)
- Filter mod via left stick → CC74/CC71
- Arpeggiator

### v1.0
- Initial release

---

Made by Dima Xhavid
