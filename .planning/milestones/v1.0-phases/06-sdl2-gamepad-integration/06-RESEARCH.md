# Phase 06: SDL2 Gamepad Integration - Research

**Researched:** 2026-02-23
**Domain:** SDL2 2.30.9 game controller API, C++17 JUCE VST3 plugin threading, MIDI CC gating
**Confidence:** HIGH for SDL2 facts; MEDIUM for multi-instance singleton safety; HIGH for existing code analysis

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Axis Dead Zones**
- Reuse existing `joystickThreshold` APVTS param for the right stick dead zone — no new param or UI knob needed
- Shape: Square/per-axis (each axis independently ignored below threshold) — consistent with the existing Chebyshev distance approach already in the codebase
- Center return behavior: Hold last pitch (sample-and-hold) — when stick returns within dead zone, pitch stays at last reported position; consistent with the existing joystick mouse model
- Button debounce: 20ms debounce on gamepad buttons to prevent double-triggers on noisy hardware

**UI Connection Indicator**
- Small text label at the bottom of PluginEditor (status-bar style, bottom edge)
- Label text: `"GAMEPAD: connected"` when active, `"GAMEPAD: none"` when absent
- Hot-plug: Silent — just start working; label updates, no other UI change or notification
- Disconnect: Label updates to `"GAMEPAD: none"` — no other notification

**Multi-Controller Behavior**
- Controller selection: First controller found at SDL_Init is used — stable, no UI selector needed
- On disconnect: Silently go to "no gamepad" (label updates to `"GAMEPAD: none"`)
- Stuck notes on disconnect: Send all-notes-off on disconnect to prevent stuck MIDI notes
- Multi-instance support: SDL singleton shared across instances (reference-counted SDL_Init/SDL_Quit as already scoped in ROADMAP)
- Per-instance GAMEPAD ACTIVE toggle: Add a toggle button (e.g. `[GAMEPAD ON]`/`[GAMEPAD OFF]`) in PluginEditor so users can deactivate gamepad input on a specific instance when multiple ChordJoystick instances are open — only the instance with GAMEPAD ON active reads the controller

**CC Value-Change Threshold**
- Gate strategy: Both dead zone AND value-change dedup — dead zone suppresses center wobble; dedup prevents redundant CC when stick moves slowly (only emit when mapped 0–127 integer value changes by ≥1)
- Dead zone for CC axis: Reuse same `joystickThreshold` param — one threshold param controls both right and left stick
- CC output range: Attenuated by existing `filterXAtten`/`filterYAtten` APVTS params (already wired for this purpose)
- On disconnect: Send CC reset to value 0 on both CC74 and CC71 — synth filter returns to closed/minimum position cleanly

### Claude's Discretion

- Exact SDL event polling vs callback design for hot-plug detection
- Internal SDL singleton implementation details (static instance, atomic ref-count, etc.)
- GAMEPAD ACTIVE toggle placement within the PluginEditor bottom section (near the status label)

### Deferred Ideas (OUT OF SCOPE)

- None — discussion stayed within phase scope
</user_constraints>

---

## Summary

This phase fixes SDL lifecycle bugs and wires gamepad input end-to-end. Most of the heavy lifting is **already written**: `GamepadInput.h/.cpp` implements the 60 Hz timer, axis normalization, edge detection, hot-plug via `SDL_CONTROLLERDEVICEADDED/REMOVED`, and the CC path. `PluginProcessor.cpp` already consumes voice triggers, looper buttons, and the filter CC path. What is missing is: the SDL process-level singleton (currently each `GamepadInput` instance calls `SDL_Init`/`SDL_Quit` independently — crashes with two plugin instances), the per-axis dead zone using `joystickThreshold`, the CC dedup, the 20ms button debounce, and the `[GAMEPAD ON/OFF]` toggle button in the editor.

The critical SDL fact verified by official sources: `SDL_HINT_JOYSTICK_THREAD "1"` must be set **before** `SDL_Init` and tells SDL2 on Windows to run raw input message handling on a dedicated internal thread rather than requiring a Win32 message pump on the calling thread. Without this hint, SDL2 DAW plugins can deadlock or crash because plugin threads have no Win32 message loop. The existing `GamepadInput.cpp` already sets this hint correctly.

The SDL video subsystem is **intentionally compiled out** in CMakeLists.txt (`SDL_VIDEO OFF`). Per verified SDL2 issue #5380, the workaround for receiving raw input without `SDL_INIT_VIDEO` is exactly `SDL_HINT_JOYSTICK_THREAD "1"` — which is already in place. This is confirmed as the correct fix merged in SDL 2.0.22. The build configuration is sound.

**Primary recommendation:** Convert `GamepadInput` from owning SDL_Init/Quit to using a shared `SdlContext` singleton (process-level atomic ref-count), add per-axis dead zone using `joystickThreshold`, add CC dedup with `prevCcCut_`/`prevCcRes_` fields in `PluginProcessor`, add 20ms debounce timestamps in `GamepadInput`, and add `[GAMEPAD ON/OFF]` toggle + updated status label text.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| SDL2 | 2.30.9 (static) | Gamepad polling, hot-plug events | Already in CMakeLists.txt via FetchContent; `SDL2-static` already linked |
| JUCE | 8.0.4 | Timer, MidiMessage, APVTS, UI | Project standard; already linked |
| C++17 std::atomic | C++17 | Lock-free state sharing across Timer/audio threads | CXX_STANDARD 17 already set |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| SDL_GetTicks() | SDL2 built-in | Millisecond timestamps for 20ms debounce | In timerCallback() for button state tracking |
| juce::MessageManager::callAsync | JUCE built-in | Marshal SDL connection-change callbacks to message thread | When SDL background thread fires onConnectionChange |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| SDL2 static | XInput (Windows only) | SDL2 already in build; cross-controller; no change needed |
| Per-axis square dead zone | Radial dead zone | User decided square/per-axis — matches existing joystick model |
| std::atomic ref-count | std::mutex + counter | Atomic is lock-free; mutex forbidden on audio path |

**Installation:** Already in CMakeLists.txt. No changes to build system required.

---

## Architecture Patterns

### Existing Code Structure (what's already there)

```
Source/
├── GamepadInput.h/.cpp    — SDL2 60Hz Timer, atomics for axes/triggers, hot-plug
├── PluginProcessor.h/.cpp — consumes gamepad_, joystickX/Y, filter CC, APVTS
├── PluginEditor.h/.cpp    — gamepadStatusLabel_ already declared + wired
```

**What needs adding/changing:**
```
Source/
├── SdlContext.h/.cpp      — NEW: process-level SDL singleton (atomic refcount)
├── GamepadInput.h/.cpp    — MODIFY: use SdlContext, add debounce timestamps, dead zone
├── PluginProcessor.h/.cpp — MODIFY: CC dedup state, gamepadActive_ flag
├── PluginEditor.h/.cpp    — MODIFY: [GAMEPAD ON/OFF] button, update status label text
```

### Pattern 1: Process-Level SDL Singleton (Atomic Ref-Count)

**What:** A file-scope (translation-unit-static or namespace-static) object that calls `SDL_Init` on first reference and `SDL_Quit` when the last reference goes away. Uses `std::atomic<int>` for the reference count so it is safe from any thread.

**When to use:** Any time a shared hardware/OS resource must be initialized exactly once per process regardless of how many plugin instances exist.

**Why not rely on SDL's own ref-count:** SDL2's built-in subsystem ref-counting is documented to have edge-case miscounts when subsystem dependencies are involved (e.g., GAMECONTROLLER auto-inits JOYSTICK, which auto-inits EVENTS). A thin wrapper that calls `SDL_Init(SDL_INIT_GAMECONTROLLER)` once and tracks the caller count avoids this.

**Example:**
```cpp
// Source/SdlContext.h
#pragma once

// Process-level SDL2 lifecycle guard.
// Call SdlContext::acquire() from GamepadInput ctor.
// Call SdlContext::release() from GamepadInput dtor.
// SDL_Init is called exactly once per process; SDL_Quit when last caller releases.

struct SdlContext
{
    // Returns true if SDL is now (or already was) initialised.
    static bool acquire();

    // Must be called once for every successful acquire().
    static void release();

    // True if SDL was successfully initialised in this process.
    static bool isAvailable();
};
```

```cpp
// Source/SdlContext.cpp
#include "SdlContext.h"
#include <SDL.h>
#include <atomic>

static std::atomic<int> gRefCount { 0 };
static std::atomic<bool> gAvailable { false };

bool SdlContext::acquire()
{
    if (gRefCount.fetch_add(1, std::memory_order_acq_rel) == 0)
    {
        // First caller — do the actual SDL_Init
        SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        const bool ok = (SDL_Init(SDL_INIT_GAMECONTROLLER) == 0);
        gAvailable.store(ok, std::memory_order_release);
        return ok;
    }
    // Subsequent callers — SDL already initialised
    return gAvailable.load(std::memory_order_acquire);
}

void SdlContext::release()
{
    if (gRefCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        // Last caller — shut down SDL
        SDL_Quit();
        gAvailable.store(false, std::memory_order_release);
    }
}

bool SdlContext::isAvailable()
{
    return gAvailable.load(std::memory_order_acquire);
}
```

**Thread safety note:** `gRefCount` and `gAvailable` are process-global atomics. Multiple `GamepadInput` instances in the same DAW process will share them correctly. Plugin instances running out-of-process (e.g., Bitwig sandbox) each get their own copy — that is fine and correct.

### Pattern 2: Per-Axis Square Dead Zone Using joystickThreshold

**What:** Before storing to `pitchX_`/`pitchY_` (and `filterX_`/`filterY_`), apply the threshold independently to each axis. If `|value| < threshold`, store 0.0f (dead) and do NOT update the sample-and-hold output.

**Key point:** The existing `normaliseAxis()` in `GamepadInput.cpp` already applies a hard-coded `kDeadzone = 0.08f`. This must be replaced with a threshold read from the APVTS. Since `GamepadInput` does not own the APVTS, the threshold must be passed in from `PluginProcessor::buildChordParams()` when reading axis values, OR `GamepadInput` must accept a threshold parameter on construction or via a setter.

**Recommended approach:** Pass threshold as a float parameter to a new `setPitchDeadZone(float)` method on `GamepadInput`, called from `PluginProcessor::processBlock` once per block (same pattern as other APVTS params are read in processBlock). Alternatively, read it in `timerCallback` via an `std::atomic<float> deadZone_` that the processor sets.

**Example (dead zone in timerCallback):**
```cpp
// In GamepadInput.h — add:
void setDeadZone(float t) { deadZone_.store(t, std::memory_order_relaxed); }

// In GamepadInput.cpp — replace hard-coded kDeadzone usage:
float GamepadInput::normaliseAxis(int16_t raw) const
{
    const float v  = static_cast<float>(raw) / 32767.0f;
    const float dz = deadZone_.load(std::memory_order_relaxed);
    return std::abs(v) < dz ? 0.0f : v;
}
```

**Sample-and-hold for center return:** When `normaliseAxis` returns 0.0f, the existing code stores 0.0f into `pitchX_`/`pitchY_`. This is WRONG for the locked decision (hold last pitch when within dead zone). The fix: track `lastPitchX_` / `lastPitchY_` in `GamepadInput` and only update them when the axis is outside the dead zone.

```cpp
// timerCallback — right stick section:
const float rawX = normaliseAxis(SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTX));
const float rawY = -normaliseAxis(SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTY));

if (std::abs(rawX) >= deadZone_.load())
    lastPitchX_ = rawX;
if (std::abs(rawY) >= deadZone_.load())
    lastPitchY_ = rawY;

pitchX_.store(lastPitchX_);
pitchY_.store(lastPitchY_);
```

Wait — note that `normaliseAxis` already zeroes values inside the dead zone. So "rawX == 0.0f" means "inside dead zone". The simpler form:

```cpp
if (rawX != 0.0f) lastPitchX_ = rawX;
if (rawY != 0.0f) lastPitchY_ = rawY;
pitchX_.store(lastPitchX_);
pitchY_.store(lastPitchY_);
```

Apply the same pattern to `filterX_`/`filterY_`.

### Pattern 3: Button Debounce (20ms in GamepadInput)

**What:** Track the timestamp of the last button state change. Ignore a change to `pressed` if less than 20ms has elapsed since the last change for that button.

**SDL2 API:** `SDL_GetTicks()` returns `Uint32` milliseconds since SDL_Init. Safe to call from the Timer thread (no video dependency).

**Example:**
```cpp
// In GamepadInput.h — add per-button tracking:
struct ButtonState
{
    bool     prev    = false;
    Uint32   lastMs  = 0;
};
ButtonState btnVoice_[4];
ButtonState btnAllNotes_;
ButtonState btnStartStop_, btnDelete_, btnReset_, btnRecord_;

static constexpr Uint32 kDebounceMsBtn = 20;

// Helper:
bool debounce(bool cur, ButtonState& state)
{
    const Uint32 now = SDL_GetTicks();
    if (cur != state.prev && (now - state.lastMs) >= kDebounceMsBtn)
    {
        state.prev   = cur;
        state.lastMs = now;
        return true;  // state changed
    }
    return false;
}
```

**Caution:** The existing code uses `prevVoice_[]`, `prevAllNotes_`, etc. These can be merged into `ButtonState` structs to reduce boilerplate.

### Pattern 4: CC Dedup (Value-Change Gate in PluginProcessor)

**What:** In `PluginProcessor::processBlock`, before emitting CC74/CC71, compare the computed integer CC value to the last emitted value. Only send if different.

**Where:** In `PluginProcessor`, not `GamepadInput`. CONTEXT.md says CC logic is in PluginProcessor already.

**Example:**
```cpp
// In PluginProcessor.h — add:
int prevCcCut_ = -1;  // -1 = never sent
int prevCcRes_ = -1;

// In processBlock filter CC section:
const int ccCut = juce::jlimit(0, 127, (int)(((gfx + 1.0f) * 0.5f) * xAtten));
const int ccRes = juce::jlimit(0, 127, (int)(((gfy + 1.0f) * 0.5f) * yAtten));

if (ccCut != prevCcCut_)
{
    midi.addEvent(juce::MidiMessage::controllerEvent(fCh, 74, ccCut), 0);
    prevCcCut_ = ccCut;
}
if (ccRes != prevCcRes_)
{
    midi.addEvent(juce::MidiMessage::controllerEvent(fCh, 71, ccRes), 0);
    prevCcRes_ = ccRes;
}
```

**CC reset on disconnect:** When `onConnectionChange(false)` fires (from GamepadInput callback), PluginProcessor must queue a CC reset. Since the callback fires on an SDL background thread and `processBlock` runs on the audio thread, use an `std::atomic<bool> pendingCcReset_` flag:

```cpp
// In GamepadInput's onConnectionChange callback setup (in PluginProcessor ctor):
gamepad_.onConnectionChange = [this](bool connected)
{
    if (!connected)
        pendingCcReset_.store(true, std::memory_order_release);
    // ... existing UI callback ...
};

// In processBlock, at the top of the filter CC section:
if (pendingCcReset_.exchange(false, std::memory_order_acq_rel))
{
    const int fCh = (int)apvts.getRawParameterValue(ParamID::filterMidiCh)->load();
    midi.addEvent(juce::MidiMessage::controllerEvent(fCh, 74, 0), 0);
    midi.addEvent(juce::MidiMessage::controllerEvent(fCh, 71, 0), 0);
    prevCcCut_ = 0;
    prevCcRes_ = 0;
}
```

### Pattern 5: Per-Instance GAMEPAD ACTIVE Toggle

**What:** A `TextButton` with `setClickingTogglesState(true)`. When OFF, `PluginProcessor::processBlock` skips consuming gamepad triggers and skips writing CC. `GamepadInput` still polls (shared SDL state is unaffected).

**Where to add in PluginProcessor:** Add `std::atomic<bool> gamepadActive_ { true }` to PluginProcessor. Editor writes it when button is toggled.

**UI placement:** Locked decision says near the status label at the bottom of PluginEditor. The existing `gamepadStatusLabel_` is placed in `resized()` at `right.removeFromTop(16)`. Add the toggle button beside it in the same row (split the 16px row or give it its own row).

**Existing status label text:** The current code uses `"Gamepad: Connected"` / `"Gamepad: --"`. The locked decision specifies `"GAMEPAD: connected"` / `"GAMEPAD: none"`. Update both strings.

### Anti-Patterns to Avoid

- **Calling SDL_Init/SDL_Quit per GamepadInput instance:** The current code does this — it will crash when two plugin instances exist in the same DAW process, because the second SDL_Quit tears down SDL while the first instance is still polling.
- **Calling SDL_PollEvent from a non-initialization thread:** SDL2 docs say PollEvent should be called from the thread that initialized SDL (or a thread with the message pump). With `SDL_HINT_JOYSTICK_THREAD "1"`, SDL runs its own internal thread for raw input; `SDL_PollEvent` in the JUCE Timer callback (message thread) is safe because JUCE's message thread IS the process's main UI thread, which is where SDL was initialized.
- **Using std::mutex in processBlock:** For the CC dedup and pending-reset flag, use only `std::atomic`. The JUCE community and Timur Doumler's talks confirm: never use mutex on the audio thread.
- **Sending allNotesOff for every voice channel on disconnect:** The all-notes-off should go to each of the four voice channels AND the filterMidiCh channel for the CC reset. Don't use a blanket all-channels reset — send targeted messages.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Raw input background thread on Windows | Custom WM_DEVICECHANGE listener | `SDL_HINT_JOYSTICK_THREAD "1"` (already in code) | SDL handles the hidapi/rawinput thread internally; manual WM_DEVICECHANGE in a plugin DLL is fragile |
| Controller hot-plug detection | Poll SDL_NumJoysticks() every frame | SDL_CONTROLLERDEVICEADDED/REMOVED events (already in timerCallback) | Events are already firing correctly; extra polling is redundant |
| SDL per-subsystem ref-counting | Trust SDL's built-in ref-count | Our own `SdlContext` atomic wrapper | SDL2 issue #8381: built-in ref-count has known miscount bugs with dependency subsystems |

**Key insight:** The SDL2 API for game controllers is complete and stable. Every required feature (axis reading, button reading, hot-plug, dead zones via application code) has a documented API. No custom hardware interaction layer is needed.

---

## Common Pitfalls

### Pitfall 1: Two Plugin Instances — SDL_Quit Race

**What goes wrong:** Two `PluginProcessor` instances are created in the same DAW. Each constructs a `GamepadInput`, each calls `SDL_Init`. When the first instance is destroyed, `GamepadInput` destructor calls `SDL_Quit`. The second instance's timer fires and calls `SDL_PollEvent` on a torn-down SDL — crash or undefined behavior.

**Why it happens:** Current `GamepadInput` owns SDL lifecycle unconditionally. This is the top-priority bug to fix.

**How to avoid:** Replace per-instance SDL_Init/Quit with `SdlContext::acquire()`/`release()`.

**Warning signs:** Crash on DAW track duplication, crash on plugin reopen.

### Pitfall 2: SDL_VIDEO Disabled — Hot-Plug May Silently Fail

**What goes wrong:** `SDL_VIDEO OFF` in CMake means the video subsystem is compiled out. On SDL 2.0.16–2.0.21, `SDL_INIT_GAMECONTROLLER` without video could silently fail to receive raw input events on Windows. Fixed by `SDL_HINT_JOYSTICK_THREAD "1"` in SDL 2.0.22+. SDL 2.30.9 includes this fix.

**Why it happens:** SDL's raw input backend on Windows historically needed a Win32 message pump (provided by the video subsystem). The joystick thread hint was the official fix.

**How to avoid:** The existing code already sets both hints before SDL_Init. Verified correct. No action needed — but must not remove these hints.

**Warning signs:** Controller not detected on first launch; hot-plug not working. Test by plugging in controller after plugin loads.

### Pitfall 3: Dead Zone Applied Twice

**What goes wrong:** `normaliseAxis()` zeroes values inside the dead zone. Then the caller compares to 0.0f to decide sample-and-hold. But the existing code ALSO has `kDeadzone = 0.08f` hard-coded. If the refactor changes dead zone to use `joystickThreshold` but forgets to remove `kDeadzone`, two dead zones stack.

**How to avoid:** Remove `kDeadzone` constant. Replace with `deadZone_.load()` from atomic. Ensure only one threshold applies.

### Pitfall 4: onConnectionChange Fires on SDL Internal Thread

**What goes wrong:** With `SDL_HINT_JOYSTICK_THREAD "1"`, SDL's hot-plug thread fires the `SDL_CONTROLLERDEVICEADDED/REMOVED` events. However, in the current code, `SDL_PollEvent` is called in the JUCE Timer (message thread). Events are therefore dispatched on the message thread — NOT the SDL internal thread.

**Clarification:** `SDL_HINT_JOYSTICK_THREAD "1"` means SDL's internal thread detects device changes and pushes them into SDL's event queue. `SDL_PollEvent` (called from the JUCE Timer on the message thread) dequeues them. Therefore `onConnectionChange` fires on the message thread, which is correct for JUCE UI updates. The existing `juce::MessageManager::callAsync` wrapper is redundant but harmless.

**How to avoid:** No action needed; the architecture is correct. But the `pendingCcReset_` atomic flag IS needed for the CC-reset-on-disconnect path, because processBlock runs on the audio thread and the timer-fired onConnectionChange callback runs on the message thread.

### Pitfall 5: All-Notes-Off Sent from Timer Thread

**What goes wrong:** If the disconnect logic tries to push MIDI messages directly from `onConnectionChange` (message thread), it cannot write into the `MidiBuffer` which is owned by `processBlock` (audio thread).

**How to avoid:** Use the `pendingCcReset_` and a companion `pendingAllNotesOff_` atomic flag. `processBlock` checks these at the top of each block and emits the MIDI messages safely.

### Pitfall 6: Gamepad Status Label Text Mismatch

**What goes wrong:** The existing code sets `"Gamepad: Connected"` and `"Gamepad: --"`. The locked decision specifies `"GAMEPAD: connected"` and `"GAMEPAD: none"`. A mismatch here will confuse users or testers.

**How to avoid:** Update both strings in `PluginEditor` constructor (the `onConnectionChange` lambda) to match exactly.

---

## Code Examples

Verified patterns from the existing codebase and SDL2 official docs:

### SDL Init/Quit (corrected pattern — SdlContext)
```cpp
// Source/GamepadInput.cpp (after adding SdlContext):
GamepadInput::GamepadInput()
{
    if (!SdlContext::acquire())
    {
        DBG("SDL_Init failed: " << SDL_GetError());
        return;
    }
    sdlInitialised_ = true;

    tryOpenController();
    startTimerHz(60);
}

GamepadInput::~GamepadInput()
{
    stopTimer();
    closeController();
    if (sdlInitialised_)
        SdlContext::release();
}
```

### SDL GameController Axis Mapping (verified from SDL2 wiki)
```cpp
// SDL_GameControllerAxis values (SDL2/SDL_GameControllerAxis):
// SDL_CONTROLLER_AXIS_LEFTX         — left stick X
// SDL_CONTROLLER_AXIS_LEFTY         — left stick Y
// SDL_CONTROLLER_AXIS_RIGHTX        — right stick X
// SDL_CONTROLLER_AXIS_RIGHTY        — right stick Y
// SDL_CONTROLLER_AXIS_TRIGGERLEFT   — L2, range 0..32767 (never negative)
// SDL_CONTROLLER_AXIS_TRIGGERRIGHT  — R2, range 0..32767 (never negative)
// SDL_GameControllerGetAxis() returns Sint16 (-32768..32767 for sticks, 0..32767 for triggers)
```

### All-Notes-Off on Disconnect (via atomic flag in processBlock)
```cpp
// In PluginProcessor.h:
std::atomic<bool> pendingAllNotesOff_ { false };
std::atomic<bool> pendingCcReset_     { false };

// In processBlock, before trigger system:
if (pendingAllNotesOff_.exchange(false, std::memory_order_acq_rel))
{
    for (int v = 0; v < 4; ++v)
        midi.addEvent(juce::MidiMessage::allNotesOff(voiceChs[v]), 0);
}
```

### Button Debounce Using SDL_GetTicks
```cpp
// SDL_GetTicks() returns Uint32 milliseconds since SDL was initialized
// Safe to call from timerCallback (same thread as SDL_PollEvent)
static constexpr Uint32 kDebounceMsBtn = 20;

// For each button:
const Uint32 now = SDL_GetTicks();
const bool   cur = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_A) != 0;
if (cur && !prevStartStop_ && (now - btnStartStopMs_) >= kDebounceMsBtn)
{
    looperStartStop_.store(true);
    btnStartStopMs_ = now;
}
prevStartStop_ = cur;
```

### CC Dedup (in PluginProcessor::processBlock)
```cpp
// Only gate on isConnected() AND gamepadActive_
if (gamepad_.isConnected() && gamepadActive_.load())
{
    const float gfx = gamepad_.getFilterX();
    const float gfy = gamepad_.getFilterY();
    const int ccCut = juce::jlimit(0, 127, (int)(((gfx + 1.0f) * 0.5f) * xAtten));
    const int ccRes = juce::jlimit(0, 127, (int)(((gfy + 1.0f) * 0.5f) * yAtten));

    if (ccCut != prevCcCut_) { midi.addEvent(...CC74, ccCut...); prevCcCut_ = ccCut; }
    if (ccRes != prevCcRes_) { midi.addEvent(...CC71, ccRes...); prevCcRes_ = ccRes; }
}
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| SDL_INIT_VIDEO required for game controllers on Windows | `SDL_HINT_JOYSTICK_THREAD "1"` as standalone fix | SDL 2.0.22 (2022) | No video subsystem needed; plugin-safe |
| Per-instance SDL_Init/Quit | Process singleton with atomic ref-count | Best practice for multi-instance plugins | Required for multiple instances in same DAW process |
| Radial (circular) dead zone | Per-axis (square) dead zone | User decision for ChordJoystick | Matches joystick mouse model already in codebase |

**Deprecated/outdated:**
- Hard-coded `kDeadzone = 0.08f`: Replace with `joystickThreshold` APVTS param read.
- `kTriggerThreshold = 8000`: This is the L2/R2 trigger axis threshold (not the stick dead zone). Keep this separate from `joystickThreshold` — triggers are 0..32767, sticks are -32768..32767. Do NOT reuse `joystickThreshold` for the trigger threshold.

---

## Open Questions

1. **SDL_GetTicks() wrap-around in debounce**
   - What we know: `SDL_GetTicks()` returns `Uint32`, wraps at ~49.7 days.
   - What's unclear: Subtraction `(now - lastMs)` handles wrap-around correctly in unsigned arithmetic (`Uint32` overflow is well-defined in C). If `now < lastMs` due to wrap, `now - lastMs` is a large positive number, which is > 20ms. This would incorrectly allow the button through as debounced. In practice, 49.7 days of uptime before this matters.
   - Recommendation: Acceptable risk; document it. Could use `SDL_GetTicks64()` if available in 2.30.9 (it is — added in 2.0.18).

2. **GAMEPAD ACTIVE toggle: should it be persisted via APVTS?**
   - What we know: Other toggle states (looper modes) are NOT in APVTS — they are ephemeral.
   - What's unclear: User may want DAW to remember which instance had GAMEPAD ON.
   - Recommendation: Keep it ephemeral (simple `std::atomic<bool>`) for Phase 06. APVTS persistence can be added later if requested.

3. **joystickThreshold range vs stick axis range**
   - What we know: `joystickThreshold` is a float param, range 0.001..0.1, default 0.015. This is normalized (0.0..1.0 scale). The stick axes return -1.0..+1.0 after `normaliseAxis`. A threshold of 0.015 means 1.5% stick deflection activates.
   - What's unclear: Whether 0.015 is appropriate as a gamepad dead zone (physical sticks drift more than mouse; typical gamepad dead zones are 0.1–0.2).
   - Recommendation: Reuse the param as locked, but note that the existing default may be too small for noisy gamepad hardware. The user can adjust it via the THRESH slider already in the UI.

4. **Per-instance vs shared GAMEPAD ACTIVE behavior when one instance is OFF**
   - What we know: Only the instance with GAMEPAD ON reads the controller (context says so). SDL polling in GamepadInput::timerCallback continues regardless — only the processBlock consumption is gated.
   - What's unclear: If GAMEPAD is OFF, should voice triggers from L1/R1/L2/R2 also be suppressed? Yes — the entire gamepad consumption block in processBlock should be gated.
   - Recommendation: Gate the entire gamepad section in processBlock (voice triggers, looper buttons, CC) behind `gamepadActive_.load()`.

---

## Sources

### Primary (HIGH confidence)
- SDL2 official wiki — `SDL_HINT_JOYSTICK_THREAD`: https://wiki.libsdl.org/SDL2/SDL_HINT_JOYSTICK_THREAD
- SDL2 official wiki — `SDL_Init` ref-counting: https://wiki.libsdl.org/SDL2/SDL_Init
- SDL2 official wiki — `SDL_PollEvent` thread constraint: https://wiki.libsdl.org/SDL2/SDL_PollEvent
- SDL2 official wiki — `SDL_GameControllerGetAxis`: https://wiki.libsdl.org/SDL2/SDL_GameControllerGetAxis
- SDL2 GitHub issue #5380 — `SDL_INIT_GAMECONTROLLER` without video on Windows; fix via `SDL_HINT_JOYSTICK_THREAD "1"` in 2.0.22: https://github.com/libsdl-org/SDL/issues/5380
- SDL2 official wiki — `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS`: https://wiki.libsdl.org/SDL2/SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS
- Existing codebase: `Source/GamepadInput.h/.cpp`, `Source/PluginProcessor.h/.cpp`, `Source/PluginEditor.h/.cpp`, `CMakeLists.txt` — direct code review

### Secondary (MEDIUM confidence)
- SDL2 GitHub issue #8381 — subsystem ref-count miscount bugs: https://github.com/libsdl-org/SDL/issues/8381
- JUCE forum — singleton shared between plugin instances: https://forum.juce.com/t/singleton-instance-shared-between-plugin-instances/39867
- timur.audio — using locks in real-time audio safely (confirms: no mutex on audio thread): https://timur.audio/using-locks-in-real-time-audio-processing-safely

### Tertiary (LOW confidence — not directly verified)
- SDL_WINDOWS_DETECT_DEVICE_HOTPLUG hint: added ~Jan 2026, purpose unclear (version branch unknown — may be SDL3 only). Not relevant to this build. Source: https://discourse.libsdl.org/t/sdl-added-temporary-workaround-hint-sdl-windows-detect-device-hotplug-a8266/66247

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — SDL2 2.30.9 is already in the build; JUCE 8.0.4 already linked; all APIs verified against official SDL2 docs
- Architecture: HIGH — based on direct code review of existing Source files; patterns are straightforward refactors of existing code
- SDL singleton pattern: MEDIUM — atomic ref-count is correct C++17; the specific risk of SDL subsystem ref-count bugs (issue #8381) motivates the wrapper, but the wrapper itself is simple and well-understood
- Pitfalls: HIGH — SDL_Quit race is directly observable from existing code; video subsystem requirement is documented in SDL2 issue #5380 (closed/fixed)
- Debounce pattern: HIGH — SDL_GetTicks() is documented; unsigned subtraction wrap is a known C behavior

**Research date:** 2026-02-23
**Valid until:** 2026-09-01 (SDL2 is in maintenance mode; APIs are stable)
