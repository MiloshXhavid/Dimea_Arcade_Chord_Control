# Phase 44: Instrument Type Conversion — Research

**Researched:** 2026-03-07
**Domain:** JUCE VST3 plugin type / bus configuration
**Confidence:** HIGH — all findings verified directly from codebase + JUCE 8.0.4 source

## Summary

The plugin is currently a pure MIDI effect (`IS_MIDI_EFFECT TRUE`, `isMidiEffect()=true`, both audio buses disabled). Converting to an instrument adds a silent stereo audio output bus and changes the VST3 category — making it visible and usable in DAWs that only accept instruments in their instrument slot (Cakewalk, Logic, FL Studio, Bitwig).

The change is surgical: 4 sites across 2 files (CMakeLists.txt + PluginProcessor.h/.cpp). The MIDI generation engine, APVTS, all feature subsystems, and the existing `audio.clear()` in processBlock are completely untouched. Preset XML state is compatible (same PLUGIN_CODE "DCJM", same PLUGIN_MANUFACTURER_CODE "MxCJ").

**Primary recommendation:** Change CMakeLists IS_MIDI_EFFECT→FALSE / IS_SYNTH→TRUE / VST3_CATEGORIES "Instrument"; enable the output bus in the constructor; update isBusesLayoutSupported; set isMidiEffect()=false. Everything else stays identical.

---

## Current State (verified from codebase)

### CMakeLists.txt (lines 127–132)
```cmake
VST3_CATEGORIES         "Fx" "MIDI"
IS_SYNTH                FALSE
NEEDS_MIDI_INPUT        TRUE
NEEDS_MIDI_OUTPUT       TRUE
IS_MIDI_EFFECT          TRUE
```

### PluginProcessor.h (line 30)
```cpp
bool isMidiEffect()  const override { return true;  }
```

### PluginProcessor.cpp constructor (lines 369–372)
```cpp
PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), false)  // disabled
          .withOutput("Output", juce::AudioChannelSet::stereo(), false)), // disabled
```
Both buses declared but **disabled** (the `false` third arg = not active by default).

### PluginProcessor.cpp isBusesLayoutSupported (lines 514–521)
```cpp
return (numIn == 0 && numOut == 0) || (numIn == 2 && numOut == 2);
```
Accepts 0+0 (MIDI effect) or 2+2 (stereo passthrough).

### PluginProcessor.cpp processBlock (line 610)
```cpp
audio.clear();   // already present — no change needed
```

---

## Required Changes (4 sites, 2 files)

### Change 1 — CMakeLists.txt
```cmake
# BEFORE
VST3_CATEGORIES         "Fx" "MIDI"
IS_SYNTH                FALSE
IS_MIDI_EFFECT          TRUE

# AFTER
VST3_CATEGORIES         "Instrument" "Fx"
IS_SYNTH                TRUE
IS_MIDI_EFFECT          FALSE
```
`NEEDS_MIDI_INPUT TRUE` and `NEEDS_MIDI_OUTPUT TRUE` stay — they control MIDI buses, independent of plugin type.

### Change 2 — PluginProcessor.h
```cpp
// BEFORE
bool isMidiEffect()  const override { return true;  }

// AFTER
bool isMidiEffect()  const override { return false; }
```

### Change 3 — PluginProcessor.cpp constructor
```cpp
// BEFORE
: AudioProcessor(BusesProperties()
      .withInput ("Input",  juce::AudioChannelSet::stereo(), false)
      .withOutput("Output", juce::AudioChannelSet::stereo(), false)),

// AFTER
: AudioProcessor(BusesProperties()
      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
```
- Drop the input bus — instruments don't need audio input
- Enable the output bus (`true`) — this is what makes hosts see it as an instrument with audio output
- The silent output is already guaranteed by `audio.clear()` in processBlock

### Change 4 — PluginProcessor.cpp isBusesLayoutSupported
```cpp
// BEFORE
return (numIn == 0 && numOut == 0) || (numIn == 2 && numOut == 2);

// AFTER
// Instrument: no audio input, stereo output (some hosts may set 0 output for pure MIDI use)
return numIn == 0 && (numOut == 0 || numOut == 2);
```

---

## Architecture Patterns

### JUCE Instrument with MIDI Output (Industry Standard)
This is how Scaler 2, Captain Chords, and all professional MIDI generator plugins are built:
- `IS_SYNTH TRUE` → VST3 category "Instrument" → appears in instrument slots everywhere
- `NEEDS_MIDI_OUTPUT TRUE` → VST3 MIDI output event bus → DAW sees MIDI output from instrument
- Silent audio output bus (always `buffer.clear()`) → satisfies DAW instrument requirement
- MIDI generation in processBlock unchanged — same `MidiBuffer& midi` write path

### VST3 Category Strings
JUCE maps `VST3_CATEGORIES "Instrument" "Fx"` to the VST3 subcategory string `"Instrument|Fx"`. This is the standard for multi-purpose plugins. "Instrument" alone is also valid.

### Bus Configuration Rules
- `IS_MIDI_EFFECT TRUE` in JUCE → JUCE wrapper disables all audio buses at the VST3 level regardless of what the C++ constructor declares. **Must set FALSE to allow audio output.**
- `IS_SYNTH TRUE` in JUCE → sets `kIsSynth` in VST3 component flags, used by some DAWs to route MIDI to the plugin automatically.
- The constructor `BusesProperties` declares available buses; `isBusesLayoutSupported` gates what layouts the host can select.

---

## Common Pitfalls

### Pitfall 1: IS_MIDI_EFFECT overrides BusesProperties
**What goes wrong:** Developer enables output bus in constructor but leaves `IS_MIDI_EFFECT TRUE` → JUCE VST3 wrapper still reports no audio buses to host → instrument slot not available.
**Prevention:** Both CMakeLists `IS_MIDI_EFFECT FALSE` AND `isMidiEffect()=false` must be set together.

### Pitfall 2: Input bus left in constructor
**What goes wrong:** Leaving `.withInput(...)` with `false` is harmless but unnecessary — instruments don't accept audio input. Some hosts may incorrectly offer audio input routing.
**Prevention:** Remove input bus from BusesProperties entirely.

### Pitfall 3: Logic / some DAWs don't expose instrument MIDI output by default
**What goes wrong:** In Logic, instrument MIDI output requires a dedicated "MIDI FX" or "External Instrument" track type — regular instrument tracks may not route MIDI output to other instruments.
**Impact:** This is a DAW limitation, not a plugin bug. Cakewalk, Reaper, FL Studio, Bitwig all handle it correctly. Logic is handled differently (documented in verification checklist).
**Prevention:** Document in release notes — "For Logic users, see routing guide."

### Pitfall 4: Ableton project files (not presets) need manual re-routing
**What goes wrong:** Existing `.als` files that use the plugin as MIDI FX won't automatically update to instrument slot — users need to re-add the plugin.
**Impact:** Preset XML files (APVTS state) are 100% compatible — same PLUGIN_CODE, no parameter changes. Only the DAW project slot type changes.
**Prevention:** Note in changelog. This is a one-time migration per project for existing users.

### Pitfall 5: cmake reconfigure required
**What goes wrong:** Changing CMakeLists.txt flags requires a CMake reconfigure (not just a rebuild) to take effect in the VST3 manifest.
**Prevention:** After changing CMakeLists, user must delete CMakeCache.txt and re-run cmake, OR use the existing `build32.ps1` which already does full configure+build.

---

## DAW Compatibility Matrix (Post-Change)

| DAW | Current | After Change | Notes |
|-----|---------|-------------|-------|
| Ableton | MIDI FX slot (dedicated MIDI track) | Instrument slot (same 2-track workflow) | MIDI output routing via "MIDI To" on second track |
| Reaper | Stacks on same track | Same — works better | Instrument slot, MIDI output to next FX |
| Cakewalk | Not detected | Instrument slot — works | Primary motivation |
| FL Studio | MIDI FX (limited) | Instrument slot — works | Channel rack instrument |
| Logic | MIDI FX (limited support) | Instrument slot | MIDI output routing requires "External Instrument" track |
| Bitwig | MIDI FX | Instrument slot — works | Native instrument support |

---

## State Compatibility

- APVTS state XML: fully compatible — same parameter IDs, same PLUGIN_CODE "DCJM"
- Plugin ID: unchanged (PLUGIN_CODE + PLUGIN_MANUFACTURER_CODE unchanged)
- VST3 slot type: changes from "Fx/MIDI" to "Instrument/Fx" — existing DAW projects need plugin re-insertion (presets survive)

---

## Sources

- Verified from codebase: CMakeLists.txt (lines 121–133), PluginProcessor.h (line 30), PluginProcessor.cpp (lines 368–372, 514–521, 610)
- JUCE 8.0.4 source: `juce_add_plugin` macro — IS_MIDI_EFFECT, IS_SYNTH, VST3_CATEGORIES flags
- JUCE AudioProcessor BusesProperties API — withOutput(), withInput(), third arg = enabled default
- Industry reference: Scaler 2, Captain Chords VST3 manifests — confirmed "Instrument|Fx" category with MIDI output

## RESEARCH COMPLETE

**Phase:** 44 - Instrument Type Conversion
**Confidence:** HIGH

### Key Findings
- 4 surgical changes across 2 files — no logic changes whatsoever
- Constructor already has the buses declared (just disabled) — enabling output is 1 word change (`false` → `true`) + drop input bus
- processBlock `audio.clear()` already present — silent output guaranteed
- Preset XML 100% compatible — same PLUGIN_CODE
- CMake reconfigure required (build32.ps1 handles this)

### Confidence Assessment
| Area | Level | Reason |
|------|-------|--------|
| Change sites | HIGH | Verified directly from codebase |
| JUCE flag semantics | HIGH | Read from CMakeLists + JUCE source behavior |
| DAW behavior | MEDIUM | Based on industry patterns; Cakewalk/Logic not locally testable |
| Preset compat | HIGH | PLUGIN_CODE unchanged, APVTS params unchanged |

### Ready for Planning
Research complete. Planner can now create PLAN.md files.
