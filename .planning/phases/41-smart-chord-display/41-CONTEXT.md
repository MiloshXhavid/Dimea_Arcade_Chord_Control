# Phase 41: Smart Chord Display — CONTEXT

## Goal
The chord display always shows the correct mode quality (major/minor) and 7th quality
built from the root up — inferring absent voices from the active scale — and only
updates on an active trigger. Unicode jazz symbols replace verbose text.

## Decisions

### A. "Not triggered" definition

- A voice is **not triggered** when it did not produce a note-on in the same trigger
  event as the snapshot (option b — event-based, not real-time gate state).
- Implementation: add `uint8_t displayVoiceMask_` to `PluginProcessor` (4 bits, one per
  voice). Written alongside `displayPitches_` at every snapshot site. Default `0xF`
  (all voices active). Same best-effort non-atomic pattern as `displayPitches_`.
- **Arp special case:** arp fires individual voice steps, but the full chord is known
  via `heldPitch_`. Use mask `0xF` for arp steps so all 4 pitches are treated as
  "real" — inference does not apply during arp playback.
- `getCurrentVoiceMask()` exposes it alongside `getCurrentPitches()`.

### B. Scale inference for absent voices

**Third (Voice 1 absent — bit 1 not set in mask):**
- Check `(rootPc + 3) % 12` in active scale → minor quality
- Check `(rootPc + 4) % 12` in active scale → major quality
- Both present (e.g. chromatic) → use Voice 1's real pitch to decide
- Neither present (e.g. pentatonic without 3rd) → omit third from name entirely

**Seventh (Voice 3 absent — bit 3 not set in mask):**
- Check `(rootPc + 11) % 12` in active scale → maj7 quality (`△`)
- Check `(rootPc + 10) % 12` in active scale → dominant/minor 7th quality
- Both present → use Voice 3's real pitch to decide
- Neither present → omit 7th from name entirely

**Scale data source:** UI thread reads APVTS params (`scalePreset`, `useCustomScale`,
`scaleNote0..11`) inside `timerCallback()` when computing the chord name. One-frame
latency is acceptable — scale rarely changes mid-performance.

Scale notes extracted via `ScaleQuantizer::getScalePattern()` + `getScaleSize()` for
presets, or the custom `bool[12]` array directly for custom mode.

### C. Update trigger logic

Current behavior is correct — **no changes**. Chord name updates on:
- Deliberate triggers (touchplate, gamepad button, random gate)
- Chord parameter changes (scale, intervals, transpose) via `lastChordParamHash_`
- Each arp step

### D. Arpeggiator

Chord display **updates on every arp step**. Voice mask = `0xF` for all arp steps
(full chord known, no inference needed). Display updates rhythmically with the arp.

### E. Unicode symbols (applies to all chord names, not just inferred ones)

Replace verbose strings throughout `ChordNameHelper.h`:

| Old | New | Meaning |
|-----|-----|---------|
| `"maj7"` | `"△"` (U+25B3) | major 7th |
| `"mmaj7"` | `"m△"` | minor major 7th — avoids "majmaj" |
| `"m7b5"` | `"ø"` (U+00F8) | half-diminished |
| `"dim7"` | `"°7"` (U+00B0) | diminished 7th |
| `"maj9"` | `"△9"` | major 9th |
| `"mmaj9"` | `"m△9"` | minor major 9th |
| `"maj7#11"` | `"△#11"` | Lydian major 7 |
| `"maj7#5"` | `"△#5"` | augmented major 7th |
| `"maj13"` | `"△13"` | major 13th |

All other symbols remain as-is (e.g. "m7", "7", "6", "dim", "aug", "sus2").

### F. Extended chord name function

New signature in `ChordNameHelper.h`:

```cpp
// Existing (kept for backward compat):
inline std::string computeChordNameStr(const int pitches[4]);

// New smart version with inference:
struct ChordNameContext {
    const int*  scalePattern;   // from ScaleQuantizer::getScalePattern()
    int         scaleSize;
    uint8_t     voiceMask;      // bit v = voice v produced a real note-on
};
inline std::string computeChordNameSmart(const int pitches[4],
                                          const ChordNameContext& ctx);
```

`computeChordNameSmart` substitutes inferred pitch classes for absent voices before
running the existing template-matching logic — no duplication of the matching table.

## Code context

| Symbol | File | Notes |
|--------|------|-------|
| `computeChordNameStr()` | `ChordNameHelper.h:14` | Existing function — extend with smart overload |
| `displayPitches_` | `PluginProcessor.h:346` | Pattern for new `displayVoiceMask_` |
| `getCurrentPitches()` | `PluginProcessor.h:169` | Add `getCurrentVoiceMask()` alongside |
| `lastChordParamHash_` | `PluginProcessor.cpp:1556` | Hash-triggered refresh — unchanged |
| `ScaleQuantizer::getScalePattern()` | `ScaleQuantizer.h:34` | Preset → `const int*` semitone array |
| `ScaleQuantizer::getScaleSize()` | `ScaleQuantizer.h:35` | Preset → note count |
| `buildCustomPattern()` | `ScaleQuantizer.h:39` | Custom `bool[12]` → pattern array |
| `chordNameLabel_` | `PluginEditor.cpp:2815` | 48px bold label, updated in timerCallback |
| `computeChordName()` | `PluginEditor.cpp:45` | Thin JUCE wrapper — update to call smart version |
| `displayPitches_ = heldPitch_` | `PluginProcessor.cpp:1591` | All snapshot sites need mask write too |

## Scope guardrail
- `ChordNameHelper.h` template table is NOT expanded — new chord types are out of scope
- No changes to `ChordEngine`, `ScaleQuantizer`, or `TriggerSystem`
- No UI layout changes — label position/size unchanged
- Voice 0 (root) always treated as "real" — no inference for the root note
- Voice 2 (fifth) always treated as "real" — inference only for voice 1 (third) and voice 3 (tension/7th)
