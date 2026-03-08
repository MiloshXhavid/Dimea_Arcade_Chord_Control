# Phase 45 — Arpeggiator Step Patterns + UI Redesign — Research

**Researched:** 2026-03-08
**Domain:** JUCE VST3, C++17, custom Component paint/hit-testing, APVTS param migration
**Confidence:** HIGH — all findings verified directly from source files, no speculation

---

## Summary

Phase 45 has three self-contained deliverables. The codebase is well-structured and
provides clear precedents for all three: the ScaleKeyboard class already demonstrates
the exact sub-region hit-test pattern needed for the step grid; addChoice / AudioParameterChoice
is used in a dozen places and the migration path from bool is straightforward; the arp step
loop is compact and its extension point is unambiguous.

The biggest implementation risk is the bool-to-Choice APVTS migration for `randomClockSync`.
The existing `setStateInformation` is a plain `apvts.replaceState(xml)` with no migration
shim — old presets that contain a `<randomClockSync value="1"/>` child will not be recognized
by a new `randomSyncMode` AudioParameterChoice, so they will silently load as the default
(FREE = 0). The CONTEXT.md requires a migration shim: the planner must include a Wave 0
task that adds XML-migration code to `setStateInformation` before the param rename lands.

The step grid uses a paint-inside-PluginEditor approach (like every other sub-panel) rather
than a new Component class. The ScaleKeyboard pattern (noteRect helper + two-pass hit test
in mouseDown) is the canonical model; adapt it directly.

**Primary recommendation:** Implement the step grid as a non-Component sub-panel drawn in
`PluginEditor::paint()` + hit-tested in `PluginEditor::mouseDown()`, following the ScaleKeyboard
pattern exactly. Register 8 + 1 + 1 = 10 new APVTS params. Replace the `randomClockSync`
bool with a `randomSyncMode` Choice param and add a migration shim in `setStateInformation`.

---

## User Constraints (from CONTEXT.md)

### Locked Decisions
- 8-step pattern grid: 2 rows × 4 columns, ON / TIE / OFF states per cell
- `kArpH = 84` stays unchanged — no box resize
- Pixel budget: 14 header + 20 ARP ON + 14 row A + 14 row B + 0 gap + 20 controls + 2 breathing = 84px
- Single left-click cycles state: ON → TIE → OFF → ON
- First step = TIE treated as ON at playback
- Cells beyond arpLength are clickable but grayed out at 40% alpha
- OFF = immediate note-off; TIE = sustain previous note, no re-trigger
- `arpStepState0..7`: Choice "ON"/"TIE"/"OFF", default "ON"
- `arpLength`: Int 1–8, default 8
- Labels row (`arpSubdivLabel_`, `arpOrderLabel_`, `arpGateTimeLabel_`) removed
- Controls row: 4 equal-width items — Rate | Order | LEN | Gate
- `arpLengthBox_` new combo "1".."8", default "8"
- `randomSyncMode`: Choice "FREE"/"INT"/"DAW", default FREE (replaces `randomClockSync` bool)
- FREE = Poisson, INT = internal BPM grid, DAW = DAW beat grid
- Visual: cycles same style as LFO SYNC button
- Preset compat: `randomClockSync=false` → FREE, `randomClockSync=true` → INT

### Claude's Discretion
- None specified — all areas are locked decisions or standard implementation details

### Deferred Ideas (OUT OF SCOPE)
- LFO heart/star preset
- Dual-sequencer / Subharmonicon mode (two independent 4-step rows)
- Any arp-order behavior changes beyond step states

---

## Area 1: Step Grid — Paint and Hit-Test Pattern

### Canonical pattern: ScaleKeyboard

`ScaleKeyboard` (PluginEditor.h line 221, PluginEditor.cpp lines 2037–2159) is the exact
model for the step grid. Key elements:

**`noteRect(int note) const`** — computes a `juce::Rectangle<float>` from note index and
component dimensions. The arp equivalent computes a cell rect from (row, col) and
`arpBlockBounds_`.

**`mouseDown(const juce::MouseEvent& e)` (line 2064–2118)** — uses a lambda that calls
`noteRect(n).contains(e.position)` and cycles the APVTS param if hit. Two-pass strategy
(black keys first) is not needed for the grid — a single left-to-right, top-to-bottom scan
of 8 cells is sufficient.

**`paint(juce::Graphics& g)` (line 2120–2159)** — iterates cells, fills and outlines based
on state. Directly translatable to the 4-state arp grid (Active ON / Active TIE / Active
OFF / Inactive-beyond-length).

**Critical difference from ScaleKeyboard:** ScaleKeyboard is a standalone `juce::Component`
with its own `mouseDown`. The arp step grid should NOT be a separate Component — it is drawn
in `PluginEditor::paint()` and hit-tested in `PluginEditor::mouseDown()`, consistent with how
every other sub-panel in this file works (LFO rows, looper position bar, quantizer box). The
bounds rectangle `arpBlockBounds_` is already computed in `resized()` and available in both
`paint()` and `mouseDown()`.

### Cell rect helper (inline in resized or as a local lambda in paint/mouseDown)

```cpp
// arpBlockBounds_ width = 448px; cell width = 448/4 = 112px; height = 14px
// rowIdx: 0 = cells 1–4, 1 = cells 5–8; colIdx: 0–3
auto arpCellRect = [&](int cellIndex) -> juce::Rectangle<int>
{
    const int colIdx = cellIndex % 4;
    const int rowIdx = cellIndex / 4;
    const int cellW  = arpBlockBounds_.getWidth() / 4;
    const int cellH  = 14;
    // grid starts at: top of arpBlockBounds_ + 14 (header) + 20 (ARP ON button) + 4 (gap, from existing layout)
    const int gridY  = arpBlockBounds_.getY() + 14 + 20 + 4;  // verify against resized() rebuild
    return { arpBlockBounds_.getX() + colIdx * cellW,
             gridY + rowIdx * cellH,
             cellW - 1,   // 1px gap between cols
             cellH };
};
```

Note: after the Phase 45 resized() rewrite, the ARP ON button row is 20px (currently 22px —
confirm whether it stays 22 or changes to 20 to meet the pixel budget). The exact gridY
offset must be computed from the rebuilt `resized()` layout, not hard-coded here.

### Hit-test in PluginEditor::mouseDown

`PluginEditor` does not currently override `mouseDown`. The step grid requires adding one.
Check whether there are any existing mouseDown handlers in PluginEditor — there are none
in the header (PluginEditor.h lines 255–586). The new mouseDown should:

1. Check `arpBlockBounds_` containment first (early-exit for non-arp clicks)
2. Loop over cells 0–7, compute `arpCellRect(i)`, test `contains(e.getPosition())`
3. On hit: read current APVTS value for `"arpStepState" + String(i)`, cycle 0→1→2→0, write back

```cpp
void PluginEditor::mouseDown(const juce::MouseEvent& e)
{
    if (!arpBlockBounds_.contains(e.getPosition()))
        return;
    for (int i = 0; i < 8; ++i)
    {
        if (!arpCellRect(i).contains(e.getPosition()))
            continue;
        auto* p = dynamic_cast<juce::AudioParameterChoice*>(
            proc_.apvts.getParameter("arpStepState" + juce::String(i)));
        if (p)
            p->setValueNotifyingHost(
                p->convertTo0to1(static_cast<float>((p->getIndex() + 1) % 3)));
        repaint(arpBlockBounds_);
        return;
    }
}
```

### 14px cell UX precedent

14px is the exact height used for every label row throughout the arp and LFO panels:
- `arpSubdivLabel_` row: `r.removeFromTop(14)` (PluginEditor.cpp line 4015)
- LFO header clearance: `col.removeFromTop(14)` (line 4029)
- `loopSubdivLabel_` header: `col1.removeFromTop(14)` (line 3919)

There are no existing **interactive** elements at 14px, but this codebase does not use JUCE
hit-testing margins or minimum-size enforcement — any rect returned by `contains()` is
treated as clickable regardless of height. The ScaleKeyboard black keys are ~h*0.6 of a
component height that is approximately 30px; the black key interactive rect is ~18px tall.
14px is tighter but consistent with the UI aesthetic.

---

## Area 2: APVTS Choice Param Pattern

### addChoice / AudioParameterChoice usage (PluginProcessor.cpp lines 114–117)

```cpp
// local helper lambda in createParameterLayout():
auto addChoice = [&](const juce::String& id, const juce::String& name,
                     juce::StringArray choices, int def)
{
    layout.add(std::make_unique<juce::AudioParameterChoice>(id, name, choices, def));
};
```

This is the pattern to use for all 10 new params. Example for arpStepState:

```cpp
const juce::StringArray kStepStates { "ON", "TIE", "OFF" };
for (int i = 0; i < 8; ++i)
    addChoice("arpStepState" + juce::String(i),
              "Arp Step " + juce::String(i + 1) + " State",
              kStepStates, 0);  // default: ON (index 0)
```

And for arpLength:

```cpp
const juce::StringArray kLenChoices { "1","2","3","4","5","6","7","8" };
addChoice("arpLength", "Arp Length", kLenChoices, 7);  // default: "8" (index 7)
```

Note: CONTEXT.md says `arpLength` is an "Int param, range 1–8, default 8". Use addChoice
with string items "1".."8" (matching the ComboBox display) OR addInt. The ComboBox approach
(Choice) is simpler since the UI is a ComboBox — choice index 0="1", index 7="8". But addInt
1..8 also works — the planner should use whichever matches the ComboBoxAttachment approach.
Current pattern for similar params is `addChoice` for anything that drives a ComboBox
(see `arpSubdiv`, `arpOrder`). Use addChoice here for consistency.

### Reading Choice param in processBlock

```cpp
// Current arpOrder read pattern (PluginProcessor.cpp line 1812–1813):
const int orderIdx = juce::jlimit(0, 6,
    static_cast<int>(*apvts.getRawParameterValue(ParamID::arpOrder)));

// Same pattern for arpLength:
const int arpLen = juce::jlimit(1, 8,
    static_cast<int>(*apvts.getRawParameterValue("arpLength")) + 1);
// +1 because index 0 = length 1

// For arpStepState[i] (0=ON, 1=TIE, 2=OFF):
const int stepState = static_cast<int>(
    *apvts.getRawParameterValue("arpStepState" + juce::String(arpStep_)));
```

### ComboBoxAttachment for arpLengthBox_

No ComboBoxParameterAttachment needed — use the standard ComboAtt alias:

```cpp
// In PluginEditor.h — add to arp attachments (line 542–545):
std::unique_ptr<ComboAtt>   arpLengthAtt_;

// In constructor:
arpLengthBox_.addItemList({"1","2","3","4","5","6","7","8"}, 1);
styleCombo(arpLengthBox_);
addAndMakeVisible(arpLengthBox_);
arpLengthAtt_ = std::make_unique<ComboAtt>(p.apvts, "arpLength", arpLengthBox_);
```

---

## Area 3: Bool-to-Choice Param Migration

### Existing setStateInformation (PluginProcessor.cpp lines 2635–2640)

```cpp
void PluginProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
```

There is **no existing migration logic** — this is a clean passthrough. Adding migration
requires inserting XML-mutation code before the `replaceState` call.

### Migration shim pattern

APVTS stores param values as attributes on child XML elements. For a Bool param
`randomClockSync`, the stored XML child looks like:

```xml
<PARAM id="randomClockSync" value="1"/>
```

For the new Choice param `randomSyncMode`, the stored format is:

```xml
<PARAM id="randomSyncMode" value="1"/>   <!-- 0=FREE, 1=INT, 2=DAW -->
```

The migration shim must:
1. Look for a `randomClockSync` child in the loaded XML
2. Map its value to the new `randomSyncMode` value (false→0=FREE, true→1=INT)
3. Remove the old `randomClockSync` child
4. Add a `randomSyncMode` child with the mapped value

```cpp
void PluginProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        // ── Migration: randomClockSync (bool) → randomSyncMode (Choice 0=FREE/1=INT/2=DAW)
        if (auto* oldParam = xml->getChildByAttribute("id", "randomClockSync"))
        {
            const bool wasSync = (oldParam->getDoubleAttribute("value") > 0.5);
            xml->removeChildElement(oldParam, true);

            auto* newParam = xml->createNewChildElement("PARAM");
            newParam->setAttribute("id", "randomSyncMode");
            newParam->setAttribute("value", wasSync ? 1.0 : 0.0);  // INT=1, FREE=0
        }
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}
```

### Impact on gamepad dispatch (PluginProcessor.cpp line 764–769)

The gamepad `consumeRndSyncToggle()` path currently toggles the Bool param directly:

```cpp
// line 764–769 (current):
if (gamepad_.consumeRndSyncToggle())
{
    auto* rndParam = dynamic_cast<juce::AudioParameterBool*>(
        apvts.getParameter("randomClockSync"));
    if (rndParam != nullptr)
        rndParam->setValueNotifyingHost(rndParam->get() ? 0.0f : 1.0f);
}
```

This must be updated to cycle the new Choice param (0→1→2→0).

### TriggerSystem::ProcessParams

`TriggerSystem.h` line 99 declares `bool randomClockSync`. After the migration, this field
must be replaced by an int (0=FREE, 1=INT, 2=DAW) or keep the bool and map it at the
processBlock fill site. The minimal-change approach: keep `bool randomClockSync` in
ProcessParams but add `bool randomUseDaw` alongside, OR replace with `int randomSyncMode`.
The latter is cleaner since TriggerSystem already has two branches on the bool. All sites
that read `p.randomClockSync` in TriggerSystem.cpp must be updated:
- Line 95: `if (p.randomClockSync && ...` → `if (p.randomSyncMode >= 1 && ...`
- Line 112: same
- Line 129: `(!p.randomClockSync || !p.isDawPlaying)` →
  `(p.randomSyncMode == 0 || (p.randomSyncMode == 1) || !p.isDawPlaying)` — see Area 6
- Line 136: `if (!p.randomClockSync)` → `if (p.randomSyncMode == 0)`
- Line 279, 305, 345: same pattern

---

## Area 4: Arp Step Loop — Extension Points

### Full step loop (PluginProcessor.cpp lines 1887–1951)

The loop fires at most `stepsToFire` times per processBlock. The relevant section for
step-state integration is at line 1934–1950:

```cpp
const int voice = seq[arpStep_];           // line 1934
arpStep_ = (arpStep_ + 1) % seqLen;       // line 1935

// Skip voices where a pad is held (pad has priority).
if (trigger_.isGateOpen(voice)) continue; // line 1938

const int pitch = heldPitch_[voice];       // line 1940
// ... note-on fires unconditionally ...
```

**The arpStep_ counter indexes the ORDER sequence (seq[]), not the step pattern directly.**
The step pattern (arpStepState) is a *separate* layer: arpStepState[0..7] gates whether the
current firing event results in ON / TIE / OFF. The arpStep_ counter in the existing code
tracks position within `seq[]` which has up to 8 entries (Up+Down / Down+Up modes = 6).

**Integration point:** The step pattern should index by `arpStep_` modulo `arpLen`. Since
arpStep_ is reset to 0 at arpWaitingForPlay_ launch and incremented every `stepsToFire`
iteration, it naturally acts as the step-pattern index when arpLen = seqLen. When they
differ (e.g., seqLen = 6 for Up+Down, arpLen = 5), use `arpStep_` modulo `arpLen` to gate
the step pattern independently. The CONTEXT.md says "the grid visual is 2×4 but it's a
SINGLE 8-step sequence" — so arpStepState is indexed by step position in the overall
sequence, not by voice.

**Proposed extension at line 1934 (insert after `const int voice = seq[arpStep_]`):**

```cpp
// Read step-pattern state before advancing arpStep_
const int arpLen   = juce::jlimit(1, 8,
    static_cast<int>(*apvts.getRawParameterValue("arpLength")) + 1);
const int patStep  = arpStep_ % arpLen;   // current position in the pattern
const int stepState = static_cast<int>(
    *apvts.getRawParameterValue("arpStepState" + juce::String(patStep)));
// 0=ON, 1=TIE, 2=OFF

arpStep_ = (arpStep_ + 1) % seqLen;  // advance as normal

if (trigger_.isGateOpen(voice)) continue;  // pad priority (unchanged)

// Apply step state:
if (stepState == 2)  // OFF
{
    // Immediate note-off if any arp note is sounding
    if (arpActivePitch_ >= 0 && arpActiveCh_ > 0)
    {
        if (noteCount_[arpActiveCh_ - 1][arpActivePitch_] > 0 &&
            --noteCount_[arpActiveCh_ - 1][arpActivePitch_] == 0)
            midi.addEvent(juce::MidiMessage::noteOff(arpActiveCh_, arpActivePitch_, (uint8_t)0), 0);
        arpActivePitch_ = -1;
        arpActiveVoice_ = -1;
        arpCurrentVoice_.store(-1, std::memory_order_relaxed);
        arpNoteOffRemaining_ = 0.0;
    }
    continue;  // skip note-on
}

if (stepState == 1 && arpActivePitch_ >= 0)  // TIE (and previous note exists)
{
    // Sustain: do NOT send note-off. Reset gate timer so note keeps ringing.
    arpNoteOffRemaining_ = gateBeats;
    continue;  // skip note-on re-trigger
}
// stepState == 0 (ON) or stepState == 1 with no previous note → fall through to normal note-on
```

Note: the block at lines 1888–1900 (which cuts any still-sounding note at the step boundary)
fires unconditionally before the step-state check. For TIE, this means the current note is
cut first and then the TIE logic says "sustain". This is a conflict: the correct TIE
implementation must suppress the step-boundary cut for TIE steps. One approach: hoist the
step-state read before the step-boundary noteOff block at line 1888 and skip the cut when
stepState == TIE.

**Revised insertion point — before line 1888 (the step-boundary noteOff):**

```cpp
for (int s = 0; s < stepsToFire; ++s)
{
    // Read step-pattern state FIRST (before step-boundary noteOff)
    const int arpLen   = juce::jlimit(1, 8,
        static_cast<int>(*apvts.getRawParameterValue("arpLength")) + 1);
    const int patStep  = arpStep_ % arpLen;
    const int stepState = static_cast<int>(
        *apvts.getRawParameterValue("arpStepState" + juce::String(patStep)));

    const bool isTie = (stepState == 1) && (arpActivePitch_ >= 0);

    // Step-boundary noteOff — suppressed for TIE (sustain previous note)
    if (!isTie && arpActivePitch_ >= 0 && arpActiveVoice_ >= 0)
    {
        // ... existing noteOff code (lines 1890–1900) ...
    }

    // ... existing seq[] build (lines 1902–1929) ...

    if (seqLen == 0) continue;
    if (arpStep_ >= seqLen) arpStep_ = 0;

    const int voice = seq[arpStep_];
    arpStep_ = (arpStep_ + 1) % seqLen;

    if (trigger_.isGateOpen(voice)) continue;

    if (stepState == 2)  // OFF
    {
        // Force noteOff even if isTie suppressed the boundary cut
        if (arpActivePitch_ >= 0 && arpActiveCh_ > 0) { /* noteOff */ }
        continue;
    }
    if (isTie)
    {
        arpNoteOffRemaining_ = gateBeats;  // extend gate, no re-trigger
        continue;
    }
    // stepState == ON → existing note-on path unchanged
```

IMPORTANT: `arpLen` and 8 `arpStepState` values are read via `getRawParameterValue` inside
the `stepsToFire` loop. Since `stepsToFire` is at most 1–2 per block in practice, this is
not a performance concern, but consider hoisting these reads above the loop for clarity.

---

## Area 5: 3-State RND SYNC Button

### Reference: LFO SYNC button (lines 3258–3298)

The LFO SYNC button uses `setClickingTogglesState(true)` + `ButtonParameterAttachment`
because it's a two-state (bool) param. The new 3-state `randomSyncMode` requires manual
cycling since JUCE ButtonAttachment only handles bools.

**Pattern to use (matching randomSyncButton_ setup, lines 2477–2486, but manual cycling):**

```cpp
// Constructor setup:
randomSyncButton_.setButtonText("FREE");
randomSyncButton_.setName("round");
randomSyncButton_.setClickingTogglesState(false);   // manual cycling, NOT toggle
styleButton(randomSyncButton_);
addAndMakeVisible(randomSyncButton_);
// No ButtonParameterAttachment — drive manually via onClick

randomSyncButton_.onClick = [this]()
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(
            proc_.apvts.getParameter("randomSyncMode")))
    {
        const int next = (p->getIndex() + 1) % 3;  // 0=FREE → 1=INT → 2=DAW → 0=FREE
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next)));
        updateRndSyncButtonAppearance();
    }
};
```

**timerCallback — update label + colour:**

```cpp
void PluginEditor::updateRndSyncButtonAppearance()
{
    const int mode = static_cast<int>(
        *proc_.apvts.getRawParameterValue("randomSyncMode"));
    if (mode == 0)      // FREE
    {
        randomSyncButton_.setButtonText("FREE");
        randomSyncButton_.setColour(juce::TextButton::buttonColourId, Clr::gateOff);
        randomSyncButton_.setColour(juce::TextButton::buttonOnColourId, Clr::gateOff);
    }
    else if (mode == 1) // INT
    {
        randomSyncButton_.setButtonText("INT");
        randomSyncButton_.setColour(juce::TextButton::buttonColourId, Clr::gateOn);
        randomSyncButton_.setColour(juce::TextButton::buttonOnColourId, Clr::gateOn);
    }
    else                // DAW
    {
        randomSyncButton_.setButtonText("DAW");
        randomSyncButton_.setColour(juce::TextButton::buttonColourId,
                                    juce::Colours::cornflowerblue);
        randomSyncButton_.setColour(juce::TextButton::buttonOnColourId,
                                    juce::Colours::cornflowerblue);
    }
}
```

Call `updateRndSyncButtonAppearance()` in the constructor (after param registration),
in `onClick`, and in `timerCallback` (since the param can change from gamepad too).

### Removal of randomSyncButtonAtt_

The existing `std::unique_ptr<juce::ButtonParameterAttachment> randomSyncButtonAtt_`
(PluginEditor.h line 355) must be removed — its `ButtonParameterAttachment` will crash if
it tries to write to a non-bool param. Delete the declaration and the line
`randomSyncButtonAtt_ = std::make_unique<juce::ButtonParameterAttachment>(...)`.

### TriggerSystem ProcessParams update

Replace `bool randomClockSync` in TriggerSystem::ProcessParams with `int randomSyncMode`
(0=FREE, 1=INT, 2=DAW). Fill site in PluginProcessor.cpp line 1720:

```cpp
// Before:
tp.randomClockSync = (*apvts.getRawParameterValue("randomClockSync") > 0.5f);

// After:
tp.randomSyncMode = static_cast<int>(*apvts.getRawParameterValue("randomSyncMode"));
```

In TriggerSystem.cpp, replace `p.randomClockSync` with:
- `p.randomSyncMode >= 1` for "is any sync mode active" checks
- `p.randomSyncMode == 2` for "is DAW sync specifically active" checks
- `p.randomSyncMode == 0` for "is free mode" checks

The current code treats `randomClockSync=true` as "use DAW BPM and DAW grid".
After migration:
- FREE (0): Poisson timer, uses `p.randomFreeTempo` BPM regardless of DAW
- INT (1): Grid-locked timer, uses `p.randomFreeTempo` BPM (internal clock)
- DAW (2): Grid-locked timer, uses DAW BPM when playing

The effective BPM expression at TriggerSystem.cpp line 129:
```cpp
// Before:
const double effectiveBpm = (!p.randomClockSync || !p.isDawPlaying)
                                ? static_cast<double>(p.randomFreeTempo)
                                : p.bpm;
// After:
const double effectiveBpm = (p.randomSyncMode == 2 && p.isDawPlaying)
                                ? p.bpm
                                : static_cast<double>(p.randomFreeTempo);
```

The grid-gating at TriggerSystem.cpp line 136:
```cpp
// Before:
if (!p.randomClockSync)  // free mode: countdown timer
// After:
if (p.randomSyncMode == 0)  // FREE: countdown timer
```

---

## Area 6: Arp Box resized() — Exact Pixel Arithmetic

### Current layout (PluginEditor.cpp lines 4009–4023)

```cpp
auto r = arpBlockBounds_;             // 448 × 84, anchored to bottom-left
r.removeFromTop(14);                  // header clearance → 70px remaining
arpEnabledBtn_.setBounds(r.removeFromTop(22));  // ARP ON/OFF → 48px remaining
r.removeFromTop(4);                   // gap → 44px remaining
const int third = r.getWidth() / 3;  // = 149px
auto labelRow = r.removeFromTop(14); // label row → 30px remaining
arpSubdivLabel_.setBounds(labelRow.removeFromLeft(third));
arpOrderLabel_ .setBounds(labelRow.removeFromLeft(third));
arpGateTimeLabel_.setBounds(labelRow);
auto comboRow = r.removeFromTop(22); // combo/slider row → 8px remaining
arpSubdivBox_    .setBounds(comboRow.removeFromLeft(third).reduced(1, 0));
arpOrderBox_     .setBounds(comboRow.removeFromLeft(third).reduced(1, 0));
arpGateTimeKnob_.setBounds(comboRow.reduced(1, 0));
```

Remaining after current layout: 8px (= 84 - 14 - 22 - 4 - 14 - 22).

### New layout target

```
84px total
- 14px header clearance
= 70px
- 20px ARP ON button (was 22px → needs 2px trim to fit)
  OR keep 22px and adjust gaps
= 50px (if 20px button) OR 48px (if 22px button)
- 14px step row A (cells 0–3)
- 14px step row B (cells 4–7)
= 22px (if 20px) OR 20px (if 22px)
- 0px gap
- 20px controls row (4 combos/slider)
= 2px breathing room (if 20px button)
  OR 0px with tight fit (if 22px button, 20px controls)
```

With the ARP ON button at 22px (current), the pixel budget is:
14 + 22 + 14 + 14 + 20 + 0 = 84 exactly (no breathing room).

With the ARP ON button at 20px:
14 + 20 + 14 + 14 + 20 + 2 = 84 (2px breathing).

The CONTEXT.md pixel budget says "20px ARP ON/OFF button" — use 20px.

### New resized() code

```cpp
auto r = arpBlockBounds_;
r.removeFromTop(14);                           // 70px remaining
arpEnabledBtn_.setBounds(r.removeFromTop(20)); // 50px remaining
// step row A: cells 0–3
const auto stepRowA = r.removeFromTop(14);     // 36px remaining
// step row B: cells 4–7
const auto stepRowB = r.removeFromTop(14);     // 22px remaining
// controls row: 4 equal-width items
const int quarter = r.getWidth() / 4;          // 448/4 = 112px
auto ctrlRow = r.removeFromTop(20);
arpSubdivBox_.setBounds(ctrlRow.removeFromLeft(quarter).reduced(1, 0));
arpOrderBox_ .setBounds(ctrlRow.removeFromLeft(quarter).reduced(1, 0));
arpLengthBox_.setBounds(ctrlRow.removeFromLeft(quarter).reduced(1, 0));
arpGateTimeKnob_.setBounds(ctrlRow.reduced(1, 0));
// stepRowA and stepRowB bounds are stored for paint() and mouseDown() use
arpStepRowABounds_ = stepRowA;
arpStepRowBBounds_ = stepRowB;
```

Add `juce::Rectangle<int> arpStepRowABounds_, arpStepRowBBounds_` to PluginEditor.h (near
`arpBlockBounds_` at line 456).

### Control widths

- `arpBlockBounds_.getWidth() = 448` (verified: `left.getWidth() = kLeftColW = 448`)
- `quarter = 112`px per control
- All four controls get `.reduced(1, 0)` = 110px inner width (consistent with existing
  combo sizing pattern: `reduced(1, 0)` on each third in the current layout)

### Label rows removal

`arpSubdivLabel_`, `arpOrderLabel_`, `arpGateTimeLabel_` are set invisible (or
`setVisible(false)`) and removed from the layout. Do NOT delete their declarations yet —
the `mode1Labels[]` array in timerCallback (line 5638) references `arpSubdivLabel_` and
`arpOrderLabel_` for option-mode highlighting. Either keep them invisible and keep the
reference, or replace the references with the combo boxes themselves.

---

## Area 7: Paint Conditions for Step Grid

### Four paint states (from CONTEXT.md)

1. **Active + ON** (in range, stepState=0): `Clr::accent` filled square, full alpha
   Suggestion: use `Clr::gateOn` (bright green) for "ON" to match ARP button on-state.
   Using `Clr::highlight` (pink-red) also works for variation. Check visual with ARP panel.

2. **Active + TIE** (in range, stepState=1): dim fill + horizontal connecting bar
   Fill: `Clr::gateOn.darker(0.5f)`. Bar: draw a 2px horizontal line at cell vertical center,
   spanning to the next cell (full cell width including gap). Last cell in a row wraps to
   first cell of next row (or next row cell 0 if in row A, wrap within 8).

3. **Active + OFF** (in range, stepState=2): no fill, visible border only.
   Fill: `Clr::gateOff.darker(0.2f)`. Border: `Clr::textDim` at 0.5px.

4. **Inactive (beyond arpLength)**: render any state at 40% alpha.
   Use `g.saveState()` / `g.restoreState()` with `g.setOpacity(0.4f)` for the dim cells.
   Or pass alpha through colour multiplication: `fillColour.withAlpha(0.4f)`.

### Active step highlight

The existing `arpCurrentVoice_` atomic is read by `timerCallback` for pad highlighting.
Add `arpCurrentStep_` (or read from `arpStep_`) for grid highlighting the currently-playing
step. Since `arpStep_` is audio-thread-only, expose it via an atomic:

```cpp
// PluginProcessor.h — add alongside arpCurrentVoice_:
std::atomic<int> arpCurrentStep_ { 0 };
// updated each time a step fires: arpCurrentStep_.store(patStep, std::memory_order_relaxed);
```

Read in `timerCallback` → repaint `arpBlockBounds_` on change.

---

## Area 8: New Members Required in PluginEditor.h

```cpp
// ── Arpeggiator (Phase 45 additions) ──────────────────────────────────────────
juce::ComboBox   arpLengthBox_;          // "1".."8" combo
juce::Rectangle<int> arpStepRowABounds_; // cells 0–3 row bounds
juce::Rectangle<int> arpStepRowBBounds_; // cells 4–7 row bounds
std::unique_ptr<ComboAtt> arpLengthAtt_;
// arpStepState0..7: no UI components needed — drawn in paint(), clicked in mouseDown()
// randomSyncButtonAtt_ (line 355) — DELETE this, no longer needed
```

---

## New APVTS Params Summary

| Param ID | Type | Range / Options | Default | Registration location |
|----------|------|-----------------|---------|----------------------|
| `arpStepState0` | AudioParameterChoice | "ON"/"TIE"/"OFF" | ON (0) | After arpOrder in createParameterLayout() |
| `arpStepState1` | AudioParameterChoice | "ON"/"TIE"/"OFF" | ON (0) | same block |
| `arpStepState2` | AudioParameterChoice | "ON"/"TIE"/"OFF" | ON (0) | same |
| `arpStepState3` | AudioParameterChoice | "ON"/"TIE"/"OFF" | ON (0) | same |
| `arpStepState4` | AudioParameterChoice | "ON"/"TIE"/"OFF" | ON (0) | same |
| `arpStepState5` | AudioParameterChoice | "ON"/"TIE"/"OFF" | ON (0) | same |
| `arpStepState6` | AudioParameterChoice | "ON"/"TIE"/"OFF" | ON (0) | same |
| `arpStepState7` | AudioParameterChoice | "ON"/"TIE"/"OFF" | ON (0) | same |
| `arpLength` | AudioParameterChoice | "1"–"8" | "8" (index 7) | After arpStepState7 |
| `randomSyncMode` | AudioParameterChoice | "FREE"/"INT"/"DAW" | FREE (0) | Replace `randomClockSync` Bool |

`randomClockSync` Bool param registration (PluginProcessor.cpp lines 207–208) must be
**removed** and replaced with `randomSyncMode` Choice registration.

---

## Common Pitfalls

### Pitfall 1: TIE + step-boundary noteOff conflict

**What goes wrong:** The step-boundary noteOff at lines 1888–1900 fires before the step-state
check. TIE steps will have their sustained note cut before the TIE sustain logic runs.
**How to avoid:** Hoist the step-state read above the step-boundary noteOff block and
conditionally suppress the noteOff for TIE steps.
**Warning signs:** Audible gap between TIE cells (the note briefly cuts out).

### Pitfall 2: arpStep_ counter vs. patStep distinction

**What goes wrong:** The arpStep_ counter indexes `seq[]` (the order-permuted voice list,
up to 8 entries for Up+Down). The step pattern index should index the pattern grid (0–7)
relative to the active length. These are the same value for Up/Down orders (seqLen=4) but
diverge for Up+Down (seqLen=6) — step 5 in the order would map to patStep 4 in an 8-step
grid.
**How to avoid:** Always use `arpStep_ % arpLen` (BEFORE advancing arpStep_) as the
pattern index.

### Pitfall 3: randomSyncButtonAtt_ crash after param type change

**What goes wrong:** If `ButtonParameterAttachment` is left pointing to a param that is now
an AudioParameterChoice instead of AudioParameterBool, it will static_cast incorrectly
and produce undefined behavior or a crash.
**How to avoid:** Remove `randomSyncButtonAtt_` declaration and construction entirely before
registering `randomSyncMode`. Do not leave any orphaned attachment code.

### Pitfall 4: option-mode label highlight references

**What goes wrong:** `timerCallback` at line 5638 references `arpSubdivLabel_` in the
`mode1Labels[]` array. If the label is deleted, compilation breaks.
**How to avoid:** Keep the juce::Label members in PluginEditor.h even if invisible. Just
don't call `addAndMakeVisible` and don't `setBounds` them.

### Pitfall 5: arpLength combo index offset

**What goes wrong:** AudioParameterChoice index 0 = "1", index 7 = "8". But
`getRawParameterValue("arpLength")` returns 0..7, not 1..8. If the processBlock reads
the raw value without `+1` offset, all patterns are off by one length.
**How to avoid:** Always use `static_cast<int>(*apvts.getRawParameterValue("arpLength")) + 1`
for the effective length in beats arithmetic.

### Pitfall 6: State save for arpStepState after mouseDown

**What goes wrong:** Writing to AudioParameterChoice via `setValueNotifyingHost` in
mouseDown is correct and does trigger state updates, but it does NOT automatically cause
a `repaint()` of the cell grid.
**How to avoid:** Call `repaint(arpBlockBounds_)` in the mouseDown handler after writing
the param.

### Pitfall 7: Cells beyond arpLength — still clickable

**What goes wrong:** Graying out inactive cells must not make them non-interactive. If the
hit-test loop bails out on `patStep >= arpLen`, clicking a grayed cell does nothing.
**How to avoid:** The hit-test loop should run for all 8 cells regardless of arpLen.
The 40% alpha is visual-only.

---

## Code Examples

### Cycling a Choice param in mouseDown (canonical from ScaleKeyboard)

```cpp
// ScaleKeyboard::mouseDown (line 2109–2111) — writes to an AudioParameterBool:
if (auto* p = dynamic_cast<juce::AudioParameterBool*>(
        apvts_.getParameter("scaleNote" + juce::String(baseNote))))
    *p = !cur;

// Adapted for AudioParameterChoice cycle (3-state):
if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(
        proc_.apvts.getParameter("arpStepState" + juce::String(i))))
    p->setValueNotifyingHost(
        p->convertTo0to1(static_cast<float>((p->getIndex() + 1) % 3)));
```

### Reading a Choice param in processBlock (canonical pattern)

```cpp
// Current arpOrder read (line 1812):
const int orderIdx = juce::jlimit(0, 6,
    static_cast<int>(*apvts.getRawParameterValue(ParamID::arpOrder)));

// New arpLength read:
const int arpLen = juce::jlimit(1, 8,
    static_cast<int>(*apvts.getRawParameterValue("arpLength")) + 1);
```

### XML migration shim pattern (no existing example in codebase — new pattern)

```cpp
// In setStateInformation, before apvts.replaceState():
if (auto* oldParam = xml->getChildByAttribute("id", "randomClockSync"))
{
    const bool wasSync = (oldParam->getDoubleAttribute("value") > 0.5);
    xml->removeChildElement(oldParam, true);
    auto* newParam = xml->createNewChildElement("PARAM");
    newParam->setAttribute("id", "randomSyncMode");
    newParam->setAttribute("value", wasSync ? 1.0 : 0.0);
}
```

---

## Sources

### Primary (HIGH confidence)
- `/c/Users/Milosh Xhavid/get-shit-done/Source/PluginEditor.cpp` — full layout, paint, init
- `/c/Users/Milosh Xhavid/get-shit-done/Source/PluginEditor.h` — member declarations
- `/c/Users/Milosh Xhavid/get-shit-done/Source/PluginProcessor.cpp` — processBlock arp loop,
  APVTS registration, state persistence
- `/c/Users/Milosh Xhavid/get-shit-done/Source/PluginProcessor.h` — arp state fields
- `/c/Users/Milosh Xhavid/get-shit-done/Source/TriggerSystem.h` — ProcessParams struct
- `/c/Users/Milosh Xhavid/get-shit-done/Source/TriggerSystem.cpp` — randomClockSync branch sites

### No external sources required
All research derived directly from codebase inspection. JUCE API usage (AudioParameterChoice,
XmlElement::getChildByAttribute, removeChildElement) follows patterns already present in the
codebase.

---

## Metadata

**Confidence breakdown:**
- Step grid paint/hit-test: HIGH — ScaleKeyboard is a direct analogue
- APVTS Choice registration: HIGH — pattern used in 15+ places
- Bool-to-Choice migration: HIGH — setStateInformation inspected, shim pattern is standard XML manipulation
- Arp step loop integration: HIGH — loop inspected in full; TIE/noteOff interaction is a genuine pitfall but well-scoped
- Pixel arithmetic: HIGH — kLeftColW=448, kArpH=84 confirmed in source

**Research date:** 2026-03-08
**Valid until:** Stable code; valid until any of the 6 source files changes significantly
