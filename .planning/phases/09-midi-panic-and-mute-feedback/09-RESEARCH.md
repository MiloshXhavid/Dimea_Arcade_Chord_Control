# Phase 09: MIDI Panic and Mute Feedback - Research

**Researched:** 2026-02-25
**Domain:** JUCE MIDI output (C++17), JUCE UI timer pattern, JUCE TextButton styling
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Button placement**
- Lives in the **Gamepad section**, on the right side of the gamepad status label row
- Layout: `PS4 Connected         [ PANIC! ]`
- The status label and button share the same row (status label left, button right)

**Button appearance**
- Label: **"PANIC!"** (with exclamation mark)
- Width: ~60px — same compact size as other small buttons (RST, DEL, etc.)
- Idle color: Claude's discretion — pick what fits the existing palette best

**Flash feedback**
- Duration: **~167ms** (5 frames × 30 Hz timer) — same as RST/DEL flash pattern
- Flash color: **Clr::highlight** — consistent with existing button feedback
- Implementation: consumes existing `flashPanic_` atomic counter in timerCallback, same pattern as `flashLoopReset_` / `flashLoopDelete_`

**Technical contract (from domain section)**
- Panic emits exactly 48 events: CC64=0, CC120=0, CC123 (allNotesOff) × 16 channels
- Zero CC121 events
- Plugin resumes normal MIDI output immediately after panic fires
- Flash animation reuses the existing 30 Hz editor timer (no second timer added)

### Claude's Discretion
- Idle button color (accent, dim, or a subtle warning tint — whatever looks cleanest)
- Exact pixel layout within the gamepad row (left-aligned status label, right-aligned button, or proportioned as fits)

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PANIC-01 | Panic sends full MIDI reset sequence: CC64=0, CC120 (All Sound Off), CC123 (All Notes Off) — no CC121 (corrupts downstream VST3 instrument parameters) | Processor already emits CC64=0 + CC120 + CC123 per active channel. Change: iterate all 16 channels unconditionally instead of deduplicated voice+filter channels. See Architecture Patterns §Processor panic sweep. |
| PANIC-02 | Panic sweeps all 16 MIDI channels (not just the 4 configured voice channels) to clear stuck notes regardless of routing | Current implementation calls `killCh()` only for voiceChs[0..3] + filterMidiCh (max 5 channels, deduplicated). Replace with a simple `for (int ch = 1; ch <= 16; ++ch)` loop — no sent[] dedup guard needed since each channel appears exactly once. |
| PANIC-03 | Panic button shows brief flash feedback on press (using existing flashPanic_ counter pattern) — no persistent mute state | `flashPanic_` atomic already incremented by processor. Editor timerCallback already handles flashLoopReset_ and flashLoopDelete_ with identical counter pattern. Gap: panicBtn_ not yet declared in header; timerCallback not yet consuming flashPanic_. |
</phase_requirements>

---

## Summary

Phase 09 is a narrow, well-constrained change with two independent work areas: (1) expand the processor's panic sweep from 4-5 channels to all 16, and (2) add a UI PANIC! button wired to the existing infrastructure.

The processor side is nearly complete: `pendingPanic_`, `triggerPanic()`, and `flashPanic_` already exist. The `killCh()` lambda in `processBlock` already does CC64=0 + CC120 + CC123 correctly, which satisfies PANIC-01. The only processor change needed is to replace the current deduplication loop (which covers only voiceChs[0..3] + filterMidiCh) with a flat `for (int ch = 1; ch <= 16; ++ch)` loop, satisfying PANIC-02. The sent[] guard and filterMidiCh lookup can be deleted.

The editor side requires three changes: (a) declare `juce::TextButton panicBtn_` and `int panicFlashCounter_` in PluginEditor.h, (b) wire the button in the constructor with `onClick` calling `proc_.triggerPanic()`, (c) add a layout slot in `resized()` and flash-counter logic in `timerCallback()` following the identical pattern used for `loopResetBtn_` / `loopDeleteBtn_`. The R3 gamepad button (`consumeRightStickTrigger()`) should be re-wired in processBlock to call `triggerPanic()` rather than being silently consumed.

**Primary recommendation:** Implement as two tasks — Task 1: processor sweep expansion (processor.cpp only, 8-line change); Task 2: editor button + flash wiring (editor.h + editor.cpp, following RST/DEL pattern exactly).

---

## Standard Stack

### Core
| Component | Version | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| JUCE MidiMessage::controllerEvent() | JUCE 8.0.4 | Emit CC64, CC120 per channel | Already used in killCh() lambda |
| JUCE MidiMessage::allNotesOff() | JUCE 8.0.4 | Emit CC123 per channel | Already used in killCh() lambda |
| JUCE TextButton | JUCE 8.0.4 | UI button with setColour() for flash | Same component as RST, DEL, all looper buttons |
| std::atomic<int> flashPanic_ | C++17 | Signal processor→UI for flash | Already declared in PluginProcessor.h line 118 |

### Supporting
| Component | Version | Purpose | When to Use |
|-----------|---------|---------|-------------|
| Clr::highlight (0xFFFF3D6E) | project palette | Flash color | Locked decision — use for panic flash, identical to RST/DEL |
| Clr::accent | project palette | Idle button color | Recommendation: use for idle state (matches other utility buttons) |
| styleButton() helper | PluginEditor.cpp | One-call button style init | Call in constructor after setting button text |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| for loop ch=1..16 | current killCh() with sent[] guard | sent[] was needed when channels could overlap; flat loop is simpler and guarantees exactly 48 events |
| Clr::accent idle | Clr::gateOff (dark red) | gateOff would imply a toggle state; accent is neutral/utility — better for a one-shot action |

---

## Architecture Patterns

### Recommended Project Structure

No new files. All changes are in:
```
Source/
├── PluginProcessor.cpp    # expand panic sweep to ch 1..16
├── PluginEditor.h         # add panicBtn_, panicFlashCounter_
└── PluginEditor.cpp       # constructor wiring + resized() layout + timerCallback()
```

### Pattern 1: Processor Panic Sweep (existing, to be expanded)

**What:** pendingPanic_ flag set by UI/gamepad on message thread, consumed atomically in processBlock audio thread.
**When to use:** All destructive MIDI-clear actions.

Current code (lines 537–563 of PluginProcessor.cpp):
```cpp
// CURRENT — sweeps only voiceChs + filterMidiCh (max 5 channels)
if (pendingPanic_.exchange(false, std::memory_order_acq_rel))
{
    if (looper_.isPlaying()) looper_.startStop();
    {
        const int fCh = (int)apvts.getRawParameterValue(ParamID::filterMidiCh)->load();
        bool sent[17] = {};
        auto killCh = [&](int ch)
        {
            if (ch < 1 || ch > 16 || sent[ch]) return;
            sent[ch] = true;
            midi.addEvent(juce::MidiMessage::controllerEvent(ch, 64,  0), 0);
            midi.addEvent(juce::MidiMessage::controllerEvent(ch, 120, 0), 0);
            midi.addEvent(juce::MidiMessage::allNotesOff(ch),             0);
        };
        for (int v = 0; v < 4; ++v) killCh(voiceChs[v]);
        killCh(fCh);
    }
    trigger_.resetAllGates();
    looperActivePitch_.fill(-1);
    prevCcCut_.store(-1, std::memory_order_relaxed);
    prevCcRes_.store(-1, std::memory_order_relaxed);
    flashPanic_.fetch_add(1, std::memory_order_relaxed);
}
```

**Target code (Phase 09 change — replace the inner braced block):**
```cpp
// PHASE 09 TARGET — sweeps all 16 channels unconditionally (48 events)
if (pendingPanic_.exchange(false, std::memory_order_acq_rel))
{
    if (looper_.isPlaying()) looper_.startStop();
    for (int ch = 1; ch <= 16; ++ch)
    {
        midi.addEvent(juce::MidiMessage::controllerEvent(ch, 64,  0), 0);  // sustain off
        midi.addEvent(juce::MidiMessage::controllerEvent(ch, 120, 0), 0);  // all sound off
        midi.addEvent(juce::MidiMessage::allNotesOff(ch),             0);  // CC123
    }
    trigger_.resetAllGates();
    looperActivePitch_.fill(-1);
    prevCcCut_.store(-1, std::memory_order_relaxed);
    prevCcRes_.store(-1, std::memory_order_relaxed);
    flashPanic_.fetch_add(1, std::memory_order_relaxed);
}
```

Event count: 16 channels × 3 events = **48 events exactly**. No CC121.

### Pattern 2: Flash Counter in timerCallback (existing, replicate for panic)

**What:** Atomic int incremented on audio thread, consumed (exchange 0) on message thread. Non-zero means "start flash"; decremented each 30 Hz frame until 0.
**When to use:** All gamepad button confirmation feedback.

Existing pattern (PluginEditor.cpp lines 1800–1827) — replicate exactly:
```cpp
// In timerCallback(), AFTER flashLoopDelete_ block:
if (proc_.flashPanic_.exchange(0, std::memory_order_relaxed) > 0)
    panicFlashCounter_ = 5;   // ~167ms at 30Hz

if (panicFlashCounter_ > 0)
{
    --panicFlashCounter_;
    panicBtn_.setColour(juce::TextButton::buttonColourId,  Clr::highlight);
    panicBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}
else
{
    panicBtn_.setColour(juce::TextButton::buttonColourId,  Clr::accent);
    panicBtn_.setColour(juce::TextButton::textColourOffId, Clr::text);
}
```

### Pattern 3: Button Layout in Gamepad Row (existing row to be modified)

**What:** The gamepad status row is laid out in `resized()` at lines 1396–1406. Current layout:
```
[GAMEPAD ON/OFF 90px] [4px] [FILTER MOD ON/OFF 90px] [4px] [REC FILTER 58px] [4px] [status label — remainder]
```

**Phase 09 target layout:**
```
[GAMEPAD ON/OFF 90px] [4px] [FILTER MOD ON/OFF 90px] [4px] [REC FILTER 58px] [4px] [status label — grows] [4px] [PANIC! 60px]
```

Implementation in `resized()` (replace the `gamepadStatusLabel_.setBounds(row)` line):
```cpp
// Gamepad status row: [GAMEPAD ON/OFF] [FILTER MOD ON/OFF] [REC FILTER] [status label] [PANIC!]
{
    auto row = right.removeFromTop(20);
    gamepadActiveBtn_.setBounds(row.removeFromLeft(90));
    row.removeFromLeft(4);
    filterModBtn_.setBounds(row.removeFromLeft(90));
    row.removeFromLeft(4);
    filterRecBtn_.setBounds(row.removeFromLeft(58));
    row.removeFromLeft(4);
    panicBtn_.setBounds(row.removeFromRight(60));  // right-aligned, 60px wide
    row.removeFromRight(4);
    gamepadStatusLabel_.setBounds(row);            // remainder to status label
}
```

### Pattern 4: R3 Gamepad Wiring (processBlock, currently silently consumed)

**What:** `consumeRightStickTrigger()` is currently called in processBlock line 411 and its return value discarded. Phase 09 wires it to `triggerPanic()`.

Current (PluginProcessor.cpp line 411):
```cpp
gamepad_.consumeRightStickTrigger();  // R3 — unassigned, consume to prevent accumulation
```

Phase 09 target:
```cpp
if (gamepad_.consumeRightStickTrigger())
{
    midiMuted_.store(false, std::memory_order_relaxed);
    pendingPanic_.store(true, std::memory_order_acq_rel);
}
```

Note: `triggerPanic()` is a public method that does exactly this. Since this runs on the audio thread, call the atomic stores inline (same as `triggerPanic()` body) rather than calling the public method, to make the thread context explicit. Alternatively, the processBlock code can just call `triggerPanic()` directly — it only touches atomics, which is safe from any thread.

### Anti-Patterns to Avoid

- **CC121 (Reset All Controllers):** Explicitly forbidden. CC121 causes VST3 wrappers (Kontakt, Waves CLA-76) to reset instrument parameters via JUCE's CC→parameter mapping. Never add CC121 to the panic sweep.
- **Persistent mute toggle:** `midiMuted_` must remain false after panic fires. `triggerPanic()` already stores false before setting `pendingPanic_`. Do not add any muted state to the panicBtn_.
- **Second timer:** Do not call `startTimerHz()` again. The existing 30 Hz timer is the single timer. The pre-submission success criterion requires exactly 1 result for `grep startTimerHz PluginEditor.cpp`.
- **Separate sent[] dedup in the new loop:** The new `for (ch = 1..16)` loop visits each channel exactly once; no dedup guard is needed.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Flash animation timer | A second juce::Timer subclass or a timer ID | Existing 30 Hz timerCallback + counter int | Project constraint: exactly 1 startTimerHz call. Counter pattern already proven with RST/DEL. |
| MIDI all-notes-off | Custom note-tracking and note-off sweep | `juce::MidiMessage::allNotesOff(ch)` + CC120 | JUCE wraps CC123; CC120 cuts sound even on sustain-held notes |
| Button toggle state | setClickingTogglesState(true) | Do NOT set clicking-toggles-state | Panic is a one-shot action, not a toggle. No setClickingTogglesState call. |

**Key insight:** The infrastructure for this phase already exists. This is a wiring and expansion task, not a design task.

---

## Common Pitfalls

### Pitfall 1: CC121 Inclusion
**What goes wrong:** Adding CC121 (Reset All Controllers) to the panic sweep corrupts downstream VST3 instrument parameters.
**Why it happens:** "Reset All Controllers" sounds like the right thing to send in a full panic; MIDI spec says so. But VST3 wrapper maps CC121 to plugin parameter resets.
**How to avoid:** The sweep uses only CC64=0, CC120, CC123. CC121 is explicitly excluded. Verify the final event count is exactly 48 (16 × 3), not 64 (16 × 4).
**Warning signs:** Any `controllerEvent(ch, 121, ...)` in the panic block.

### Pitfall 2: midiMuted_ Left True
**What goes wrong:** If `midiMuted_` is left true after panic, all further MIDI output is blocked permanently.
**Why it happens:** The existing `triggerPanic()` calls `midiMuted_.store(false)` before `pendingPanic_.store(true)`. But if Phase 09 adds button wiring that accidentally sets muted=true, MIDI output stops.
**How to avoid:** Do not add any `midiMuted_.store(true)` logic. The panicBtn_ onClick must call only `proc_.triggerPanic()`. No toggle state on the button.
**Warning signs:** After pressing PANIC!, no MIDI output on next pad touch.

### Pitfall 3: Double startTimerHz Call
**What goes wrong:** If a second `startTimerHz()` call is added (e.g., for animation), the editor runs two timers, breaking the project's single-timer invariant.
**Why it happens:** Developer thinks flash needs its own timer.
**How to avoid:** The flash counter in timerCallback is sufficient. Verify with `grep startTimerHz PluginEditor.cpp` — must return exactly 1 match.
**Warning signs:** grep returns 2 results; timerCallback fires at wrong rate.

### Pitfall 4: panicBtn_ Missing from addAndMakeVisible
**What goes wrong:** Button is declared and wired but never appears in the UI.
**Why it happens:** Forgot `addAndMakeVisible(panicBtn_)` in the constructor.
**How to avoid:** Follow the exact same constructor sequence used for gamepadActiveBtn_ and filterModBtn_: setText → setTooltip → onClick → styleButton → addAndMakeVisible.

### Pitfall 5: Wrong Event Count Due to Leftover voiceChs Loop
**What goes wrong:** If the old `for (int v = 0; v < 4; ++v) killCh(voiceChs[v])` loop is kept alongside the new ch=1..16 loop, channels 1–4 get double-killed (6 events each instead of 3). Event count becomes 48 + extra.
**How to avoid:** Replace the entire inner braced block, including fCh lookup and sent[] array and killCh lambda. The new loop is a clean replacement.
**Warning signs:** MIDI monitor shows >48 events; duplicate CCs on channels 1-4.

---

## Code Examples

Verified patterns from source inspection (PluginEditor.cpp + PluginProcessor.cpp):

### Constructor Wiring for panicBtn_ (model: gamepadActiveBtn_, lines 1162–1178)
```cpp
// In PluginEditor constructor — model after gamepadActiveBtn_ pattern:
panicBtn_.setButtonText("PANIC!");
panicBtn_.setTooltip("Send All Notes Off on all 16 MIDI channels");
// No setClickingTogglesState — panic is one-shot, not a toggle
styleButton(panicBtn_, Clr::accent);   // idle: accent color (Claude's discretion)
panicBtn_.onClick = [this]
{
    proc_.triggerPanic();
    // flashPanic_ will be incremented by the processor's pendingPanic_ handler;
    // timerCallback will pick it up and start the flash counter.
};
addAndMakeVisible(panicBtn_);
```

### PluginEditor.h Additions
```cpp
// In the "Flash counters" block (after arpBlinkCounter_):
int panicFlashCounter_ = 0;

// In the looper section or gamepad section of member declarations:
juce::TextButton panicBtn_;   // [PANIC!] — one-shot MIDI all-notes-off
```

### timerCallback Flash Block Addition (after deleteFlashCounter_ block)
```cpp
if (proc_.flashPanic_.exchange(0, std::memory_order_relaxed) > 0)
    panicFlashCounter_ = 5;   // ~167ms at 30Hz

if (panicFlashCounter_ > 0)
{
    --panicFlashCounter_;
    panicBtn_.setColour(juce::TextButton::buttonColourId,  Clr::highlight);
    panicBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}
else
{
    panicBtn_.setColour(juce::TextButton::buttonColourId,  Clr::accent);
    panicBtn_.setColour(juce::TextButton::textColourOffId, Clr::text);
}
```

### R3 Wiring in processBlock (replace line 411)
```cpp
// Before (line 411):
gamepad_.consumeRightStickTrigger();  // R3 — unassigned, consume to prevent accumulation

// After:
if (gamepad_.consumeRightStickTrigger())
    triggerPanic();   // R3 → MIDI panic (same as UI panicBtn_)
```

Note: `triggerPanic()` is a public method on PluginProcessor. Called from processBlock (audio thread) — safe because it only writes atomics.

---

## State of the Art

| Old Approach | Current Approach | Impact |
|--------------|------------------|--------|
| Panic sweeps voiceChs[0..3] + filterMidiCh (≤5 ch) | Phase 09: sweep all 16 channels | Clears stuck notes on any routing configuration |
| R3 silently consumed (line 411) | Phase 09: R3 calls triggerPanic() | Gamepad can trigger panic without touching the UI |
| flashPanic_ incremented but never consumed in editor | Phase 09: consumed in timerCallback | Visual feedback for gamepad-triggered panic |

**No deprecated APIs involved.** All JUCE calls (`controllerEvent`, `allNotesOff`, `TextButton`, `setColour`) are stable JUCE 8 API.

---

## Open Questions

1. **panicBtn_ member ordering in PluginEditor.h**
   - What we know: No strict ordering rule; existing members are grouped by function section
   - What's unclear: Whether it belongs under "looper" section or a new "gamepad section"
   - Recommendation: Add under the looper section comment block near gamepadActiveBtn_ and filterModBtn_, labeled `// [PANIC!] one-shot MIDI panic button`

2. **STATE.md mentions "timerCallback R3 sync untouched (08-01) — Lines 1484-1502"**
   - What we know: Lines 1484-1502 in current PluginEditor.cpp are in `resized()`, not `timerCallback()`. No R3-related timerCallback code exists.
   - What's unclear: This note may refer to a now-deleted panicBtn_ toggle that was synced to `isMidiMuted()` in an earlier plan. That code is gone; no reference to panicBtn_ exists anywhere in the current editor.
   - Recommendation: The note describes code that no longer exists. Phase 09 introduces panicBtn_ fresh, with no toggle-sync behavior. `isMidiMuted()` integration is NOT needed for Phase 09.

---

## Sources

### Primary (HIGH confidence)
- Direct source inspection: `Source/PluginProcessor.h` — confirms `flashPanic_`, `pendingPanic_`, `triggerPanic()` already exist (lines 116-125)
- Direct source inspection: `Source/PluginProcessor.cpp` lines 535–563 — confirms current panic sweep covers voiceChs[0..3] + filterMidiCh only
- Direct source inspection: `Source/PluginEditor.cpp` lines 1799–1827 — flash counter pattern for RST/DEL confirmed exact
- Direct source inspection: `Source/PluginEditor.cpp` line 1296 — `startTimerHz(30)` is the only timer call
- Direct source inspection: `Source/PluginEditor.h` — confirms `panicBtn_` not yet declared anywhere
- Direct source inspection: `Source/GamepadInput.h` line 28 — confirms R3 = MIDI panic intent already documented

### Secondary (MEDIUM confidence)
- JUCE 8 MIDI spec: `MidiMessage::allNotesOff(ch)` sends CC123; `controllerEvent(ch, 120, 0)` sends CC120 (All Sound Off). Both are standard MIDI 1.0 GM spec.

### Tertiary (LOW confidence)
- None — all claims verified from source code directly.

---

## Metadata

**Confidence breakdown:**
- Processor change: HIGH — current code is fully visible; change is mechanical (replace inner block)
- Editor wiring: HIGH — pattern is replicated from existing RST/DEL flash which is fully visible
- Button layout: HIGH — resized() gamepad row is fully visible; removeFromRight(60) pattern is standard
- R3 wiring: HIGH — GamepadInput.h confirms consumeRightStickTrigger() exists; processBlock wiring confirmed

**Research date:** 2026-02-25
**Valid until:** N/A — research is against local source code, not external dependencies
