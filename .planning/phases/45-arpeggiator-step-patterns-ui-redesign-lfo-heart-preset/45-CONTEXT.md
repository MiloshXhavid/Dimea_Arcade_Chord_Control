# Phase 45 — Arpeggiator Step Patterns + UI Redesign

## Phase Goal

Add an 8-step pattern grid to the arpeggiator (ON/TIE/OFF per step, variable length 1–8), redesign the arp panel to a 2-row control layout to accommodate the grid within the same box size, and polish the RND SYNC button into a 3-state cycling control.

---

## Scope Boundary

**In scope:**
- 8-step grid (2 rows × 4 columns) with ON / TIE / OFF states
- Arp length combo (1–8) squeezed into the controls row
- Arp panel controls collapsed from 3 rows to 2 rows (no separate label row)
- RND SYNC → 3-state cycling button: FREE / INT / DAW

**Deferred (do NOT implement in this phase):**
- LFO heart/star preset — removed from this phase entirely
- Dual-sequencer / Subharmonicon-mode: two independent 4-step rows at different rates. The grid visual is 2×4 but it's a SINGLE 8-step sequence.
- Any other arp-order behavior changes beyond what step states provide.

---

## Area 1: Step Grid — States and Behavior

### Cell states
| State | Visual | Behavior |
|-------|--------|----------|
| **ON** | Bright filled square (accent color) | Fires a new note-on for the current arp voice |
| **TIE** | Dim square with a horizontal connecting bar to next cell | Sustains the previous note through this step — no note-off, no re-trigger |
| **OFF** | Dark/empty square | Sends immediate note-off for any currently sounding arp note; step is skipped |

### Click interaction
- **Single left-click** cycles the cell: ON → TIE → OFF → ON (wrap)
- No right-click menu needed

### Edge cases (locked decisions)
- **First step = TIE**: treated as ON (fires normally — no previous note to sustain)
- **Consecutive TIEs**: the note from the last ON step sustains across all of them
- **OFF with active note**: sends immediate note-off, note does not decay naturally
- **Cells beyond length**: all 8 cells are always clickable and editable regardless of the current length setting. Steps beyond the active length are visually grayed out (40% alpha) to signal "won't play" — but their state is preserved and resumes when length is extended. This is distinct from the OFF state: inactive-beyond-length cells look uniformly dim, OFF cells within range look visually empty/dark with a border.

### APVTS parameters needed
- `arpStepState0` … `arpStepState7`: Choice param, 3 options: "ON" / "TIE" / "OFF", default "ON"
- `arpLength`: Int param, range 1–8, default 8

---

## Area 2: Arp Box Layout

### Fixed constraint
`arpBlockBounds_` **position, width, and height do not change** — `kArpH` stays at **84px**. No looper adjustments needed.

### Pixel budget (fits in 84px)
```
14px  header clearance ("ARPEGGIATOR" knockout label)
20px  ARP ON/OFF button
14px  step row A  (cells 1–4)
14px  step row B  (cells 5–8)
 0px  gap
20px  RATE | ORDER | LEN | GATE controls row
 2px  breathing room
────
84px  total
```

### Target layout (grid between button and controls)
```
┌──────────────────────────────────────────────────────┐
│  [14px header clearance — "ARPEGGIATOR" label]       │
│  [ ARP ON/OFF ·········· full width ················] │  20px
│  ┌─────────┬─────────┬─────────┬─────────┐           │
│  │ step 1  │ step 2  │ step 3  │ step 4  │           │  14px
│  ├─────────┼─────────┼─────────┼─────────┤           │
│  │ step 5  │ step 6  │ step 7  │ step 8  │           │  14px
│  └─────────┴─────────┴─────────┴─────────┘           │
│  [ RATE▼  |  ORDER▼  |  LEN▼  |  GATE──────]         │  20px
└──────────────────────────────────────────────────────┘
```

### Controls row (no separate label row)
The separate label row (`arpSubdivLabel_`, `arpOrderLabel_`, `arpGateTimeLabel_`) is **removed**. The controls row fits 4 equal-width items:
- `arpSubdivBox_` — Rate combo (existing)
- `arpOrderBox_` — Order combo (existing)
- `arpLengthBox_` — new LEN combo, items "1"…"8", default "8"
- `arpGateTimeKnob_` — Gate knob (existing)

Each item ≈ `arpBlockBounds_.getWidth() / 4`. The knob can be `reduced(kKnobPad, 0)` horizontally.

### Step cell sizing
- Cell width = `arpBlockBounds_.getWidth() / 4` (minus 1px inter-cell gap)
- Cell height = 14px
- 2 rows × 4 cols = 8 cells total
- A subtle 1px vertical separator line at the center between col 4 and col 5 visually groups the two rows of 4

### Inactive cell visual — LOCKED
Four paint conditions:
1. **Active + ON**: bright filled accent color, full alpha
2. **Active + TIE**: dim fill + horizontal connecting bar linking to next cell, full alpha
3. **Active + OFF**: dark/empty square, visible border, no fill
4. **Inactive (beyond length)**: any state rendered at 40% alpha — uniformly grayed out, still clickable

---

## Area 3: RND SYNC Button Polish

### Problem
The current `rndSyncBtn_` boolean toggle is visually ambiguous and doesn't expose the full 3-state behavior that already exists in the processor logic (FREE / INT / DAW modes).

### New behavior — 3-state cycling button
| State | Label | Color | Behavior |
|-------|-------|-------|----------|
| **FREE** | `FREE` | `gateOff` (gray/dim) | Poisson random firing — no grid, truly stochastic intervals |
| **INT** | `INT` | `gateOn` (accent green) | Locked to internal free-tempo BPM clock grid |
| **DAW** | `DAW` | `Colours::cornflowerblue` | Locked to DAW beat grid |

Single click cycles: FREE → INT → DAW → FREE.

### APVTS change
The current `randomClockSync` bool becomes a **3-value Choice param** (`randomSyncMode`) with options "FREE" / "INT" / "DAW". The processor already has 3 distinct code branches — wire to the new param instead of the old bool + implicit DAW-playing check.

> **Preset compatibility**: existing presets with `randomClockSync=false` → load as FREE; `randomClockSync=true` → load as INT. Handle in `setStateInformation`.

### Visual style
Match the LFO SYNC button exactly: same font size, same border radius, same overall dimensions. `TextButton` with `setClickingTogglesState(false)`, `onClick` manually steps the APVTS param.

### Location
Stays where it is — no positional change.

---

## Code Context

### Key files
| File | Relevance |
|------|-----------|
| `Source/PluginProcessor.cpp` | Arp step logic (~line 1900–1952), randomClockSync dispatch (~line 1730+), APVTS param registration (~line 284–300) |
| `Source/PluginProcessor.h` | `arpStep_`, `arpActivePitch_`, `arpNoteOffRemaining_`, `arpWaitingForPlay_` (~line 384–394) |
| `Source/PluginEditor.cpp` | Arp box layout (~line 4040–4057), arp control init (~line 3057–3102), `kArpH=84` (~line 3876), `kLeftColW=448` (~line 3577) |
| `Source/PluginEditor.h` | `arpEnabledBtn_`, `arpSubdivBox_`, `arpOrderBox_`, `arpGateTimeKnob_` (~line 449–453) |

### Existing arp loop entry point
```cpp
// PluginProcessor.cpp ~line 1902
// Build fixed-voice step sequence (all 4 voices, stack-allocated max 8).
int seq[8]; int seqLen = 0;
// switch(orderIdx) builds seq[] based on Order param
```
Step pattern logic wraps around this: before firing step `seq[arpStep_]`, check `arpStepState[arpStep_]`. ON=fire, TIE=sustain, OFF=note-off+skip.

### New APVTS params summary
```
arpStepState0..7   Choice "ON"/"TIE"/"OFF"  default ON    (8 params)
arpLength          Int    1..8               default 8
randomSyncMode     Choice "FREE"/"INT"/"DAW" default FREE  (replaces randomClockSync bool)
```
