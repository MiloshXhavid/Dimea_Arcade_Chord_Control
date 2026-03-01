# Phase 21: Left Joystick Modulation Expansion — Research

**Researched:** 2026-03-01
**Domain:** JUCE APVTS extension, C++ audio-thread/message-thread dispatch, MIDI CC routing
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**X Axis Target List (6 items, in order)**

| Index | Label | Dispatch |
|-------|-------|----------|
| 0 | Cutoff (CC74) | MIDI CC74 — existing behavior |
| 1 | VCF LFO (CC12) | MIDI CC12 — existing behavior |
| 2 | LFO-X Freq | Direct APVTS write to lfoXRate |
| 3 | LFO-X Phase | Direct APVTS write to lfoXPhase |
| 4 | LFO-X Level | Direct APVTS write to lfoXLevel |
| 5 | Gate Length | Direct APVTS write to gateLength |

Note: Mod Wheel (CC1) dropped from list — replaced by LFO targets.

**Y Axis Target List (6 items, symmetric)**

| Index | Label | Dispatch |
|-------|-------|----------|
| 0 | Resonance (CC71) | MIDI CC71 — existing behavior |
| 1 | LFO Rate (CC76) | MIDI CC76 — existing behavior |
| 2 | LFO-Y Freq | Direct APVTS write to lfoYRate |
| 3 | LFO-Y Phase | Direct APVTS write to lfoYPhase |
| 4 | LFO-Y Level | Direct APVTS write to lfoYLevel |
| 5 | Gate Length | Direct APVTS write to gateLength |

**LFO Sync + Freq Target Behavior**
- When LFO Sync is ON and stick targets LFO Freq: stick scales the sync subdivision (multiplier 0.25x–4x applied to current sync rate), stays grid-locked, does NOT switch to free mode
- When LFO Sync is OFF and target is LFO Freq: stick drives the free Hz rate directly

**Attenuation Knob Relabeling**

| Target | Atten label | Meaning |
|--------|-------------|---------|
| Cutoff (CC74) | Atten | CC output scale 0–127% (current) |
| VCF LFO (CC12) | Atten | CC output scale 0–127% (current) |
| LFO Rate (CC76) | Atten | CC output scale 0–127% (current) |
| Resonance (CC71) | Atten | CC output scale 0–127% (current) |
| LFO-X/Y Freq | Hz | How many Hz of the LFO rate range the stick sweeps |
| LFO-X/Y Phase | Phase | How much of the phase range (0–2π) the stick sweeps |
| LFO-X/Y Level | Level | How much of the level range (0–1) the stick sweeps |
| Gate Length | Gate | How much of the gate range (0–1) the stick sweeps |

Claude's Discretion: exact scaling ranges for each LFO target (e.g. Freq sweep 0–20Hz, Level sweep 0–1.0). Choose sensibly based on existing APVTS param ranges.

**CC Cleanup on Target Switch**
- When switching from CC target to LFO target: stop emitting that CC immediately — `prevCcCut_` / `prevCcRes_` reset to -1 on mode change; extend existing pattern to cover all 6 targets

**Dispatch Architecture**
- Indices 0–1 (X) and 0–1 (Y): existing CC dispatch path, unchanged
- Indices 2–5: "pending-atomic dispatch" — write stick value to an atomic float that the audio thread reads each processBlock and applies to target APVTS param (same pattern as modulatedJoyX_/Y_ atomics)
- Gate Length target: writes to unified `gateLength` APVTS param (shared with Arp + Random)

### Claude's Discretion

Exact scaling ranges for each LFO target:
- Freq sweep range when sync OFF: full lfoXRate / lfoYRate range (0.01–20 Hz); Atten knob (renamed "Hz") acts as depth limiter
- Phase sweep range: full lfoXPhase / lfoYPhase range (0–360 degrees)
- Level sweep range: full lfoXLevel / lfoYLevel range (0.0–1.0)
- Gate Length sweep range: full gateLength range (0.0–1.0)
- When sync ON and freq target: stick position maps to multiplier float in [0.25, 4.0]; Atten knob (renamed "Hz") gates this multiplier range (0% atten = no effect, 100% = full ±4x range)

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| LJOY-01 | Left Joystick X dropdown offers: Filter Cutoff (CC74), Filter Resonance (CC71), LFO-X Frequency, LFO-X Phase, LFO-X Level, Gate Length | APVTS filterXMode param extended from 3 to 6 choices; existing ComboBox widget reused; ccXnum switch extended |
| LJOY-02 | Left Joystick Y dropdown offers same set with LFO-Y variants | APVTS filterYMode param extended from 2 to 6 choices; symmetric implementation to X |
| LJOY-03 | Left Joystick X dropdown appears before Left Joystick Y in the UI | filterXModeBox_ layout already appears after filterYModeBox_ in resized(); swap order in resized() |
| LJOY-04 | LFO Frequency target: when sync ON, stick scales subdivision multiplier (0.25x–4x), does NOT suppress | Requires reading lfoXSync/lfoYSync atomics in processBlock dispatch path; subdivision multiplier applied to subdivBeats |
</phase_requirements>

---

## Summary

Phase 21 extends the left gamepad stick X and Y axis target menus from their current small sets (X: 3 CC targets, Y: 2 CC targets) to 6 targets each, adding direct LFO parameter modulation and Gate Length control. The architecture is entirely within existing patterns: APVTS choice parameter extension, ComboBox item addition, CC dispatch switch extension, and a new atomic-float pending dispatch path for LFO/Gate targets.

The core challenge is the dispatch split: indices 0–1 continue using the existing MIDI CC path (unchanged), while indices 2–5 use a new "pending atomic float" path where the audio thread reads a pair of `std::atomic<float>` values each processBlock and applies them directly to APVTS raw parameter pointers — the same pattern already used for `modulatedJoyX_/Y_`. This is safe because the destination params (lfoXRate, lfoXPhase, lfoXLevel, lfoYRate, lfoYPhase, lfoYLevel, gateLength) are `std::atomic<float>` under the hood in JUCE APVTS, and writing from the audio thread to their raw value pointer is the project's established direct-write pattern.

One critical subtlety: REQUIREMENTS.md LJOY-04 says "LFO Frequency target is suppressed when sync active" but CONTEXT.md (locked decision) replaces this with subdivision scaling behavior (0.25x–4x multiplier). The CONTEXT.md decision governs. The Atten knob label changes per target in timerCallback() via the existing `styleLabel()` helper — no new mechanism required.

**Primary recommendation:** Implement in two tasks: (1) APVTS + processor dispatch extension, (2) UI ComboBox + label wiring. The processor task is the critical path; the UI task is mechanical.

---

## Standard Stack

### Core
| Component | Version | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| JUCE APVTS | 8.0.4 (FetchContent) | Parameter storage and persistence | Existing infrastructure; all params already registered here |
| `std::atomic<float>` | C++17 | Cross-thread stick value passing | Established project pattern for modulatedJoyX_/Y_; lock-free, audio-thread safe |
| JUCE ComboBox + ComboBoxAttachment | 8.0.4 | UI dropdown with APVTS binding | Already used for filterXMode/filterYMode; no new widget type needed |

### Supporting
| Component | Version | Purpose | When to Use |
|-----------|---------|---------|-------------|
| `apvts.getRawParameterValue()` | 8.0.4 | Direct atomic read from audio thread | Exact mechanism for reading all LFO param values in processBlock() |
| `juce::AudioProcessorParameter::setValueNotifyingHost()` | 8.0.4 | Normalized param write from message thread | Used for quantize/arp param writes in the existing codebase |
| `styleLabel()` helper | project | Label text update in timerCallback | Already used for all label text changes in PluginEditor |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Atomic float pending dispatch | APVTS Listener + ValueTree | ValueTree is not audio-thread safe; atomic float IS the correct pattern here |
| Atomic float pending dispatch | Direct param write from audio thread via `setValueNotifyingHost` | `setValueNotifyingHost` posts to message thread — causes ~20ms lag; atomic direct write is instant |

**No npm install** — this is a C++ JUCE project, no package manager.

---

## Architecture Patterns

### Recommended File Structure

Only two files change in this phase:

```
Source/
├── PluginProcessor.cpp    # APVTS layout + processBlock dispatch (primary)
├── PluginEditor.cpp       # ComboBox items + timerCallback label update
└── PluginProcessor.h      # New atomic<float> members for pending LFO dispatch
```

### Pattern 1: APVTS Choice Extension

**What:** Replace the existing 3-item `filterXMode` and 2-item `filterYMode` StringArrays with 6-item arrays.

**Critical constraint:** APVTS choice parameters encode their value as a 0-based integer. Extending from 3→6 items means existing saved presets that stored values 0, 1, 2 load correctly — indices 0 and 1 keep the same dispatch. Index 2 (previously "Mod Wheel" for X) will now map to "LFO-X Freq" — sessions that had CC1/Mod Wheel selected will silently switch to LFO-X Freq on load. This is acceptable (Mod Wheel is being intentionally dropped per the decision).

```cpp
// Source: PluginProcessor.cpp createParameterLayout() — current code
juce::StringArray yModes { "Resonance", "LFO Rate" };
juce::StringArray xModes { "Cutoff",    "VCF LFO",  "Mod Wheel" };

// Replacement
juce::StringArray xModes { "Cutoff (CC74)", "VCF LFO (CC12)", "LFO-X Freq",
                            "LFO-X Phase",  "LFO-X Level",    "Gate Length" };
juce::StringArray yModes { "Resonance (CC71)", "LFO Rate (CC76)", "LFO-Y Freq",
                            "LFO-Y Phase",     "LFO-Y Level",     "Gate Length" };
addChoice("filterXMode", "Left Stick X Mode", xModes, 0);
addChoice("filterYMode", "Left Stick Y Mode", yModes, 0);
```

### Pattern 2: Pending-Atomic LFO Dispatch in processBlock()

**What:** Two new `std::atomic<float>` members (`pendingLfoX_`, `pendingLfoY_`) receive the stick value from the CC dispatch path. Each processBlock, before reading LFO params, check if mode is indices 2–5 and apply the pending value to the target APVTS param via direct raw pointer write.

**Why atomic float + direct raw pointer, not `setValueNotifyingHost`:**
`setValueNotifyingHost` is not audio-thread safe (it posts to the message queue). `getRawParameterValue()` returns a `std::atomic<float>*` — direct store to this pointer IS audio-thread safe and is the established project pattern.

```cpp
// PluginProcessor.h — new members
std::atomic<float> pendingStickX_ { 0.0f };  // normalised -1..+1, written by CC dispatch
std::atomic<float> pendingStickY_ { 0.0f };  // normalised -1..+1, written by CC dispatch

// PluginProcessor.cpp — processBlock(), inside the filterModOn block
// After reading xMode / yMode:
if (xMode >= 2 && xMode <= 5)
{
    const float stickVal = pendingStickX_.load(std::memory_order_relaxed);  // -1..+1
    const float atten    = xAtten / 100.0f;  // 0..1
    // Map stick to target param range, scaled by Atten
    applyStickToParam(xMode, stickVal, atten, /*isX=*/true);
}
// (symmetric for Y)
```

**`applyStickToParam` inline logic** (no new method needed, just inline):
```cpp
auto writeParam = [&](const juce::String& id, float val)
{
    if (auto* raw = apvts.getRawParameterValue(id))
        raw->store(val, std::memory_order_relaxed);
};

// stick -1..+1, atten 0..1
switch (xMode)
{
    case 2: { // LFO-X Freq
        const bool syncOn = *apvts.getRawParameterValue(ParamID::lfoXSync) > 0.5f;
        if (syncOn) {
            // Subdivision scaling: map stick to [0.25, 4.0] multiplier
            // stick -1 = 0.25x, center = 1.0x (no effect), +1 = 4.0x
            // Blend: baseMultiplier + stick * (maxRange * atten)
            // Stored as a new atomic float read by LFO block
            pendingLfoXSubdivMult_.store(std::pow(4.0f, stickVal * atten),
                                         std::memory_order_relaxed);
        } else {
            // Direct Hz: map stick 0..1 → 0.01..20 Hz, center = 1 Hz
            // Stick -1..+1 → normalized 0..1; apply to lfoXRate range
            const float norm = (stickVal * atten + 1.0f) * 0.5f;  // 0..1
            const float hz   = 0.01f + norm * (20.0f - 0.01f);
            writeParam(ParamID::lfoXRate, hz);
        }
        break;
    }
    case 3: { // LFO-X Phase
        const float degrees = (stickVal * atten + 1.0f) * 0.5f * 360.0f;
        writeParam(ParamID::lfoXPhase, degrees);
        break;
    }
    case 4: { // LFO-X Level
        const float level = (stickVal * atten + 1.0f) * 0.5f;  // 0..1
        writeParam(ParamID::lfoXLevel, level);
        break;
    }
    case 5: { // Gate Length
        const float gate = (stickVal * atten + 1.0f) * 0.5f;  // 0..1
        writeParam(ParamID::gateLength, gate);
        break;
    }
}
```

**Note on lfoXRate log skew:** The `lfoXRate` param has a log skew factor of 0.2306. Direct linear store to the raw float still works — the param stores the actual Hz value, not the normalized position. The skew only affects slider display. Storing 5.0f sets the param to 5 Hz regardless of skew.

### Pattern 3: CC Cleanup on Mode Switch to LFO Target

**What:** Extend the existing `prevXMode_`/`prevYMode_` change detection to reset CC dedup when switching to any of the 6 modes (not just between CC modes).

```cpp
// Current code (PluginProcessor.cpp:1477-1478):
if (xMode != prevXMode_) { prevCcCut_.store(-1, std::memory_order_relaxed); prevXMode_ = xMode; }
if (yMode != prevYMode_) { prevCcRes_.store(-1, std::memory_order_relaxed); prevYMode_ = yMode; }
```

This code already handles all mode transitions — no change needed here. When switching from index 0 (CC74) to index 2 (LFO-X Freq), `xMode != prevXMode_` fires and resets `prevCcCut_` to -1. The CC dispatch path then won't emit CC74 because indices 2–5 branch away from the CC emit block. The -1 sentinel in prevCcCut_ causes a force-send on the next CC mode restore — correct behavior.

**Additional: zero-emit on CC side when switching away.** The current pattern sends one final CC=0 on gamepad disconnect (`pendingCcReset_`). On target switch, no zero-send is specified — the CONTEXT says "stop emitting that CC immediately" which the branch-away achieves without a zero send. This is correct.

### Pattern 4: UI ComboBox Item Replacement

**What:** Replace `filterXModeBox_.addItem()` calls in PluginEditor.cpp constructor with the new 6-item lists.

```cpp
// Current (PluginEditor.cpp ~line 1451):
filterYModeBox_.addItem("Resonance (CC71)", 1);
filterYModeBox_.addItem("LFO Rate  (CC76)", 2);

filterXModeBox_.addItem("Cutoff    (CC74)", 1);
filterXModeBox_.addItem("VCF LFO   (CC12)", 2);
filterXModeBox_.addItem("Mod Wheel (CC1)",  3);

// Replacement:
filterXModeBox_.addItem("Cutoff (CC74)", 1);
filterXModeBox_.addItem("VCF LFO (CC12)", 2);
filterXModeBox_.addItem("LFO-X Freq",    3);
filterXModeBox_.addItem("LFO-X Phase",   4);
filterXModeBox_.addItem("LFO-X Level",   5);
filterXModeBox_.addItem("Gate Length",   6);

filterYModeBox_.addItem("Resonance (CC71)", 1);
filterYModeBox_.addItem("LFO Rate (CC76)",  2);
filterYModeBox_.addItem("LFO-Y Freq",       3);
filterYModeBox_.addItem("LFO-Y Phase",      4);
filterYModeBox_.addItem("LFO-Y Level",      5);
filterYModeBox_.addItem("Gate Length",      6);
```

**ComboAtt still binds to "filterXMode" / "filterYMode" — no change to attachment.**

### Pattern 5: Atten Label Relabeling in timerCallback()

**What:** Each 30 Hz timer tick, read the current combo selection and call `styleLabel()` to update the Atten knob label.

```cpp
// In timerCallback() — after the existing filterOn block:
{
    const int xMode = (int)*proc_.apvts.getRawParameterValue("filterXMode");
    const juce::String xLabel = (xMode == 2) ? "Hz"
                              : (xMode == 3) ? "Phase"
                              : (xMode == 4) ? "Level"
                              : (xMode == 5) ? "Gate"
                              : "Atten";
    if (filterXAttenLabel_.getText() != xLabel)
        styleLabel(filterXAttenLabel_, xLabel);

    const int yMode = (int)*proc_.apvts.getRawParameterValue("filterYMode");
    const juce::String yLabel = (yMode == 2) ? "Hz"
                              : (yMode == 3) ? "Phase"
                              : (yMode == 4) ? "Level"
                              : (yMode == 5) ? "Gate"
                              : "Atten";
    if (filterYAttenLabel_.getText() != yLabel)
        styleLabel(filterYAttenLabel_, yLabel);
}
```

The conditional `if (text != current)` guard avoids calling `setText` every 30 Hz tick when nothing changed — keeps UI repaint minimal.

### Pattern 6: LJOY-03 — X Before Y in UI

**What:** Current layout in `resized()` places `filterYModeBox_` before `filterXModeBox_` (Y is drawn first at top). The requirement says X must appear before Y. Swap the two `setBounds` calls in `resized()`.

```cpp
// Current (PluginEditor.cpp ~line 1863):
right.removeFromTop(12);
filterYModeBox_.setBounds(right.removeFromTop(22));
right.removeFromTop(12);
filterXModeBox_.setBounds(right.removeFromTop(22));

// Fixed:
right.removeFromTop(12);
filterXModeBox_.setBounds(right.removeFromTop(22));
right.removeFromTop(12);
filterYModeBox_.setBounds(right.removeFromTop(22));
```

Also swap the labels in `paint()` where `drawAbove(filterYModeBox_, "LEFT Y")` and `drawAbove(filterXModeBox_, "LEFT X")` appear — these auto-position relative to the widget's current bounds, so swapping bounds automatically fixes them.

### Anti-Patterns to Avoid

- **Calling `setValueNotifyingHost()` from the audio thread:** This posts a message to the message queue and is not truly audio-thread safe. Use direct raw pointer store (`raw->store(val)`) instead.
- **Writing to APVTS from the CC dispatch branch unconditionally every block:** This causes continuous APVTS churn. Gate writes behind a `stickUpdated` check (same pattern as existing CC dedup).
- **Using a separate atomic per LFO param:** Two atomics (`pendingStickX_`, `pendingStickY_`) are sufficient — the mode index tells processBlock which param to write. No need for 7 separate atomics.
- **Subdivisions scaling snapping to discrete steps:** The CONTEXT says "feels continuous/smooth" — use a continuous float multiplier (`std::pow(4.0f, stickVal * atten)`) rather than mapping to a discrete subdivision index.
- **Forgetting that filterXMode item indices in JUCE ComboBox are 1-based:** ComboBox items use 1-based IDs but APVTS choice index is 0-based. ComboBoxAttachment handles the mapping — but when reading `getRawParameterValue("filterXMode")` directly in processBlock, you get a 0-based integer.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| LFO subdivision scaling | Custom integer subdivision snap | Continuous `std::pow(4.0f, stick * atten)` float multiplier stored in atomic | Discrete snapping breaks "analog feel" requirement |
| Atten label sync | Separate listener or ValueTree callback | timerCallback() poll | Listener adds complexity; 30 Hz poll is already in place and matches existing pattern |
| Per-target prev-value dedup for LFO writes | Per-param "prevLfoX_" floats | Write only when `stickUpdated` is true | The existing `stickUpdated` flag already gates the entire block — piggyback on it |

**Key insight:** This phase is purely extension of existing infrastructure. Every mechanism (atomics, dedup, timerCallback polling, styleLabel, APVTS raw reads) already exists — this phase wires them together in new combinations.

---

## Common Pitfalls

### Pitfall 1: APVTS Choice Size Mismatch (Runtime Assertion)
**What goes wrong:** If the APVTS layout registers 6 choices but the ComboBox in the editor adds only 3 items (or vice versa), JUCE asserts in debug builds and silently misbehaves in release.
**Why it happens:** The APVTS parameter definition and the ComboBox item list are maintained separately.
**How to avoid:** Change both `createParameterLayout()` and `PluginEditor.cpp` constructor in the same task. The APVTS change must come first (or be done atomically) because the editor reads the layout on construction.
**Warning signs:** `jassert` firing with "parameter value out of range" in debug build.

### Pitfall 2: CC Bleed After Target Switch
**What goes wrong:** When switching from CC74 to LFO-X Freq, the old CC74 value lingers on the synth (the stick stopped sending CC74 but never sent CC74=0 on switch).
**Why it happens:** The branch-away from the CC emit block stops new CC74 emissions but doesn't zero the synth's filter state.
**How to avoid:** Per CONTEXT decision, the spec says "stop emitting that CC immediately" — no zero-send is required. The existing `prevCcCut_.store(-1)` on mode change ensures that when the mode switches back to CC74, the first emission will be a full send. If the user wants zero on switch, they can disconnect the gamepad (which sends pendingCcReset_). This is the accepted behavior.
**Warning signs:** Synth filter stuck at non-zero value after target switch — expected and acceptable per CONTEXT.

### Pitfall 3: Log-Skew Raw Write Produces Wrong Display Value
**What goes wrong:** Writing a linear Hz value (e.g. 5.0f) directly to the `lfoXRate` raw atomic works correctly for the DSP (LfoEngine reads it as 5 Hz), but the slider display may show a different position than expected because the slider's NormalisableRange has a log skew (0.2306).
**Why it happens:** The slider display converts the stored value back through the NormalisableRange for visual position; the stored value itself is in real units (Hz), which is correct.
**How to avoid:** This is correct behavior. The slider will show the correct Hz value in its text box. The visual position on the slider track will reflect the log scale, which is also correct. Do not compensate.
**Warning signs:** None — this is correct behavior, not a bug.

### Pitfall 4: Subdivision Multiplier Not Applied to LFO Processing
**What goes wrong:** The subdivision scaling (0.25x–4x) is stored in a new atomic but the LFO processBlock section doesn't read it when building `ProcessParams`.
**Why it happens:** The LFO block (lines 608–680 of processBlock) assembles `ProcessParams` from APVTS and calls `lfoX_.process()`. A new `pendingLfoXSubdivMult_` atomic won't be read unless explicitly wired into the `xp.subdivBeats` calculation.
**How to avoid:** In the LFO block, multiply `xp.subdivBeats` by `pendingLfoXSubdivMult_.load()` when `xMode == 2` (LFO-X Freq target) and sync is on. Only when xMode==2 and syncOn, otherwise the multiplier is 1.0 (no-op).
**Warning signs:** Stick movement has no audible effect on LFO rate in sync mode.

### Pitfall 5: Gate Length Conflict Between Two Axes
**What goes wrong:** Both X and Y axes can be set to "Gate Length" simultaneously. Each processBlock write would overwrite the other's value.
**Why it happens:** `gateLength` is a single APVTS param, and both stick dispatches write to it.
**How to avoid:** The last write wins. This is acceptable — two axes fighting over the same param is the user's configuration choice. No special handling needed. Document in tooltips that both X and Y targeting Gate Length simultaneously produces indeterminate blending.
**Warning signs:** Gate length "jitters" when both axes are on Gate Length target — expected, not a bug.

### Pitfall 6: LJOY-03 Layout Swap Breaks paint() Labels
**What goes wrong:** Swapping the `setBounds` calls in `resized()` but forgetting to also verify that `paint()` labels draw above the correct combo box — if labels reference old hardcoded positions, they'll be misplaced.
**Why it happens:** `drawAbove(widget, "text")` in paint() calculates position from the widget's current bounds — so if bounds are swapped correctly, `drawAbove` automatically follows. But only if the swap is complete in `resized()`.
**How to avoid:** The existing `drawAbove()` helper is position-independent — it reads `widget.getBounds()` live. Swapping bounds in `resized()` is sufficient. No paint() change needed.
**Warning signs:** "LEFT X" label appears above the Y combo box — missed a `removeFromTop` line in resized().

---

## Code Examples

### Reading filterXMode in processBlock (confirmed pattern)

```cpp
// Source: PluginProcessor.cpp lines 1472–1478 — existing pattern
const int xMode  = (int)apvts.getRawParameterValue("filterXMode")->load();
const int yMode  = (int)apvts.getRawParameterValue("filterYMode")->load();
const int ccXnum = (xMode == 1) ? 12 : (xMode == 2) ? 1 : 74;
const int ccYnum = (yMode == 1) ? 76 : 71;

if (xMode != prevXMode_) { prevCcCut_.store(-1, std::memory_order_relaxed); prevXMode_ = xMode; }
if (yMode != prevYMode_) { prevCcRes_.store(-1, std::memory_order_relaxed); prevYMode_ = yMode; }
```

Extended for 6 modes (index 0–5, APVTS returns 0-based):

```cpp
// Extended ccXnum for 6-item list: indices 0–1 are CC, indices 2–5 are LFO params
// For indices 2–5, ccXnum is unused (no CC emitted)
const int ccXnum = (xMode == 1) ? 12 : 74;  // 0=CC74, 1=CC12; 2-5 branch away
const int ccYnum = (yMode == 1) ? 76 : 71;  // 0=CC71, 1=CC76; 2-5 branch away
```

### Direct Raw Param Write (audio-thread safe pattern)

```cpp
// Source: established project pattern — getRawParameterValue returns std::atomic<float>*
// Direct store is safe from audio thread (same as modulatedJoyX_ stores)
if (auto* raw = apvts.getRawParameterValue(ParamID::lfoXRate))
    raw->store(hzValue, std::memory_order_relaxed);
```

### Subdivision Multiplier (sync mode continuous scaling)

```cpp
// stick in [-1, +1], atten in [0, 1]
// Maps: stick=-1 → 0.25x, stick=0 → 1.0x, stick=+1 → 4.0x
// std::pow(4.0f, -1 * 1.0) = 0.25, pow(4.0f, 0) = 1.0, pow(4.0f, 1) = 4.0
const float mult = std::pow(4.0f, stickVal * atten);
pendingLfoXSubdivMult_.store(mult, std::memory_order_relaxed);

// In LFO processBlock section, when xMode==2 and syncOn:
xp.subdivBeats = kLfoSubdivBeats[subdivIdx]
               * pendingLfoXSubdivMult_.load(std::memory_order_relaxed);
```

### LFO Param Ranges (confirmed from createParameterLayout)

```cpp
// lfoXRate / lfoYRate: NormalisableRange<float>(0.01f, 20.0f, 0.0f, 0.2306f), default 1.0f
// lfoXPhase / lfoYPhase: addFloat(id, name, 0.0f, 360.0f, 0.0f)   — degrees
// lfoXLevel / lfoYLevel: addFloat(id, name, 0.0f, 1.0f,   0.0f)   — 0..1
// gateLength:            addFloat(id, name, 0.0f, 1.0f,   0.0f)   — 0..1
//
// Stick -1..+1 maps to param range:
// Freq:  hz = 0.01 + ((stick * atten + 1) / 2) * (20.0 - 0.01)
// Phase: deg = ((stick * atten + 1) / 2) * 360.0
// Level: lvl = (stick * atten + 1) / 2
// Gate:  gate = (stick * atten + 1) / 2
```

### styleLabel call (confirmed in PluginEditor.cpp line 831)

```cpp
static void styleLabel(juce::Label& l, const juce::String& text)
{
    l.setText(text, juce::dontSendNotification);
    l.setFont(juce::Font(11.0f));
    l.setJustificationType(juce::Justification::centred);
}
// Called from timerCallback to relabel Atten knob per current mode
```

---

## State of the Art

| Old Approach | Current Approach | Impact |
|--------------|-----------------|--------|
| 3-item filterXMode (Cutoff / VCF LFO / Mod Wheel) | 6-item filterXMode (+LFO-X Freq/Phase/Level + Gate Length) | Mod Wheel CC1 slot repurposed; saved presets with index 2 silently switch to LFO-X Freq |
| 2-item filterYMode (Resonance / LFO Rate) | 6-item filterYMode (+LFO-Y Freq/Phase/Level + Gate Length) | New targets added; indices 0–1 backward-compatible |

**Deprecated/outdated:**
- CC1 Mod Wheel target on X axis: removed from list; index 2 now means LFO-X Freq. Any preset saved with X=Mod Wheel will load as X=LFO-X Freq.

---

## Open Questions

1. **Subdivision multiplier storage: dedicated atomic or inline calculation**
   - What we know: A `pendingLfoXSubdivMult_` atomic float must be read in the LFO processBlock section (lines 608–680)
   - What's unclear: Should this be a dedicated `std::atomic<float>` member in PluginProcessor.h, or should the stick dispatch write directly to an atomic that the LFO section reads inline?
   - Recommendation: Add `std::atomic<float> lfoXSubdivMult_ { 1.0f }` and `lfoYSubdivMult_ { 1.0f }` to PluginProcessor.h, initialized to 1.0 (no-op). Updated in the filter dispatch block when mode==2 and syncOn. Read in the LFO processBlock section when mode==2 and syncOn. This keeps the two sections decoupled.

2. **Stick-to-param write gating: use `stickUpdated` flag or always write**
   - What we know: The existing CC dispatch is gated by `stickUpdated || baseChanged` — only sends CC when the stick moved or the base knob changed
   - What's unclear: Should LFO param writes use the same gate, or write every block (simpler but causes continuous APVTS churn)?
   - Recommendation: Gate LFO param writes behind the same `stickUpdated` flag — this matches the existing dedup pattern and avoids locking the audio thread on APVTS stores every block when the stick isn't moving.

3. **Atten knob suffix in LFO mode**
   - What we know: The knob currently uses `.setTextValueSuffix(" %")` which will display as "99 %" in LFO mode even when relabeled "Hz"
   - What's unclear: Should the suffix be removed or changed when in LFO mode?
   - Recommendation: In timerCallback(), alongside the label update, also call `filterXAttenKnob_.setTextValueSuffix(xMode >= 2 ? "" : " %")` to strip the suffix for LFO modes. This avoids confusing "99 Hz" display when the Atten knob controls a depth, not a frequency.

---

## Sources

### Primary (HIGH confidence)
- PluginProcessor.cpp (read directly) — APVTS layout, filterXMode/filterYMode params, processBlock CC dispatch pattern, prevXMode_/prevCcCut_ reset pattern, LFO processBlock section, modulatedJoyX_/Y_ atomic pattern
- PluginProcessor.h (read directly) — atomic member types, established atomic float pattern
- PluginEditor.cpp (read directly) — filterXModeBox_ item list, filterXAttenLabel_ styleLabel calls, timerCallback structure, resized() layout order
- PluginEditor.h (read directly) — member declarations for filterXModeBox_, filterXAttenKnob_, filterXAttenLabel_, attachment types
- LfoEngine.h (read directly) — ProcessParams struct, rateHz/phaseShift/level field ranges and semantics

### Secondary (MEDIUM confidence)
- .planning/phases/21-left-joystick-modulation-expansion/21-CONTEXT.md — locked architecture decisions (dispatch pattern, sync behavior, label scheme)
- .planning/REQUIREMENTS.md — LJOY-01..04 requirements text, confirmed LJOY-04 is overridden by CONTEXT.md subdivision-scaling decision

### Tertiary (LOW confidence)
- None

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all components verified directly in source code
- Architecture: HIGH — dispatch patterns, param ranges, and integration points confirmed from source
- Pitfalls: HIGH — each pitfall identified from direct code reading (log skew, ComboBox 1-based IDs, subdivision multiplier wiring)
- LJOY-04 behavior: HIGH — CONTEXT.md explicitly overrides REQUIREMENTS.md LJOY-04 text with subdivision-scaling decision

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable C++/JUCE project; no fast-moving dependencies)
