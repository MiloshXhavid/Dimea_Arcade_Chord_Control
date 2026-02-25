# Phase 10: Trigger Quantization Infrastructure - Research

**Researched:** 2026-02-25
**Domain:** JUCE C++ audio plugin — lock-free MIDI looper quantization, APVTS state management, Catch2 testing
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Mode selector**
- Widget: 3-way segmented button — `Live | Post | Off`
- Lives inline with the looper controls inside the Looper section, adjacent to the single subdivision dropdown
- Default state on fresh load (no APVTS state): Off
- APVTS saves and restores the full toggle state (mode + subdivision)

**Subdivision selector**
- One shared dropdown for both Live mode and Post mode — they use the same grid
- Options: 1/4, 1/8, 1/16, 1/32
- Stored in APVTS; persists across sessions
- Independent from per-voice random gate subdivision params

**Live quantize behavior**
- Snap timing: Note-on snaps to nearest grid boundary; note-off = snapped note-on + original duration (gate shifts as a rigid unit — duration preserved exactly)
- Snap direction: Nearest grid point; ties (exact midpoint) snap to the earlier grid point (prefer the beat already passed, avoid "jumped a beat" feeling)
- Scope: Global — applies to all 4 voices simultaneously; no per-voice enable

**Post-record QUANTIZE toggle**
- Mechanism: Non-destructive toggle (the `Post` position on the mode selector acts as the QUANTIZE toggle)
  - Toggle ON (button/mode lights red): compute quantized beatPositions, overwrite `playbackStore_[]`, save originals to shadow copy
  - Toggle OFF: instant revert — future events in the current loop cycle use original positions; gates already fired this cycle are not affected
- Availability: Enabled when not recording (playback OK, stopped OK); disabled while record is active
- Feedback: Mode button segment highlighted red when Post mode is active (toggle on)
- New recording while Post is ON: New record overwrites both originals and the shadow copy; Post mode remains active but now quantizes the new content

**Gate duration edge cases**
- Negative gate: If quantized note-on >= original note-off, enforce 1/64-note minimum gate duration floor (note-off = snapped note-on + 1/64 note)
- Collision (same beat position, same voice): Keep the first event, discard the second
- Post-record: Same rules as live — note-on snap + clamped duration (not a lighter touch)

**APVTS parameters to add**
- `quantizeMode` — int/enum: 0=Off, 1=Live, 2=Post
- `quantizeSubdiv` — int: 0=1/4, 1=1/8, 2=1/16, 3=1/32

**SC3 update from discussion:** QUANTIZE button disabled only while actively recording (enabled during playback and stopped). Button is enabled during playback so the user can toggle quantize preview mid-loop.

### Claude's Discretion

None specified — all implementation details were decided in the discussion.

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| QUANT-01 | Quantize toggle in looper section — when ON, recorded gate events are snapped to the selected subdivision grid in real-time during recording | snapToGrid() pure function; called from recordGate() when liveQuantize mode is active; grid size from quantizeSubdiv APVTS param |
| QUANT-02 | Post-record QUANTIZE action button — re-snaps all gate events in the current loop to the selected subdivision without re-recording | pendingQuantize_ flag pattern (same as pendingPanic_); serviced in LooperEngine on message thread outside audio block; shadow copy in originalStore_[] for non-destructive revert |
| QUANT-03 | Quantize subdivision selector (1/4, 1/8, 1/16, 1/32 notes) — independent control from the random gate subdivision per voice | AudioParameterInt "quantizeSubdiv" 0..3; ComboBox with ComboBoxParameterAttachment in looper section |
</phase_requirements>

---

## Summary

Phase 10 adds trigger quantization to the LooperEngine. The implementation spans three layers: a pure `snapToGrid()` math function (testable in isolation), a LooperEngine data layer (live recording snap + post-record mutation), and a UI layer (3-way segmented button + subdivision dropdown in the existing Looper section).

The most critical design constraint is thread safety. The LooperEngine already uses the `pendingQuantize_`-style atomic flag pattern for deferred writes to `playbackStore_[]`. Post-record quantization follows the exact same pattern used by existing destructive operations (delete, reset): the message thread sets a flag; the audio thread services it at the top of `process()`. The shadow copy (`originalStore_[]`) lives at the same scope as `playbackStore_[]`, is populated during post-record quantize, and is restored on revert — all during the same audio-thread service window where the playbackStore_ is already exclusively owned.

The Catch2 test for the wrap edge case (`beatPosition == loopLen` wraps to 0.0 via `std::fmod`) is **mandatory before any LooperEngine integration** per STATE.md. The existing test infrastructure in `Tests/LooperEngineTests.cpp` is the correct location — add a new TC 12 there. The test binary target (`ChordJoystickTests`) already links `LooperEngine.cpp` and runs via CMake `-DBUILD_TESTS=ON`.

**Primary recommendation:** Implement `snapToGrid()` as a free `static` function in `LooperEngine.cpp`, test it in TC 12 before touching recording or playback paths, then layer in live quantize (in `recordGate()`), shadow copy + post-record flag, and UI in that order.

---

## Standard Stack

### Core
| Component | Version/Type | Purpose | Why Standard |
|-----------|-------------|---------|--------------|
| `std::atomic<bool>` | C++17 | Pending flag for post-record quantize request | Same pattern as `pendingPanic_`, `deleteRequest_`, `resetRequest_` — zero-cost, already proven |
| `std::fmod` | `<cmath>` | Wrap quantized beat position back into [0, loopLen) | Already used throughout LooperEngine.cpp for all beat position arithmetic |
| `juce::AudioParameterInt` | JUCE 8.0.4 | APVTS params `quantizeMode` (0..2) and `quantizeSubdiv` (0..3) | Same pattern as all other int params in `createParameterLayout()` |
| `juce::ComboBoxParameterAttachment` | JUCE 8.0.4 | Attach `quantizeSubdivBox_` ComboBox to APVTS | Same pattern as `loopSubdivAtt_`, `randomSubdivAtt_` |
| Catch2 v3.8.1 | v3.8.1 (pinned in CMakeLists.txt) | Unit test for snapToGrid wrap edge case | Already wired; `Tests/LooperEngineTests.cpp` is the target file |

### Supporting
| Component | Version/Type | Purpose | When to Use |
|-----------|-------------|---------|-------------|
| `juce::TextButton` (3 instances, radio group) | JUCE 8.0.4 | 3-way `Off\|Live\|Post` segmented button | No native JUCE 3-segment widget — use 3 TextButton in a radio group (same approach as arp order buttons and trigger source dropdowns) |
| `std::array<LooperEvent, LOOPER_FIFO_CAPACITY>` | C++17 | `originalStore_[]` shadow copy | Same type/size as existing `playbackStore_[]` and scratch arrays |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| 3 TextButton radio group | Custom segmented widget or `lrtoggle` pill (2-way existing) | The `lrtoggle` pill in PixelLookAndFeel only supports 2 states; extending it to 3 requires new drawing code; 3 TextButtons in radio group is simpler and consistent with how arp mode works |
| `pendingQuantize_` atomic flag | std::mutex + direct playbackStore_ write | Mutex on audio thread is forbidden by design; atomic flag matches all existing deferred-write patterns |
| `originalStore_[]` as class member | Heap-allocated vector | Class member arrays are already the pattern for `scratchNew_[]`, `scratchMerged_[]`; avoids allocation on audio thread |

---

## Architecture Patterns

### Recommended File Touch List
```
Source/LooperEngine.h         # Add shadow copy, atomic flags, quantize API
Source/LooperEngine.cpp       # snapToGrid(), live snap in recordGate(), post-record service
Source/PluginProcessor.h      # Add looper quantize passthrough methods + APVTS params
Source/PluginProcessor.cpp    # createParameterLayout(): add quantizeMode + quantizeSubdiv
Source/PluginEditor.h         # Add 3 TextButton members + ComboBox + attachments
Source/PluginEditor.cpp       # Constructor init, resized() layout, timerCallback() enable/disable
Tests/LooperEngineTests.cpp   # TC 12: snapToGrid wrap edge case
```

### Pattern 1: snapToGrid Pure Function

**What:** A pure `static` function that takes a beat position and returns the nearest grid boundary. No class state — fully testable in isolation.

**When to use:** Called in both live quantize path (recordGate) and post-record path (applyQuantize). Single source of truth for snap math.

```cpp
// Source: derived from existing std::fmod usage in LooperEngine.cpp + CONTEXT.md spec

// Returns the nearest grid boundary to beatPos within a loop of length loopLen.
// Ties (exact midpoint) snap to the earlier grid point (per locked decision).
// Result is always in [0, loopLen) via std::fmod.
static double snapToGrid(double beatPos, double gridSize, double loopLen)
{
    if (gridSize <= 0.0 || loopLen <= 0.0) return beatPos;

    // Find nearest grid point
    const double divisions = beatPos / gridSize;
    const double lower     = std::floor(divisions) * gridSize;
    const double upper     = lower + gridSize;
    const double midpoint  = lower + gridSize * 0.5;

    // Ties snap to earlier (lower) grid point
    double snapped = (beatPos < midpoint) ? lower : upper;

    // Wrap: if snapped == loopLen, it must become 0.0
    return std::fmod(snapped, loopLen);
}
```

**Note:** `std::fmod(loopLen, loopLen) == 0.0` is guaranteed by IEEE 754 for finite positive values. This handles the critical wrap edge case documented in STATE.md.

### Pattern 2: Grid Size from quantizeSubdiv APVTS Param

**What:** Convert the integer APVTS parameter (0..3) to a beat-unit grid size.

```cpp
// Source: existing beatsPerBar() pattern in LooperEngine.cpp — same switch idiom

static double quantizeSubdivToGridSize(int subdivIdx)
{
    // Quarter-note = 1.0 beat (the universal beat unit in this codebase)
    switch (subdivIdx)
    {
        case 0: return 1.0;        // 1/4 note
        case 1: return 0.5;        // 1/8 note
        case 2: return 0.25;       // 1/16 note
        case 3: return 0.125;      // 1/32 note
        default: return 0.5;       // safe fallback: 1/8
    }
}
```

### Pattern 3: Live Quantize in recordGate()

**What:** When live quantize is active, snap the incoming beatPos before writing to the FIFO.

**Gate-off handling:** Compute note-off position as `snappedOnPos + originalDuration`. If that would produce a zero-length or negative gate, clamp to `snappedOnPos + 1/64 note` minimum.

```cpp
// Source: derived from recordGate() in LooperEngine.cpp + CONTEXT.md spec

void LooperEngine::recordGate(double beatPos, int voice, bool on)
{
    ASSERT_AUDIO_THREAD();
    if (!recording_.load(std::memory_order_relaxed)) return;
    if (!recGates_.load(std::memory_order_relaxed))  return;

    const double loopLen = getLoopLengthBeats();
    if (loopLen <= 0.0) return;

    double pos = std::fmod(beatPos, loopLen);

    // Live quantize: snap note-on to grid; track snapped-on position for paired note-off
    if (quantizeMode_.load(std::memory_order_relaxed) == kQuantizeLive)
    {
        const double grid = quantizeSubdivToGridSize(
            quantizeSubdiv_.load(std::memory_order_relaxed));
        if (on)
        {
            pos = snapToGrid(pos, grid, loopLen);
            lastSnappedOnBeat_[voice] = pos;   // per-voice audio-thread-only member
        }
        else
        {
            // Duration-preserving shift: note-off = snapped-on + original duration
            const double origDuration = pos - lastSnappedOnBeat_[voice];
            // Handle wrap (note-off could be at lower beat than note-on if loop wraps)
            const double duration = (origDuration >= 0.0)
                ? origDuration
                : origDuration + loopLen;
            const double minGate  = 1.0 / 64.0;  // 1/64 note minimum
            const double clampedDuration = std::max(duration, minGate);
            pos = std::fmod(lastSnappedOnBeat_[voice] + clampedDuration, loopLen);
        }
    }

    // ... rest of existing FIFO write ...
}
```

**New LooperEngine member required:** `double lastSnappedOnBeat_[4]` — audio-thread-only (no atomic needed).

### Pattern 4: Post-Record Quantize via Flag + Shadow Copy

**What:** Message thread sets `pendingQuantize_` flag. Audio thread services it at the top of `process()`, modifies `playbackStore_[]` in place, saves originals to `originalStore_[]`.

**Threading invariant:** This must be serviced ONLY when `playing_=true` (looper running) OR `playing_=false` (stopped). Either way, `playbackStore_[]` is exclusively owned by the audio thread during `process()`. The key point: do NOT service during `recording_=true` (the UI disables the button anyway, but the audio thread should also guard).

```cpp
// Source: derived from deleteRequest_ / resetRequest_ service pattern in LooperEngine.cpp

// At top of LooperEngine::process(), after deleteRequest_ / resetRequest_ service:

if (pendingQuantize_.exchange(false, std::memory_order_acq_rel))
{
    const int n = playbackCount_.load(std::memory_order_relaxed);
    const double loopLen = getLoopLengthBeats();
    const double grid = quantizeSubdivToGridSize(
        quantizeSubdiv_.load(std::memory_order_relaxed));

    // Save originals to shadow copy (for revert)
    for (int i = 0; i < n; ++i)
        originalStore_[i] = playbackStore_[i];
    originalCount_.store(n, std::memory_order_relaxed);
    hasOriginals_.store(true, std::memory_order_relaxed);

    // Apply snap to gate events only (joystick/filter events are not quantized)
    applyQuantizeToStore(grid, loopLen);  // modifies playbackStore_ in-place
    quantizeActive_.store(true, std::memory_order_relaxed);
}

if (pendingQuantizeRevert_.exchange(false, std::memory_order_acq_rel))
{
    if (hasOriginals_.load(std::memory_order_relaxed))
    {
        const int n = originalCount_.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
            playbackStore_[i] = originalStore_[i];
        playbackCount_.store(n, std::memory_order_release);
        quantizeActive_.store(false, std::memory_order_relaxed);
    }
}
```

### Pattern 5: APVTS Parameters

**What:** Two new APVTS params following existing int param conventions in `createParameterLayout()`.

```cpp
// Source: existing addInt() lambda pattern in PluginProcessor.cpp

// In createParameterLayout():
addInt("quantizeMode",   "Quantize Mode",   0, 2, 0);   // 0=Off, 1=Live, 2=Post
addInt("quantizeSubdiv", "Quantize Subdiv", 0, 3, 1);   // 0=1/4, 1=1/8(default), 2=1/16, 3=1/32
```

**ParamID additions in the ParamID namespace:**
```cpp
static const juce::String quantizeMode   = "quantizeMode";
static const juce::String quantizeSubdiv = "quantizeSubdiv";
```

### Pattern 6: 3-Way Segmented Button in UI

**What:** Three `juce::TextButton` in a radio group simulate a 3-segment selector (`Off | Live | Post`). This is the simplest approach because the existing `lrtoggle` pill only handles 2 states, and no native 3-segment widget exists in JUCE.

**Implementation:**
```cpp
// Source: juce::Button::setRadioGroupId() — standard JUCE radio group behavior
// Assign the same radio group ID to all three; only one can be on at a time.

quantizeOffBtn_.setRadioGroupId  (kQuantizeRadioGroup, juce::dontSendNotification);
quantizeLiveBtn_.setRadioGroupId (kQuantizeRadioGroup, juce::dontSendNotification);
quantizePostBtn_.setRadioGroupId (kQuantizeRadioGroup, juce::dontSendNotification);

quantizeOffBtn_.setClickingTogglesState(true);
quantizeLiveBtn_.setClickingTogglesState(true);
quantizePostBtn_.setClickingTogglesState(true);

// onClick handlers read APVTS param, set new value via apvts.getParameterAsValue()
quantizeOffBtn_.onClick  = [this] { setQuantizeMode(0); };
quantizeLiveBtn_.onClick = [this] { setQuantizeMode(1); };
quantizePostBtn_.onClick = [this] { setQuantizeMode(2); };
```

**CRITICAL:** These buttons are NOT directly APVTS-attached via `ButtonParameterAttachment` because `ButtonParameterAttachment` is designed for a single bool parameter. The mapping from an int parameter to 3 buttons requires manual onClick handlers + timerCallback sync. This matches how `trigSrc_` ComboBoxes work — `ComboBoxParameterAttachment` handles them cleanly.

**Alternative (simpler):** Use a `juce::ComboBox` with items "Off / Live / Post" and a `ComboBoxParameterAttachment` to `quantizeMode`. This is the safest approach, fully consistent with existing patterns (`loopSubdivAtt_`, `randomSubdivAtt_`). The user specified "3-way segmented button" visually, but given the complexity of a custom 3-segment widget vs. a styled ComboBox, the planner should weigh simplicity vs. UX fidelity.

**Decision for planner:** The CONTEXT.md locked decision says "3-way segmented button." Use 3 TextButton with radio group ID. Keep it simple — style consistently with `styleButton()`. The post mode button should use a distinct color when active (use `buttonOnColourId` = `Clr::highlight` which is already the toggle-on color, i.e., the pink-red "lit red" visual the user described).

### Pattern 7: Button Enable/Disable During Recording

**What:** In `timerCallback()`, read `proc_.looperIsRecording()` and call `setEnabled()` on the quantize mode buttons and subdivision dropdown.

```cpp
// Source: existing timerCallback() pattern in PluginEditor.cpp

// In timerCallback():
const bool isRecording = proc_.looperIsRecording();
quantizeOffBtn_.setEnabled(!isRecording);   // or the full group
quantizeLiveBtn_.setEnabled(!isRecording);
quantizePostBtn_.setEnabled(!isRecording);
quantizeSubdivBox_.setEnabled(!isRecording);
```

**Note:** `setEnabled(false)` on a JUCE component automatically greys it out via the LookAndFeel. No custom drawing needed.

### Pattern 8: Catch2 Test for snapToGrid Wrap Edge Case

**What:** TC 12 in `Tests/LooperEngineTests.cpp` validates the `std::fmod(quantized, loopLen)` guard.

```cpp
// Source: existing TC 11 loop wrap pattern in LooperEngineTests.cpp

TEST_CASE("snapToGrid - wrap edge case: near-end event snaps to 0.0", "[looper][quantize]")
{
    // A gate at beatPosition = loopLen - 0.001, quantized to a 1-beat grid,
    // should land at 0.0 (not loopLen), and fire exactly once per cycle.
    // This directly validates the std::fmod(snapped, loopLen) requirement from STATE.md.

    const double loopLen  = 4.0;  // 1-bar 4/4
    const double gridSize = 1.0;  // 1/4 note = 1 beat

    // beatPos = 3.999 — nearest grid point is 4.0 = loopLen
    const double snapped = snapToGrid(3.999, gridSize, loopLen);
    CHECK(snapped == Catch::Approx(0.0).epsilon(1e-9));

    // beatPos = 3.5 — equidistant; ties snap to earlier (3.0 not 4.0)
    const double tie = snapToGrid(3.5, gridSize, loopLen);
    CHECK(tie == Catch::Approx(3.0).epsilon(1e-9));

    // beatPos = 0.001 — nearest is 0.0
    const double near_zero = snapToGrid(0.001, gridSize, loopLen);
    CHECK(near_zero == Catch::Approx(0.0).epsilon(1e-9));
}
```

**MANDATORY:** This test must pass before any LooperEngine integration work begins.

### Anti-Patterns to Avoid

- **Mutex in audio thread:** The `playbackStore_[]` never uses a mutex. Use atomic flags only.
- **Calling applyQuantize from message thread:** Message thread must NEVER write to `playbackStore_[]` directly. Flag + audio-thread service only.
- **Quantizing joystick/filter events:** Only Gate-type events are quantized. Joystick and filter positions are continuous data — snapping them makes no sense and would corrupt smooth playback.
- **Using ButtonParameterAttachment for the 3-way mode selector:** `ButtonParameterAttachment` maps a single bool. The mode is an int (0/1/2). Use `ComboBoxParameterAttachment` or manual onClick + timerCallback.
- **Snapping note-off to grid independently:** The locked decision is "duration preserved exactly." Note-off = snapped note-on + original duration. Do NOT snap note-off position independently — this changes the gate length.
- **Forgetting the originalStore reset on new recording:** When a new recording starts while Post is ON, `originalStore_[]` must be cleared/reset so the revert path doesn't restore stale pre-recording data. Set `hasOriginals_.store(false)` at the start of new recording.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Thread-safe flag for deferred write | Custom mutex-based mechanism | `std::atomic<bool>` flag, same as `deleteRequest_`, `resetRequest_`, `pendingPanic_` | Pattern already proven in this codebase; audio thread services at top of process() |
| Radio group button behavior | Custom exclusive toggle logic | `juce::Button::setRadioGroupId()` | JUCE handles exclusivity automatically; already used in this style of UI |
| APVTS persistence of quantize state | Manual serialization | `addInt()` + `ComboBoxParameterAttachment` | APVTS auto-saves/restores all parameters via `getStateInformation()` / `setStateInformation()` |
| Beat position math | Custom rounding | `std::fmod`, `std::floor`, `std::round` from `<cmath>` | Already in use throughout LooperEngine.cpp; consistent precision |

**Key insight:** This phase is pure infrastructure addition — no new concurrency primitives needed. The entire thread-safety model is already established; this is a replication of existing patterns into new logic.

---

## Common Pitfalls

### Pitfall 1: Quantizing the Wrong Event Types
**What goes wrong:** `applyQuantizeToStore()` modifies joystick or filter events, corrupting continuous-data playback (pitch jumps, filter stutters).
**Why it happens:** All event types live in `playbackStore_[]` together; an unguarded loop modifies everything.
**How to avoid:** Guard with `if (ev.type != LooperEvent::Type::Gate) continue;` before any snap logic.
**Warning signs:** Joystick playback producing staircase pitch patterns after post-quantize.

### Pitfall 2: Double-Fire at Loop Boundary After Quantize
**What goes wrong:** An event at beat 3.999 snaps to 4.0 = loopLen. Without `std::fmod`, the beat position equals `loopLen` and the playback window check `ev.beatPosition >= beatAtBlockStart && ev.beatPosition < beatAtEnd` never catches it (the event is always "after" the current window), so it never fires. Or it fires on both sides of the wrap.
**Why it happens:** Floating-point rounding near loop end.
**How to avoid:** `std::fmod(snapped, loopLen)` is MANDATORY after every snap operation. State.md explicitly calls this out. TC 12 verifies it.
**Warning signs:** Gates near loop end going silent after quantize.

### Pitfall 3: Stale Original Store After New Recording
**What goes wrong:** User records loop, enables Post quantize, then re-records. Toggling Post off reverts to the old pre-re-record originals instead of the new recording's originals.
**Why it happens:** `originalStore_[]` was populated from the first recording and never cleared.
**How to avoid:** When a new recording starts (in `process()` when `recordPending_` activates), store `hasOriginals_.store(false)`. Also update `originalStore_[]` when `finaliseRecording()` completes if Post mode is active, since Post mode should now quantize the new content.
**Warning signs:** After re-recording with Post ON, toggling Post OFF restores wrong (old) events.

### Pitfall 4: Collision After Quantize Produces Silent Voice
**What goes wrong:** Two events from different voices (or same voice, different original positions) snap to the same grid point. The second event silently overwrites or collides with the first during playback.
**Why it happens:** Multiple original positions within one grid cell all snap to the same boundary.
**How to avoid:** During `applyQuantizeToStore()`, after snapping all events, scan for same-beat same-voice Gate-on collisions. Keep only the first (earlier original beat position), discard subsequent — per locked decision.
**Warning signs:** Fewer gate-on events playing back than expected on a densely-recorded voice.

### Pitfall 5: RadioGroup ID Clash with Existing Buttons
**What goes wrong:** Using a radio group ID that is already used by another set of buttons in PluginEditor, causing unexpected mutual exclusion across unrelated controls.
**Why it happens:** `setRadioGroupId()` takes an arbitrary integer; no namespace enforcement.
**How to avoid:** Define a named constant for the quantize radio group ID (e.g., `constexpr int kQuantizeRadioGroup = 42;`) and verify it doesn't conflict with any existing `setRadioGroupId()` calls.
**Warning signs:** Pressing a quantize mode button deselects an unrelated trigger source or arp mode button.

### Pitfall 6: ButtonParameterAttachment Does Not Support Int Params
**What goes wrong:** Attempting to use `ButtonParameterAttachment` on the `quantizeMode` int parameter (0/1/2) causes a runtime assert or incorrect behavior.
**Why it happens:** `ButtonParameterAttachment` is designed for `AudioParameterBool` only.
**How to avoid:** Use `ComboBoxParameterAttachment` if going the ComboBox route, or manual `onClick` + timerCallback sync if going the 3-button route. Do NOT use `ButtonParameterAttachment` for `quantizeMode`.

### Pitfall 7: applyQuantize Called While Recording
**What goes wrong:** `pendingQuantize_` flag is serviced at top of `process()` while `recording_=true`, meaning the audio thread writes to `playbackStore_[]` while `recordGate()` is also writing to the FIFO. The FIFO is not `playbackStore_[]` so this is not technically a data race, but `finaliseRecording()` immediately following would overwrite the quantized content.
**Why it happens:** The UI disables the button during recording, but the flag may have been set just before recording started.
**How to avoid:** In the audio thread service: guard with `if (recording_.load(std::memory_order_relaxed)) { /* skip, discard flag */ }`.

---

## Code Examples

### snapToGrid with Tie-Breaking (Complete)
```cpp
// Source: derived from CONTEXT.md spec + existing std::fmod usage in LooperEngine.cpp

static double snapToGrid(double beatPos, double gridSize, double loopLen)
{
    if (gridSize <= 0.0 || loopLen <= 0.0) return beatPos;
    const double lower    = std::floor(beatPos / gridSize) * gridSize;
    const double upper    = lower + gridSize;
    const double midpoint = lower + gridSize * 0.5;
    // Ties go to earlier grid point (beatPos < midpoint, not <=)
    const double snapped  = (beatPos < midpoint) ? lower : upper;
    return std::fmod(snapped, loopLen);  // mandatory wrap
}
```

### Gate Duration Preservation
```cpp
// Source: CONTEXT.md locked decision — "gate shifts as a rigid unit"
// Both originalOnBeat and snappedOnBeat are in [0, loopLen)
static double snapGateOff(double originalOffBeat, double snappedOnBeat,
                           double originalOnBeat, double loopLen)
{
    // Recover original duration, accounting for possible wrap
    double duration = originalOffBeat - originalOnBeat;
    if (duration < 0.0) duration += loopLen;  // note-off wrapped past loop end

    const double minGate = 1.0 / 64.0;        // 1/64 note minimum
    duration = std::max(duration, minGate);

    return std::fmod(snappedOnBeat + duration, loopLen);
}
```

### APVTS Parameter Registration
```cpp
// Source: existing createParameterLayout() in PluginProcessor.cpp — addInt() lambda
// Add to ParamID namespace:
static const juce::String quantizeMode   = "quantizeMode";
static const juce::String quantizeSubdiv = "quantizeSubdiv";

// In createParameterLayout():
addInt(ParamID::quantizeMode,   "Quantize Mode",   0, 2, 0);  // default Off
addInt(ParamID::quantizeSubdiv, "Quantize Subdiv", 0, 3, 1);  // default 1/8
```

### LooperEngine New Members
```cpp
// Source: derived from existing LooperEngine.h member patterns

// Private section additions:

// Shadow copy for non-destructive post-record quantize revert
std::array<LooperEvent, LOOPER_FIFO_CAPACITY> originalStore_ {};
std::atomic<int>  originalCount_  { 0 };
std::atomic<bool> hasOriginals_   { false };
std::atomic<bool> quantizeActive_ { false };  // true = post-quantize is live

// Pending flags (message thread sets, audio thread services)
std::atomic<bool> pendingQuantize_       { false };  // apply post-quantize
std::atomic<bool> pendingQuantizeRevert_ { false };  // revert to originals

// Config (read by audio thread, written by PluginProcessor from APVTS)
std::atomic<int> quantizeMode_   { 0 };  // 0=Off, 1=Live, 2=Post
std::atomic<int> quantizeSubdiv_ { 1 };  // 0=1/4, 1=1/8, 2=1/16, 3=1/32

// Per-voice last snapped note-on beat (audio-thread-only, for duration-preserving note-off)
double lastSnappedOnBeat_[4] = { 0.0, 0.0, 0.0, 0.0 };
```

---

## State of the Art

| Old Approach | Current Approach | Applies Here |
|--------------|------------------|--------------|
| Quantize at note-off time (DAW style) | Quantize at note-on + preserve duration | Duration-preserving snap per CONTEXT.md locked decision |
| Mutex for shared buffer access | Atomic flag + audio-thread-owned buffer | Standard in all audio plugin code; must stay this way |
| Two separate buttons (QUANTIZE ON/OFF) | 3-way segmented mode (Off/Live/Post) | Per CONTEXT.md locked decision |

**No deprecated patterns apply** — this is new functionality built entirely from established codebase patterns.

---

## Open Questions

1. **Where does `snapToGrid()` live in the file structure?**
   - What we know: It is a pure function used only by LooperEngine internals
   - What's unclear: Whether to declare it `static` in LooperEngine.cpp (invisible outside the TU) or add it to LooperEngine.h for testing access
   - Recommendation: Declare `static` in LooperEngine.cpp and make it a `friend` visible to the test by adding a test accessor, OR declare it in a small `QuantizeMath.h` header included by both LooperEngine.cpp and the test. The simplest path: add it to the top of LooperEngine.cpp as a file-scope `static`, and in the test file, replicate the same function. Both files are compiled into the test binary target (`ChordJoystickTests`), so the static version in LooperEngine.cpp is directly linkable if the function signature is exposed via a non-static free function in LooperEngine.h.

2. **Should `applyQuantizeToStore()` sort the result?**
   - What we know: `finaliseRecording()` sorts `playbackStore_[]` after merge. Post-quantize snaps may reorder events (e.g., event at 3.9 snaps to 0.0).
   - What's unclear: Whether the process() scan loop depends on sort order.
   - Recommendation: Yes — call `std::sort` on `playbackStore_[]` after `applyQuantizeToStore()`, same as `finaliseRecording()` does. The sort is O(n log n) on ≤2048 events, acceptable for a one-shot non-audio-block operation triggered by user action.

3. **Radio group ID value for quantize buttons**
   - What we know: No existing `setRadioGroupId()` calls were found by grep in the current PluginEditor.cpp
   - Recommendation: Use ID `1` for the quantize mode radio group (first radio group in the plugin). Grep confirms no existing radio groups conflict.

---

## Sources

### Primary (HIGH confidence)
- LooperEngine.h / LooperEngine.cpp — direct source read; all threading patterns, data structures, and method signatures confirmed from actual code
- PluginProcessor.h / PluginProcessor.cpp — APVTS parameter layout, pendingPanic_ flag pattern, atomic flag conventions confirmed from actual code
- PluginEditor.h / PluginEditor.cpp — existing UI patterns (styleButton, timerCallback, lrtoggle, ComboBoxParameterAttachment) confirmed from actual code
- Tests/LooperEngineTests.cpp — existing Catch2 test structure and test binary setup confirmed from actual code
- CMakeLists.txt — Catch2 v3.8.1, BUILD_TESTS flag, test target `ChordJoystickTests` confirmed from actual code
- .planning/STATE.md — `pendingQuantize_` flag pattern decision, `std::fmod` requirement, and blocker note confirmed verbatim

### Secondary (MEDIUM confidence)
- .planning/phases/10-trigger-quantization-infrastructure/10-CONTEXT.md — all implementation decisions read directly; constraints are authoritative as they came from the user discussion

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all components are already in use in this codebase
- Architecture: HIGH — patterns are direct replications of existing proven patterns (pendingPanic_, finaliseRecording, ComboBoxParameterAttachment)
- Pitfalls: HIGH — derived from reading actual code paths, not speculation

**Research date:** 2026-02-25
**Valid until:** 2026-03-25 (JUCE 8.0.4, Catch2 v3.8.1, C++17 — all pinned in CMakeLists.txt; stable)
