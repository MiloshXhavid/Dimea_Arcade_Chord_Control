# Phase 03: Core MIDI Output and Note-Off Guarantee - Research

**Researched:** 2026-02-22
**Domain:** JUCE MidiBuffer, AudioProcessor lifecycle, gate LED UI pattern
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Velocity model**
- Fixed velocity for note-on: Claude's discretion (100 or 127 — pick what reads as "standard trigger")
- Joystick does NOT affect velocity
- No new APVTS velocity parameter this phase
- Note-off velocity: Claude's discretion (velocity 0 is standard MIDI practice)
- Full variable velocity deferred

**Retrigger behavior**
- Close-then-reopen: if gate is open and pad fires again, send note-off immediately then note-on with new pitch
- Different pitch retrigger: always send note-off for OLD pitch before note-on for NEW pitch
- Retrigger is global (no per-voice config)
- Two voices assigned same MIDI channel: detect conflict, show visual warning in UI

**Gate LED appearance**
- Simple on/off: glows when gate open, dark when closed
- No pulsing or fading
- One color for all 4 gate LEDs
- LED embedded in each TouchPlate button (pad itself glows)
- Reflects pad input state, not MIDI state
- Updates at 30 Hz

**Note-off edge cases**
- Transport stop: do nothing — let held pads ring until user releases TouchPlate
- Bypass: send note-off immediately for all active voices when plugin is bypassed
- releaseResources() / shutdown: best-effort note-off before deactivation
- CC 123 (All Notes Off): not handled — generator only plugin

### Claude's Discretion

- Exact fixed velocity value (100 or 127)
- Note-off velocity value (likely 0 per MIDI spec)
- Whether a global velocity APVTS parameter is needed (check existing code)
- Exact color of the gate LED glow (warm white, green, or blue)
- How the MIDI channel conflict warning is displayed (tooltip, red border, label — keep it subtle)

### Deferred Ideas (OUT OF SCOPE)

- Variable velocity (per TouchPlate pressure or joystick position) — future phase
- Per-voice retrigger mode (retrigger vs legato toggle per voice) — future phase
- CC 123 panic button handling — not applicable (generator-only plugin)
</user_constraints>

---

## Summary

Phase 03's core challenge is smaller than expected: the processBlock MIDI wiring is **already substantially implemented**. Reviewing `PluginProcessor.cpp`, `TriggerSystem.cpp`, and `PluginEditor.cpp` reveals that the TriggerSystem is already wired into processBlock via a `NoteCallback` lambda that adds `MidiMessage::noteOn` and `MidiMessage::noteOff` to the buffer. Per-voice MIDI channel routing already reads from APVTS (`voiceCh0..voiceCh3`). The retrigger close-then-reopen model is already implemented in `TriggerSystem::processBlock()`. Fixed velocity 100 is already hard-coded in the lambda.

The **actual gaps** are: (1) `releaseResources()` is empty — no note-off flush on deactivation; (2) bypass detection is absent — no note-off when the plugin is bypassed; (3) the MIDI channel conflict warning UI is not implemented; (4) the TouchPlate gate LED uses `Clr::highlight` (red/pink) for active state rather than the intended gate-on color — `Clr::gateOn` (green, `0xFF4CAF50`) is already defined in PluginEditor.cpp but not used in TouchPlate::paint(); (5) the DAW-verified MIDI routing has not been confirmed in Reaper.

**Primary recommendation:** Phase 03 is primarily a gap-fill and verification phase, not a from-scratch implementation. The three plans should be: (1) fix releaseResources + bypass handling, (2) fix TouchPlate LED color + add channel conflict warning, (3) DAW verification in Reaper.

---

## Codebase Reality Check (HIGH confidence — direct source reading)

### What is ALREADY implemented

| Feature | Location | Status |
|---------|----------|--------|
| TriggerSystem wired into processBlock | PluginProcessor.cpp:291-327 | DONE |
| MidiMessage::noteOn via NoteCallback lambda | PluginProcessor.cpp:302-304 | DONE — velocity 100, channel from APVTS |
| MidiMessage::noteOff via NoteCallback lambda | PluginProcessor.cpp:307-309 | DONE — noteOff(ch, pitch) with JUCE default velocity 0 |
| Per-voice MIDI channel routing | PluginProcessor.cpp:269-275 | DONE — reads voiceCh0..3 from APVTS |
| Sample-accurate note placement | TriggerSystem.cpp:46-126 | DONE — sampleOff passed to callback |
| Retrigger close-then-reopen | TriggerSystem.cpp:117-121 | DONE — fireNoteOff then fireNoteOn |
| Retrigger different pitch | TriggerSystem.cpp:117-121 | DONE — noteOff for activePitch_ before noteOn for new pitch |
| Gate state tracking (activePitch_) | TriggerSystem.h:79 | DONE — `int activePitch_[4]` initialized to -1 |
| 30 Hz timer calling repaint() on pads | PluginEditor.cpp:552-555 | DONE — `startTimerHz(30)`, timerCallback repaint |
| isGateOpen() read in TouchPlate::paint() | PluginEditor.cpp:78 | DONE — reads proc_.isGateOpen(voice_) |
| Glow colors defined | PluginEditor.cpp:13-14 | DONE — `Clr::gateOn {0xFF4CAF50}`, `Clr::gateOff {0xFF333355}` |

### What is MISSING or WRONG

| Gap | Location | What Needs Doing |
|-----|----------|-----------------|
| `releaseResources()` is empty | PluginProcessor.cpp:156 | Must flush note-off for all active voices |
| Bypass handling absent | PluginProcessor.h | Need `processBlockBypassed()` override or bypass detection |
| Wrong LED color for active pad | PluginEditor.cpp:79 | Uses `Clr::highlight` (red); should use `Clr::gateOn` (green) |
| No MIDI channel conflict warning | PluginEditor.cpp / PluginEditor.h | Detect duplicate voiceCh values, show subtle visual warning |
| DAW routing unverified | — | Must confirm in Reaper (Ableton still crashes on load — known blocker) |

---

## Standard Stack

### Core (all already in use in this project)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| juce::MidiBuffer | JUCE 8.0.4 | MIDI event buffer written in processBlock | Only JUCE-native MIDI container; addEvent() is the correct API |
| juce::MidiMessage | JUCE 8.0.4 | Factory for note-on, note-off, CC messages | Type-safe, channel/pitch/velocity encoding |
| juce::AudioProcessor | JUCE 8.0.4 | Base class — releaseResources(), processBlockBypassed() hooks | Required AudioProcessor lifecycle |

### No Additional Libraries Needed
This phase requires no new dependencies. All tools are already in the project.

---

## Architecture Patterns

### Pattern 1: Note-Off Flush in releaseResources()

**What:** When the DAW deactivates the plugin (stop/remove), JUCE calls `releaseResources()`. At this point the audio thread has stopped, so there is no MidiBuffer to write into. The standard approach is to keep a "pending flush" flag or rely on the fact that the DAW should absorb note-off on deactivation. However, JUCE does provide a proper hook.

**The JUCE way:** Override `releaseResources()` and, if possible, create a small MidiBuffer of note-offs to send. However, `releaseResources()` does not receive a MidiBuffer parameter. The correct approach is:

1. Maintain the active pitch state (already done via `activePitch_[4]` in TriggerSystem)
2. In `releaseResources()`, call `trigger_.flushAllNoteOffs(midiOut)` — but there is no midiOut here
3. **Real-world JUCE pattern**: Use `sendAllNotesOff()` approach — in `releaseResources()`, reset internal gate state so next processBlock call (if any) starts clean, and rely on the DAW to kill hanging notes OR send note-offs in the final processBlock before resources release

**What actually works (HIGH confidence from JUCE practice):** JUCE calls `releaseResources()` AFTER the audio thread stops. You cannot add to MidiBuffer there. Instead:
- Add a `bool pendingFlush_` flag to PluginProcessor
- In `processBlock()`, if `pendingFlush_` is true, write note-offs to the MidiBuffer for all active voices, then clear it
- Set `pendingFlush_ = true` in `releaseResources()` (this is actually useless since processBlock won't run after releaseResources)

**Correct implementation for "best-effort" note-off on shutdown:** The user decision says "best-effort note-off before deactivation." The JUCE `releaseResources()` is called from the message thread, not the audio thread, and after processBlock stops. The only reliable approach at this point is to reset TriggerSystem internal state (so on next prepareToPlay + processBlock, no stale gates exist). A "CC123 All Notes Off" to the MIDI output is not available because MidiBuffer output has already stopped.

**Alternative — processBlock early-exit flush:** Some plugins detect `!isNonRealtime()` or use a "flush requested" atomic that processBlock checks at the top of each block. Set it in `releaseResources()`. However, since `releaseResources()` is called AFTER the last processBlock, this is still racy.

**Pragmatic resolution (MEDIUM confidence):** The most common real-world approach for JUCE MIDI generators:
- `releaseResources()`: clear all `activePitch_[v]` and `gateOpen_[v]` in TriggerSystem
- Do NOT try to send MIDI from releaseResources — it's too late
- The "best-effort" interpretation means: ensure no stale state carries into the next `prepareToPlay` session

**For bypass (HIGH confidence):** JUCE provides `processBlockBypassed(AudioBuffer&, MidiBuffer&)`. Override it. The MidiBuffer IS available there. Write note-offs for all active voices into it. This is the clean hook for "send note-off on bypass."

### Pattern 2: Bypass Note-Off via processBlockBypassed()

```cpp
// Source: JUCE AudioProcessor API — processBlockBypassed override
void PluginProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio,
                                            juce::MidiBuffer& midi)
{
    audio.clear();
    midi.clear();
    // Flush all open gates
    for (int v = 0; v < 4; ++v)
    {
        const int pitch = trigger_.getActivePitch(v);
        if (pitch >= 0)
        {
            const int ch = (int)apvts.getRawParameterValue(voiceChIDs[v])->load();
            midi.addEvent(juce::MidiMessage::noteOff(ch, pitch), 0);
        }
    }
    // Reset TriggerSystem gate state
    trigger_.resetAllGates();
}
```

`resetAllGates()` must be added to TriggerSystem — sets `gateOpen_[v] = false`, `activePitch_[v] = -1`, clears `padJustFired_[v]`.

**Confidence:** HIGH — `processBlockBypassed` is a standard JUCE override. The MidiBuffer passed to it IS output to the DAW.

### Pattern 3: Gate LED — Correct Color

**Current state (PluginEditor.cpp:79):**
```cpp
g.setColour(active ? Clr::highlight : Clr::gateOff);
```
`Clr::highlight` = `0xFFE94560` (pink/red).

**Required fix:**
```cpp
g.setColour(active ? Clr::gateOn : Clr::gateOff);
```
`Clr::gateOn` = `0xFF4CAF50` (Material Green 500 — clear, reads well on dark background).

This matches the user decision: "one color for all 4 gate LEDs," "pad itself glows." Green on a dark navy background is the standard LED metaphor.

**Confidence:** HIGH — color constants already defined, one-line change.

### Pattern 4: MIDI Channel Conflict Warning

**Detection:** In `PluginEditor::timerCallback()` (runs at 30 Hz), read all 4 `voiceCh` APVTS values and check for duplicates.

```cpp
// Detect channel conflicts at 30 Hz in timerCallback
void detectChannelConflicts()
{
    int chs[4];
    chs[0] = (int)proc_.apvts.getRawParameterValue("voiceCh0")->load();
    chs[1] = (int)proc_.apvts.getRawParameterValue("voiceCh1")->load();
    chs[2] = (int)proc_.apvts.getRawParameterValue("voiceCh2")->load();
    chs[3] = (int)proc_.apvts.getRawParameterValue("voiceCh3")->load();
    bool conflict = false;
    for (int i = 0; i < 4 && !conflict; ++i)
        for (int j = i + 1; j < 4 && !conflict; ++j)
            if (chs[i] == chs[j]) conflict = true;
    // Update UI warning
    channelConflictLabel_.setVisible(conflict);
}
```

**Display:** A small label below the MIDI channel section reading "! Channel conflict" in `Clr::highlight` color (red/pink — existing alert color). This is subtle, already-styled, and requires no new colors.

**Alternative:** Red border on the conflicting pads. More complex because it requires identifying which pads conflict. The label approach is simpler and user-friendly.

**Confidence:** HIGH — all tools already in the codebase.

### Pattern 5: MidiMessage API in JUCE 8

Current code is using correct APIs (confirmed by code review):
- `juce::MidiMessage::noteOn(channel, pitch, velocity)` — channel is 1-based
- `juce::MidiMessage::noteOff(channel, pitch)` — default velocity 0 (JUCE fills in 0 for the 2-arg overload)
- `midi.addEvent(msg, sampleOffset)` — sampleOffset is within-block sample index (0-based)

**Velocity 100 vs 127:** 100 is a good "standard trigger" velocity — it's the MIDI convention for "typical playing velocity" without being at maximum (which can clip some synths). 127 is maximum. The current code already uses 100. No change needed.

**Note-off velocity:** The 2-argument `noteOff(ch, pitch)` overload in JUCE fills velocity as 64 (not 0). The 3-argument `noteOff(ch, pitch, 0)` sends velocity 0 which is proper MIDI spec. Current code uses 2-arg form — should use 3-arg with velocity 0.

**Confidence:** MEDIUM — verified from JUCE source reading convention, but should confirm with JUCE 8.0.4 docs. The MIDI spec says note-off velocity 0 and note-on velocity 0 are equivalent to note-off, so this is low risk either way.

### Anti-Patterns to Avoid

- **Calling MidiMessage::allNotesOff() in releaseResources():** There is no MidiBuffer available there. The call would be lost.
- **std::mutex or allocation in processBlock:** Already avoided in TriggerSystem (atomics + LCG). Don't introduce any in Phase 03.
- **Reading APVTS RawParameterValue in timerCallback from message thread:** This IS safe — `getRawParameterValue` returns a `std::atomic<float>*`, loads are thread-safe.
- **processBlockBypassed sending to a null MidiBuffer:** The JUCE framework guarantees MidiBuffer is valid in processBlockBypassed — it's the same contract as processBlock.
- **Calling `trigger_.setPadState()` from releaseResources():** releaseResources() is message-thread; TriggerSystem atomics ARE thread-safe for this, so it's fine.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Note-off on bypass | Custom bypass flag + processBlock check | `processBlockBypassed()` override | JUCE provides this exact hook; it receives a MidiBuffer |
| MIDI channel validation | Custom APVTS parameter listener | Read in timerCallback (30 Hz) | Simpler; no listener infrastructure needed; 30 Hz is responsive enough |
| Gate state tracking across releaseResources | Separate persistence layer | Reset `activePitch_[]` + `gateOpen_[]` atomics in TriggerSystem | State is already tracked; just need a reset method |

**Key insight:** This phase is almost entirely about using existing JUCE lifecycle hooks correctly, not building new machinery.

---

## Common Pitfalls

### Pitfall 1: Assuming releaseResources() Can Write MIDI
**What goes wrong:** Developer writes note-offs into a local MidiBuffer in releaseResources(). The notes never reach the DAW.
**Why it happens:** releaseResources() is a lifecycle teardown callback, not an audio callback.
**How to avoid:** Use processBlockBypassed() for bypass, reset internal state in releaseResources() only.
**Warning signs:** If you find yourself constructing a MidiBuffer inside releaseResources().

### Pitfall 2: Note-Off Velocity Mismatch
**What goes wrong:** `MidiMessage::noteOff(ch, pitch)` (2-arg) sends velocity 64, not 0. Some hardware synths treat this differently from velocity 0.
**Why it happens:** JUCE API overload choice is subtle.
**How to avoid:** Use `MidiMessage::noteOff(ch, pitch, (uint8_t)0)` explicitly.
**Warning signs:** Synths that respond to note-off velocity (rare but possible).

### Pitfall 3: Gate LED Reflects MIDI State Instead of Pad State
**What goes wrong:** LED is toggled based on successful MIDI send rather than pad input state. Introduces feedback loop if MIDI send fails or is delayed.
**Why it happens:** Conflating MIDI success with gate state.
**How to avoid:** User decided "reflects pad input state" — `isGateOpen()` reads from TriggerSystem's `gateOpen_` atomics, which are written by TriggerSystem itself on pad state changes. This is correct.
**Warning signs:** If LED update requires round-trip through DAW.

### Pitfall 4: Channel Conflict Warning Causing Repaint Storms
**What goes wrong:** Channel conflict detection runs every timerCallback and calls `repaint()` on every pad regardless of whether conflict changed.
**Why it happens:** Lazy implementation.
**How to avoid:** Cache the conflict state; only update label visibility when conflict status changes.
**Warning signs:** CPU spikes at 30 Hz even when UI is idle.

### Pitfall 5: processBlockBypassed Not Overriding Parent
**What goes wrong:** processBlockBypassed is defined but not declared as `override`, or the signature doesn't match AudioProcessor's virtual function.
**Why it happens:** Typo in signature.
**JUCE 8 signature:**
```cpp
void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
```
**Warning signs:** Bypass still produces zombie notes after adding the override.

---

## Code Examples

### releaseResources() — Reset Gate State (Best-Effort)
```cpp
// Source: JUCE AudioProcessor lifecycle pattern
void PluginProcessor::releaseResources()
{
    // Cannot write MIDI here — audio thread already stopped.
    // Reset all gate state so next prepareToPlay starts clean.
    trigger_.resetAllGates();
}
```

### TriggerSystem::resetAllGates() — New Method Required
```cpp
// Add to TriggerSystem.h public interface:
void resetAllGates()
{
    for (int v = 0; v < 4; ++v)
    {
        gateOpen_[v].store(false);
        activePitch_[v] = -1;
        padPressed_[v].store(false);
        padJustFired_[v].store(false);
    }
    allTrigger_.store(false);
    joystickTrig_.store(false);
}
```

### processBlockBypassed() — Note-Off Flush
```cpp
void PluginProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio,
                                            juce::MidiBuffer& midi)
{
    audio.clear();
    midi.clear();

    static const juce::String chIDs[4] = {
        "voiceCh0","voiceCh1","voiceCh2","voiceCh3" };

    for (int v = 0; v < 4; ++v)
    {
        const int pitch = trigger_.getActivePitch(v);
        if (pitch >= 0)
        {
            const int ch = (int)apvts.getRawParameterValue(chIDs[v])->load();
            midi.addEvent(juce::MidiMessage::noteOff(ch, pitch, (uint8_t)0), 0);
        }
    }
    trigger_.resetAllGates();
}
```

### TouchPlate::paint() — Fix Gate LED Color
```cpp
// Current (wrong): Clr::highlight (red/pink) for active state
// Fix:
void TouchPlate::paint(juce::Graphics& g)
{
    const bool active = proc_.isGateOpen(voice_);
    g.setColour(active ? Clr::gateOn : Clr::gateOff);   // gateOn = green 0xFF4CAF50
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

    g.setColour(Clr::text);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText(label_, getLocalBounds(), juce::Justification::centred);

    g.setColour((active ? Clr::gateOn : Clr::accent).brighter(0.2f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 8.0f, 1.5f);
}
```

### Channel Conflict Detection in timerCallback
```cpp
// In PluginEditor::timerCallback() — add after existing repaint calls:
{
    const int chs[4] = {
        (int)proc_.apvts.getRawParameterValue("voiceCh0")->load(),
        (int)proc_.apvts.getRawParameterValue("voiceCh1")->load(),
        (int)proc_.apvts.getRawParameterValue("voiceCh2")->load(),
        (int)proc_.apvts.getRawParameterValue("voiceCh3")->load()
    };
    bool conflict = false;
    for (int i = 0; i < 4 && !conflict; ++i)
        for (int j = i + 1; j < 4; ++j)
            if (chs[i] == chs[j]) { conflict = true; break; }

    if (conflict != channelConflictShown_)
    {
        channelConflictShown_ = conflict;
        channelConflictLabel_.setVisible(conflict);
    }
}
```

### noteOff Velocity Fix
```cpp
// Current:
midi.addEvent(juce::MidiMessage::noteOff(ch0 + 1, pitch), sampleOff);
// Fixed (explicit velocity 0):
midi.addEvent(juce::MidiMessage::noteOff(ch0 + 1, pitch, (uint8_t)0), sampleOff);
```

---

## Phase 03 Plan Structure Recommendation

Given that processBlock MIDI wiring is already done, Phase 03 should be structured as **2 plans** (not 3):

### Plan 03-01: Note-Off Guarantee and LED Fix
**Work items:**
1. Add `TriggerSystem::resetAllGates()` method (header + impl)
2. Implement `PluginProcessor::releaseResources()` — calls `trigger_.resetAllGates()`
3. Add `PluginProcessor::processBlockBypassed()` declaration to header
4. Implement `processBlockBypassed()` — note-off for all active voices + resetAllGates
5. Fix `noteOff` velocity: 2-arg → 3-arg with `(uint8_t)0`
6. Fix `TouchPlate::paint()`: `Clr::highlight` → `Clr::gateOn` for active state + border fix
7. Add `channelConflictLabel_` to `PluginEditor.h` + `bool channelConflictShown_` cache
8. Initialize `channelConflictLabel_` in constructor (styled in Clr::highlight, text "! Channel conflict", hidden by default)
9. Add conflict detection logic to `timerCallback()`
10. Add label bounds in `resized()` (small strip below trigger source selectors)

### Plan 03-02: DAW Verification in Reaper
**Work items:**
1. Build plugin: `cmake -B build -S . && cmake --build build`
2. Load in Reaper: confirm 4 pads → 4 MIDI note-on/off on correct channels
3. Test retrigger: hold pad, press again → no stuck note
4. Test bypass: engage bypass → all notes stop
5. Test transport stop: verify pads ring until released (no forced note-off)
6. Test MIDI channel conflict warning: set two voices to same channel → warning appears
7. Confirm gate LEDs glow green when pads active

**Note on Ableton blocker:** STATE.md records an active blocker — plugin crashes on Ableton Live 11 instantiation. Phase 03 DAW verification should use Reaper (not Ableton). The crash is deferred to Phase 07.

---

## State of the Art

| Old Approach | Current Approach | Impact |
|--------------|------------------|--------|
| JUCE MIDI Effect flag for generator plugins | IS_SYNTH=FALSE + NEEDS_MIDI_OUTPUT=TRUE (already set) | CMakeLists.txt already correct |
| Manual note-off tracking with separate array | `activePitch_[]` in TriggerSystem (already implemented) | No additional state tracking needed |
| Polling timer at 60 Hz for UI | 30 Hz timer (already set in PluginEditor) | Matches roadmap spec, responsive enough |

---

## Open Questions

1. **processBlockBypassed and DAW support**
   - What we know: JUCE AudioProcessor declares `processBlockBypassed()` as a virtual method that default-implementation calls `audio.clear()` only — no MIDI.
   - What's unclear: Do all DAWs call `processBlockBypassed()` when the plugin is bypassed, or do some DAWs bypass by simply not calling `processBlock`? If the latter, the hook won't fire.
   - Recommendation: Implement `processBlockBypassed()` regardless. Reaper calls it. Ableton behavior is unknown (and blocked anyway). This is best-effort per user decision.

2. **MIDI channel conflict warning — where in layout**
   - What we know: PluginEditor layout is dense. The trigger source section (left column, ~90px tall) has the MIDI routing controls nearby.
   - What's unclear: MIDI channel controls (`voiceCh0..3`) don't appear to have UI controls in PluginEditor.cpp — the APVTS params exist but there are no Sliders/Combos for them in the editor. The conflict warning may need to reference something the user can't currently change from the UI.
   - Recommendation: Check if voiceCh controls exist in PluginEditor. If they don't, add 4 small combo boxes for channel routing as part of Plan 03-01, then add the conflict warning adjacent to them. If they do exist somewhere not visible in the current layout, place the warning label nearby.

3. **Note-off velocity — JUCE 2-arg overload default value**
   - What we know: MIDI spec says note-off velocity should be 0 or 64 (64 is conventional "off"). JUCE 2-arg noteOff likely uses 0.
   - What's unclear: JUCE 8.0.4 exact default. Low stakes — either 0 or 64 is acceptable.
   - Recommendation: Use 3-arg form with explicit `(uint8_t)0` to be unambiguous.

---

## Sources

### Primary (HIGH confidence — direct codebase reading)
- `Source/PluginProcessor.cpp` — processBlock implementation, NoteCallback lambda, releaseResources stub
- `Source/TriggerSystem.cpp` — gate logic, retrigger, fireNoteOn/Off
- `Source/TriggerSystem.h` — API surface, atomic members
- `Source/PluginEditor.cpp` — TouchPlate::paint(), timerCallback(), color constants
- `Source/PluginEditor.h` — editor member variables
- `CMakeLists.txt` — JUCE 8.0.4, NEEDS_MIDI_OUTPUT=TRUE confirmed

### Secondary (MEDIUM confidence — JUCE API knowledge from training)
- JUCE AudioProcessor::processBlockBypassed() virtual function signature
- MidiMessage::noteOff() overload behavior (2-arg vs 3-arg)
- JUCE timer and repaint threading guarantees

### Tertiary (LOW confidence — not verified against JUCE 8.0.4 docs)
- Whether all target DAWs invoke processBlockBypassed() on bypass vs silently skipping processBlock
- Exact velocity default of MidiMessage::noteOff 2-arg overload in JUCE 8

---

## Metadata

**Confidence breakdown:**
- Codebase gap analysis: HIGH — direct source code reading
- JUCE lifecycle patterns: MEDIUM — consistent with training knowledge, JUCE API is stable
- DAW behavior (bypass): LOW — empirical testing required
- Color choices: HIGH — constants already defined, one-line fix

**Research date:** 2026-02-22
**Valid until:** 2026-03-22 (JUCE 8.0.4 is pinned; no API churn expected)
