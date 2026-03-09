# Phase 18: Single-Channel Routing — Context

**Goal:** Users can route all four voices to a single configurable MIDI channel so that any monophonic or paraphonic synthesizer can be driven by the plugin without per-voice MIDI channel setup in the DAW.

---

## Decisions

### 1. UI Layout

**Mode control:** A ComboBox dropdown labeled `Routing:` with two options — `Multi-Channel` and `Single Channel`. Consistent with existing scalePreset / looperSubdiv dropdowns.

**Placement:** A separate labeled panel positioned directly **under the quantize trigger section** (not in the bottom-right column).

**Multi-Channel mode layout:**
```
[ Routing: Multi-Channel  ▼ ]

Root  ch: [1 ▼]   Fifth   ch: [3 ▼]
Third ch: [2 ▼]   Tension ch: [4 ▼]
```
- All four per-voice channel dropdowns (voiceCh0..3) are **added in this phase** — they do not currently exist in the editor.
- Each is a ComboBox with values 1–16.

**Single-Channel mode layout:**
```
[ Routing: Single-Channel  ▼ ]
[ Channel: 5               ▼ ]
```
- The four per-voice dropdowns are **hidden** (not grayed, not visible at all).
- A single `Channel:` ComboBox (1–16) appears immediately below the Routing dropdown.

---

### 2. Note Deduplication in Single-Channel Mode

*Not discussed — handled by the spec and standard pattern.*

- A `noteCount[16][128]` reference counter (audio-thread only) tracks how many active note-ons have been sent for each pitch on each channel.
- **note-on:** increment `noteCount[ch][pitch]`; emit noteOn only if count was 0 (first trigger for that pitch).
- **note-off:** decrement `noteCount[ch][pitch]`; emit noteOff only if count reaches 0 (last release).
- This guarantees: two voices playing the same pitch → one noteOn, one noteOff. No doubled velocity, no stuck note.
- `sentChannel_[4]` snapshot: at every note-on, store `effectiveChannel(v)` per voice so note-off always sends to the exact channel used at note-on time.

---

### 3. Filter CC Channel in Single-Channel Mode

*Not discussed — derived from success criterion 2.*

- Success criterion 2 states: "all four voices, **CC74, and CC71** messages arrive on the same MIDI channel."
- In Single-Channel mode, `effectiveChannel()` returns `singleChanTarget` for **all** emissions including filter CCs.
- The `filterMidiCh` APVTS parameter is **overridden** at emission time by `singleChanTarget` when `singleChanMode` is true.
- The filterMidiCh UI selector stays visible and editable (its value is preserved for when Multi-Channel mode is restored), but its value is ignored for CC emission while in Single-Channel mode.

---

### 4. Mode Switch with Active Notes

**Mode switch (Multi → Single, or Single → Multi):**
- Before switching, send `allNotesOff` on **all 16 channels** (nuclear option — foolproof regardless of edge cases).
- After flush, new notes route through the new mode immediately.

**Looper during mode switch:**
- No special handling. The looper stores voice identity (0–3), not channel info. Channel is resolved at emission time in `processBlock` via `effectiveChannel()`. The all-notes-off flush from above covers any active looper notes.
- This is consistent with success criterion 4: "Looper playback in Single Channel mode sends notes on the currently selected channel even if the channel was different at record time."

**Single-channel target change (e.g. ch5 → ch7) while already in Single mode:**
- Flush: send `allNotesOff(oldTarget)` before routing new notes to new target.
- Prevents stuck notes on the abandoned channel.

**Flush trigger points** (detected in processBlock by comparing previous vs current APVTS values):
1. `singleChanMode` toggled (either direction) → allNotesOff all 16 channels
2. `singleChanTarget` changed while `singleChanMode` is true → allNotesOff on old target channel

---

## Code Context

| Asset | Location | Relevance |
|---|---|---|
| `voiceChs[4]` array | `PluginProcessor.cpp:671-677` | Currently the only channel resolution — replace with `effectiveChannel(v)` |
| `filterMidiCh` read | `PluginProcessor.cpp:1146` | Override with `singleChanTarget` when in single-chan mode |
| Looper gate emission | `PluginProcessor.cpp:772-786` | Uses `voiceChs[v]` — update to `effectiveChannel(v)` |
| TriggerSystem callback note-on/off | `PluginProcessor.cpp:879, 887` | Update to `effectiveChannel(v)` + `sentChannel_[v]` snapshot |
| Arpeggiator note-off | `PluginProcessor.cpp:862` | Update to `effectiveChannel(voice)` |
| Bypass mode note-off | `PluginProcessor.cpp:337-346` | Update to `effectiveChannel(v)` |
| DAW stop allNotesOff | `PluginProcessor.cpp:690` | Update to `effectiveChannel(v)` |
| Voice channel APVTS params | `PluginProcessor.cpp:219-222` | Already defined (voiceCh0..3, 1–16); no UI components exist yet |
| No per-voice channel UI | `PluginEditor.h / PluginEditor.cpp` | Phase 18 adds 4 ComboBoxes + APVTS attachments |
| `noteCount` refcounter | Does not exist yet | Phase 18 adds `int noteCount_[16][128]` to PluginProcessor (audio-thread only) |
| `sentChannel_[4]` snapshot | Does not exist yet | Phase 18 adds to PluginProcessor for note-off correctness |

## New APVTS Parameters

| ID | Type | Range | Default |
|---|---|---|---|
| `singleChanMode` | bool | false / true | false |
| `singleChanTarget` | int | 1–16 | 1 |

## Locked Scope

The following were raised as potential extensions and are explicitly **deferred to later phases**:
- Per-voice channel UI was not originally in scope but was added here as part of Phase 18 since the parameters exist without any UI at all.
- Any "channel memory" in looper events is explicitly rejected — channel is always resolved at emission time.
