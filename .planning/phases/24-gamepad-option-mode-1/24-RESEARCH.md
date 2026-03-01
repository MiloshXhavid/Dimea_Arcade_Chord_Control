# Phase 24: Gamepad Option Mode 1 - Research

**Researched:** 2026-03-01
**Domain:** C++ / JUCE VST3 gamepad input dispatch, APVTS parameter control, SDL2 button handling
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Mode 1 architecture — face buttons + D-pad coexist**
Mode 1 = face buttons do ARP dispatch AND D-pad still does octave control (existing behavior). Both work simultaneously because they use different physical buttons. No D-pad behavior changes. The user confirmed: "both at the same time since different buttons."

**Mode 1 face-button mapping (new):**
- Circle → toggle arp on/off
- Triangle (1x press) → step Arp Rate UP one step (1/4 → 1/8T → 1/8 → 1/16T → 1/16 → 1/32 → wraps to 1/4)
- Triangle (2x press within 300ms) → step Arp Rate DOWN one step
- Square (1x press) → step Arp Order UP one step (cycles through all 7 orders)
- Square (2x press within 300ms) → step Arp Order DOWN one step
- X → toggle RND Sync (randomClockSync APVTS param)

**Mode 1 D-pad mapping (unchanged from existing):**
- D-pad Up → rootOctave step
- D-pad Down → thirdOctave step
- D-pad Left → tensionOctave step
- D-pad Right → fifthOctave step

**Mode 2 — unchanged**
Mode 2 = INTRVL (D-pad → globalTranspose / thirdInterval / fifthInterval / tensionInterval). Phase 24 does NOT touch Mode 2.

**Circle toggle edge cases:**
- Looper STOPPED: toggle arp on → looper reset to beat 0 → start looper playback
- Looper ALREADY PLAYING: toggle arp on only — no looper reset, no position jump
- DAW sync active (any looper state): toggle arp on → arm arpWaitingForPlay_ (waits for bar boundary per Phase 23 fix)
- Toggling arp OFF (any state): just turn arp off — no looper side-effects

**Double-press timing window**
Use 300ms — identical to existing `kDpadDoubleClickMs` constant in GamepadInput.h. Reuse the same double-click detection architecture already used for D-pad deltas. Single-press fires direction +1; double-press within 300ms fires direction -1. Apply same `pendingOptionDpadDelta_` pattern but for face buttons (new atomic signals needed).

**R3 behavior**
R3 alone = already a no-op (Phase 19 removed R3 panic). Holding pad + R3 = Sub Oct toggle (Phase 19). Phase 24 must NOT change R3 behavior — verify only.

**UI label for Mode 1**
Change label from "OCTAVE" to "ARP" (colour stays green). Label is updated in timerCallback() at the same site as Mode 1 / Mode 2 label dispatch in PluginEditor.cpp.

### Claude's Discretion
- Exact names for the new face-button delta atomic signals in GamepadInput.h (e.g., `pendingArpFaceBtn_[4]` or individual atomics per button)
- Whether to reuse the existing `ButtonState` + `debounce()` architecture for face-button double-press or inline the 300ms check
- Where exactly in processBlock to dispatch face-button arp signals (alongside or after the existing Mode 1 D-pad dispatch block)
- APVTS step-clamp bounds for arpSubdiv (0–5) and arpOrder (0–6)

### Deferred Ideas (OUT OF SCOPE)
- PC scroll / Program Change mode on D-pad (was Mode 2 in Phase 15, replaced by INTRVL — if user wants it back, that's a separate phase)
- Mode 3 for future expansion
- Face button behavior in Mode 2 (currently unused in Mode 2)
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| OPT1-01 | In Option Mode 1 — Circle toggles Arp on/off; turning on resets looper to beat 0 and starts playback (or arms if DAW sync is active) | consumeArpCircle() atomic + bool param toggle + looper_.reset()/startStop() sequence in processBlock optMode==1 block |
| OPT1-02 | In Option Mode 1 — Triangle steps Arp Rate up (1 press = one step up); 2 quick presses = one step down | consumeArpRateDelta() returning +1/-2 using kDpadDoubleClickMs pattern; stepWrappingParam(apvts, "arpSubdiv", 0, 5, d) |
| OPT1-03 | In Option Mode 1 — Square steps Arp Order with same two-press architecture | consumeArpOrderDelta() returning +1/-2; stepWrappingParam(apvts, "arpOrder", 0, 6, d) |
| OPT1-04 | In Option Mode 1 — X button toggles RND Sync (randomClockSync parameter) | consumeRndSyncToggle() atomic; dynamic_cast<AudioParameterBool*> + setValueNotifyingHost toggle pattern (same as subOct toggle) |
| OPT1-05 | In any mode — holding a pad button (R1/R2/L1/L2) and pressing R3 toggles Sub Oct for that voice | Already implemented in Phase 19 — verify only, no code changes |
| OPT1-06 | R3 pressed alone no longer sends MIDI Panic — panic is UI button only | Already implemented in Phase 19 — verify only (consumeRightStickTrigger() anyHeld path) |
| OPT1-07 | Option Mode 2 (D-pad INTRVL control) continues working unchanged | No code changes; verify no regression in Mode 2 D-pad dispatch block |
</phase_requirements>

---

## Summary

Phase 24 adds face-button arp/random dispatch to Option Mode 1 on the PS gamepad. The existing architecture is clean and well-suited to extension: `GamepadInput.h` already has `ButtonState`/`debounce()` infrastructure and the `pendingOptionDpadDelta_` pattern (atomic int, +1 single press / -2 double-click) that should be replicated for three new signals (arp rate, arp order, rnd sync toggle). A fourth signal for Circle (arp on/off with looper side-effects) requires slightly different handling because it needs conditional looper control beyond a simple parameter step.

All the APVTS parameters required exist: `arpEnabled` (bool), `arpSubdiv` (choice 0–5), `arpOrder` (choice 0–6), `randomClockSync` (bool). The utility functions `stepWrappingParam()` and `stepClampingParam()` already exist in `PluginProcessor.h`. The bool-toggle pattern for APVTS AudioParameterBool is already established in Phase 19's sub-octave implementation. Requirements OPT1-05, OPT1-06, and OPT1-07 are already implemented and require only verification.

The UI change is a one-line string swap in `PluginEditor.cpp::timerCallback()` line ~2893: `"OCTAVE"` → `"ARP"`. No layout changes are needed. Mode 0 face buttons (looper controls) are unaffected — they operate unconditionally regardless of optMode.

**Primary recommendation:** Add four new atomic signals in GamepadInput.h (`pendingArpCircle_`, `pendingArpRateDelta_`, `pendingArpOrderDelta_`, `pendingRndSyncToggle_`) with corresponding ButtonState entries and double-click tracking for the delta signals. Dispatch them in the existing `optMode == 1` block in PluginProcessor.cpp::processBlock(). Apply the bool-toggle pattern for Circle (arpEnabled) and RND Sync, and stepWrappingParam for Rate and Order.

---

## Standard Stack

### Core
| Component | Version/Location | Purpose | Why Standard |
|-----------|-----------------|---------|--------------|
| SDL_GameControllerGetButton | SDL2 (already linked) | Read face button state | Already used for A/B/X/Y looper buttons |
| std::atomic<bool> / std::atomic<int> | C++17 stdlib | Cross-thread signal delivery | Established pattern in GamepadInput.h for all existing signals |
| juce::AudioProcessorValueTreeState | JUCE 8.0.4 | APVTS parameter read/write | All existing arp/random params use this |
| ButtonState struct + debounce() | GamepadInput.cpp | 20ms hardware debounce | Used for all existing buttons |

### Supporting
| Component | Location | Purpose | When to Use |
|-----------|---------|---------|-------------|
| stepWrappingParam() | PluginProcessor.h:318 | Wrapping int param step | arpSubdiv (0–5) and arpOrder (0–6) |
| dynamic_cast<AudioParameterBool*> + setValueNotifyingHost | PluginProcessor.cpp:519–522 | Bool param toggle | arpEnabled and randomClockSync toggles |
| looper_.isPlaying() / looper_.reset() / looper_.startStop() | PluginProcessor.cpp | Looper state query and control | Circle toggle-on edge case |
| kDpadDoubleClickMs (300ms) | GamepadInput.h:166 | Double-click window | Reused for face button double-press |

---

## Architecture Patterns

### Recommended File Changes
```
Source/
├── GamepadInput.h          # Add: 4 new atomic signals + ButtonState entries + consume methods
├── GamepadInput.cpp        # Add: face-button detection in timerCallback() (Mode 1 guard)
├── PluginProcessor.cpp     # Add: dispatch in optMode==1 block (~line 530)
└── PluginEditor.cpp        # Change: "OCTAVE" → "ARP" at line ~2893
```

### Pattern 1: Atomic Signal with Double-Click (Delta Signal)

Mirrors `pendingOptionDpadDelta_` for D-pad. Applied to Triangle (arp rate) and Square (arp order):

**GamepadInput.h additions:**
```cpp
// Face-button arp dispatch signals (Option Mode 1 only)
std::atomic<int>  pendingArpRateDelta_  {0};
std::atomic<int>  pendingArpOrderDelta_ {0};
uint32_t          arpRateLastPressMs_   = 0;
uint32_t          arpOrderLastPressMs_  = 0;
ButtonState       btnTriangle_;   // existing: btnRecord_ (SDL_CONTROLLER_BUTTON_Y)
ButtonState       btnSquare_;     // existing: btnReset_  (SDL_CONTROLLER_BUTTON_X)
```

**IMPORTANT: btnRecord_ and btnReset_ ARE the Triangle/Square buttons.** The existing `ButtonState` entries `btnRecord_` and `btnReset_` already debounce SDL_CONTROLLER_BUTTON_Y (Triangle) and SDL_CONTROLLER_BUTTON_X (Square). In Mode 0, those rising edges fire looperRecord_ and looperReset_. In Mode 1, the same rising edges must instead fire arp delta signals. The looper face-button dispatch (lines 504–507) fires UNCONDITIONALLY — it does not check optMode. This means in Mode 1, pressing Triangle/Square would fire BOTH looperRecord/looperReset AND the new arp signals.

**Resolution:** The face-button looper dispatch (lines 504–507) must be gated to `optMode == 0` for the face buttons (Triangle/Square), OR the new arp signals must use separate ButtonState trackers. The existing code already separates D-pad behavior by mode — the same mode-gate must apply to face buttons.

**Recommended approach:** In GamepadInput.cpp timerCallback(), move the face-button detection (B/A/X/Y) into mode-aware blocks. In Mode 0, fire looper signals. In Mode 1, fire arp signals. In Mode 0, Circle → looperDelete, X → looperReset, Triangle → looperRecord, Cross → looperStartStop. In Mode 1, Circle → arpCircle, Triangle → arpRateDelta, Square → arpOrderDelta, X → rndSyncToggle.

**Alternative approach (simpler, less refactor):** Keep all looper atoms firing in all modes. In processBlock, only consume the looper atoms when optMode == 0. Consume the arp atoms only when optMode == 1. This avoids restructuring timerCallback. The atoms just accumulate unread values which the consumer never consumes, then get overwritten — no harm since they're one-shot flags.

**Best approach (minimal change, zero regression risk):** Use separate `ButtonState` trackers for Mode 1 face-button arp dispatch — distinct from `btnRecord_`, `btnReset_`, `btnDelete_`, `btnStartStop_`. OR simply consume the looper flags early and discard them in Mode 1 processBlock. The existing consume pattern is fire-and-forget; unconsumed flags in an atomic are simply overwritten next press. The cleanest approach:
- In GamepadInput.h: add new `ButtonState btnFaceBtnCircle_`, `btnFaceBtnTriangle_`, `btnFaceBtnSquare_`, `btnFaceBtnCross_` — but that duplicates debounce work for the same physical buttons.

**Final verdict (read the code carefully):** The existing `btnStartStop_`, `btnDelete_`, `btnReset_`, `btnRecord_` ButtonState structs are already being used. The looper atoms (looperStartStop_, looperDelete_, looperReset_, looperRecord_) fire unconditionally. In processBlock line 504–507, the atoms are consumed unconditionally. This creates a conflict: in Mode 1, pressing Triangle would reset the looper while also stepping arp rate.

**The clean fix:** Gate the looper consume calls in processBlock behind `optMode == 0`. Then add new consume calls for arp signals inside `optMode == 1`. The timerCallback can continue firing all atoms unconditionally — the mode discrimination happens at the consume site in processBlock. Looper atoms set in Mode 1 will never be consumed (they're ignored). Arp atoms set in Mode 0 will never be consumed (they're ignored).

```cpp
// In processBlock, replacing lines 504-507:
if (optMode == 0)
{
    if (gamepad_.consumeLooperStartStop()) looper_.startStop();
    if (gamepad_.consumeLooperRecord())    looper_.record();
    if (gamepad_.consumeLooperReset())   { looper_.reset(); flashLoopReset_.fetch_add(1, ...); }
    if (gamepad_.consumeLooperDelete())  { looper_.deleteLoop(); flashLoopDelete_.fetch_add(1, ...); }
}
```

However, this is a behavior change: in Mode 2 (INTRVL), the looper face buttons would also stop working. The original code fires looper face buttons in ALL modes. Is that intentional? The CONTEXT.md says "What is NOT in scope: changing looper face-button behavior in Mode 0." — this implies looper buttons should still work in Mode 2 as well, only face-button dispatch in Mode 1 changes.

**Correct architecture:** Gate only Mode 1 face-button looper behavior. In timerCallback, add a separate Mode 1 detection pass that fires arp atoms using the same physical button reads. The looper atoms continue firing in all modes (so looper is still controllable from gamepad in Mode 2). In processBlock Mode 1 block, consume the arp atoms AND explicitly ignore/discard the looper atoms for those specific buttons.

**Simplest correct approach:**
1. In GamepadInput.h: add `pendingArpCircle_`, `pendingArpRateDelta_`, `pendingArpOrderDelta_`, `pendingRndSyncToggle_` atomics + `arpRateLastPressMs_`, `arpOrderLastPressMs_` timestamps + `ButtonState` entries for Mode 1 face buttons (can reuse existing button state since Mode 1 is exclusive to Mode 0 behaviors)
2. In GamepadInput.cpp timerCallback(): add a new block that only runs in Mode 1 (`if (optionMode == 1) { ... }`) detecting rising edges on B/Y/X/A buttons using separate ButtonState instances
3. In processBlock Mode 1 block: consume arp atoms. Separately, consume and discard looper atoms for those buttons when Mode 1 is active.

### Pattern 2: Bool Param Toggle (Circle / X buttons)

From Phase 19 subOct toggle at PluginProcessor.cpp:519–522:
```cpp
// Source: PluginProcessor.cpp line 519–522 (Phase 19 pattern)
auto* param = dynamic_cast<juce::AudioParameterBool*>(
    apvts.getParameter("arpEnabled"));
if (param != nullptr)
    param->setValueNotifyingHost(param->get() ? 0.0f : 1.0f);
```

For randomClockSync (X button), same pattern with `"randomClockSync"`.

### Pattern 3: Circle Toggle-On Looper Edge Cases

In processBlock, after toggling arpEnabled to ON:
```cpp
// OPT1-01: Circle toggle-on edge cases
const bool arpSyncActive = looper_.isSyncToDaw();
const bool wasOn = (currentArpParam->get());
// After toggle to ON:
if (!wasOn)  // just turned ON
{
    if (arpSyncActive)
    {
        // DAW sync: arm bar-boundary wait (arpWaitingForPlay_ set by existing logic)
        // No looper control needed — existing arpWaitingForPlay_ machinery handles it
    }
    else if (!looper_.isPlaying())
    {
        // Looper stopped: reset to beat 0, then start playback
        looper_.reset();
        looper_.startStop();
    }
    // If looper already playing: just toggled on — no looper action needed
}
```

**Key insight:** The existing arpWaitingForPlay_ logic in processBlock already handles the DAW sync arm case — it fires when `arpOn && !prevArpOn_ && clockRunning`. So for DAW sync: the arm behavior is automatic from the existing code when arpEnabled transitions from false to true. The Circle handler only needs to do the looper reset+start for the looper-stopped case.

### Pattern 4: Wrapping Param Step (Rate/Order)

```cpp
// Source: PluginProcessor.h:318–328
// arpSubdiv: 6 choices, indices 0–5
stepWrappingParam(apvts, "arpSubdiv", 0, 5, d);  // d = +1 or -1 (after collapsing -2 at boundary)
// arpOrder: 7 choices, indices 0–6
stepWrappingParam(apvts, "arpOrder", 0, 6, d);
```

Note on delta math: GamepadInput fires +1 for first press, then -2 for second press within window. The consumer receives these raw values. When the consumer calls `stepWrappingParam(..., d)` with d=-2, the wrapping math handles it correctly because the modulo-based formula `((cur + delta) % range + range) % range` works for any negative delta.

### Pattern 5: D-Pad Double-Click (Reference Implementation)

Existing code in GamepadInput.cpp timerCallback() for D-pad:
```cpp
// Source: GamepadInput.cpp line 247–257
const uint32_t elapsed = now - optDpadLastPressMs_[i];
const int delta = (optDpadLastPressMs_[i] != 0 && elapsed < kDpadDoubleClickMs)
                      ? -2 : +1;
pendingOptionDpadDelta_[i].fetch_add(delta, std::memory_order_relaxed);
optDpadLastPressMs_[i] = (delta == +1) ? now : 0;
```

Replicate this exact pattern for Triangle (arpRateDelta) and Square (arpOrderDelta). Timestamps: `arpRateLastPressMs_` and `arpOrderLastPressMs_`.

### Anti-Patterns to Avoid

- **Gating ALL face-button looper dispatch by optMode:** In Mode 2, looper face buttons should remain functional. Only Mode 1 arp dispatch conflicts with the looper face buttons semantically. The correct gate is: "in Mode 1, arp atoms are consumed; looper atoms still fire in timerCallback but are not consumed in Mode 1."
- **Using stepClampingParam for arpSubdiv/arpOrder:** These are choice params that should wrap (hitting end wraps to start), not clamp silently. Use `stepWrappingParam`.
- **Reading arpEnabled before toggling in processBlock:** The arpEnabled APVTS param is read at line 1143 as `arpOn`. The Circle toggle happens before that read (in the optMode==1 dispatch block at ~line 530). The toggle's effect on `arpOn` will be seen in the SAME block at line 1143 — the toggle fires `setValueNotifyingHost` which notifies APVTS; the atomic behind `getRawParameterValue` updates immediately. No extra prevState tracking needed for the toggle itself — the existing `prevArpOn_` already tracks it.
- **Calling looper_.reset() + looper_.startStop() when looper is already playing:** The Circle toggle-on code must check `looper_.isPlaying()` before calling reset+startStop. The existing `looper_.startStop()` toggles state, not "start unconditionally".
- **Conflating the -2 delta with index -2:** The -2 delta is a raw signal value. `stepWrappingParam` applies modulo math, so -2 on a 6-item list correctly steps backward 2 positions. But the intent is: first press fired +1 (moved forward), second press fires -2 (net: moved backward 1 from original position). This is the same -2/-1 net design from the D-pad delta pattern — document this clearly in comments.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Button debounce | Custom timer-based debounce | Existing `ButtonState` + `debounce()` lambda in timerCallback | Already handles 20ms bounce window consistently |
| Param wrapping | Manual modulo + bounds check | `stepWrappingParam()` in PluginProcessor.h | Correct modulo arithmetic for negative deltas |
| Bool param toggle | `apvts.state.setProperty(...)` directly | `dynamic_cast<AudioParameterBool*> + setValueNotifyingHost` | Notifies host, triggers APVTS listeners, correct API |
| Double-click window | New timer/counter | `kDpadDoubleClickMs` constant + SDL_GetTicks() timestamp | Already implemented for D-pad, exactly same spec |
| Cross-thread signal | std::mutex or queue | `std::atomic<int>::fetch_add` + `exchange(0)` | Lock-free, already the project pattern |

---

## Common Pitfalls

### Pitfall 1: Face-Button Looper Conflict in Mode 1

**What goes wrong:** Pressing Triangle in Mode 1 fires looperRecord_ atom unconditionally. processBlock consumes it unconditionally (line 504). Looper begins recording. Simultaneously, the arp rate steps. The user gets unexpected looper behavior.

**Why it happens:** timerCallback fires all button atoms regardless of optMode. processBlock consumes looper atoms regardless of optMode.

**How to avoid:** In processBlock, move the looper face-button consume calls (lines 504–507) inside an `optMode == 0` guard OR explicitly drain them when in Mode 1. Recommended: move looper consume calls to an `if (optMode == 0)` block. Verify that Mode 2 still has looper face-button control (only Mode 1 changes face-button behavior).

**Warning signs:** During smoke test, pressing Triangle in Mode 1 starts looper recording.

### Pitfall 2: Circle Double-Fire on Looper Reset+StartStop

**What goes wrong:** `looper_.reset()` then `looper_.startStop()` triggers two looper events in one processBlock call. If some other code path checks looper state change edges (like LFO recording arm at line 611), the reset may fire the clearRecording path before startStop fires looperJustStarted.

**Why it happens:** `loopOut.looperReset` is set by looper_.reset(), consumed later in the same processBlock at line 641 and line 907. If looper_.reset() then looper_.startStop() are both called before looper_.process(), the next looper_.process() call will see the reset + start together and handle both in one pass.

**How to avoid:** Call `looper_.reset()` then `looper_.startStop()` in that order before the looper_.process() call (which happens later in processBlock). The looper_.process() call integrates both commands atomically. Verify: in existing code, gamepad_.consumeLooperReset() calls looper_.reset() and gamepad_.consumeLooperStartStop() calls looper_.startStop() both before the looper_.process() call at ~line 590+. The Circle handler in Mode 1 can follow the same ordering.

**Warning signs:** After Circle toggle-on (looper stopped), looper starts from wrong beat position, or LFO recording state becomes inconsistent.

### Pitfall 3: arpEnabled Raw Value Read vs. Toggle Timing

**What goes wrong:** Circle handler reads arpEnabled to determine current state, then toggles. But the read and write happen across APVTS atomic access. Between the read and the toggle, if the UI also fires arpEnabledBtn_ (unlikely but architecturally possible), there's a TOCTOU race.

**Why it happens:** APVTS parameters are written from the message thread (UI) and read from the audio thread (processBlock).

**How to avoid:** The gamepad dispatch in processBlock runs on the audio thread. The `setValueNotifyingHost` call in processBlock is called from the audio thread, which JUCE allows for `AudioProcessorParameter`. The APVTS rawParameterValue is an atomic float, so the read-then-write sequence is the same pattern used by Phase 19 subOct toggle. No additional protection is needed — follow the exact Phase 19 pattern.

### Pitfall 4: stepWrappingParam Range for arpSubdiv

**What goes wrong:** Calling `stepWrappingParam(apvts, "arpSubdiv", 0, 5, d)` but the param is declared as a Choice with 6 items (indices 0–5 in the addChoice call). If `convertTo0to1` expects values in the range passed to `NormalisableRange`, and choice params use 0-based integer indexing, the range 0..5 is correct.

**Why it happens:** Choice params in JUCE use integer indices starting at 0. The APVTS declaration is:
```
addChoice(ParamID::arpSubdiv, "Arp Subdivision",
          { "1/4", "1/8T", "1/8", "1/16T", "1/16", "1/32" }, 2);  // 6 items → indices 0..5
addChoice(ParamID::arpOrder, "Arp Order",
          { "Up", "Down", "Up+Down", "Down+Up", "Outer-In", "Inner-Out", "Random" }, 0);  // 7 items → indices 0..6
```

**How to avoid:** Use `stepWrappingParam(apvts, "arpSubdiv", 0, 5, d)` (6-item range: 0..5) and `stepWrappingParam(apvts, "arpOrder", 0, 6, d)` (7-item range: 0..6). Confirm this matches existing usage of the same function for "globalTranspose" (0..11) and "thirdInterval" (0..12) in Mode 2.

### Pitfall 5: OPT1-02 Step Sequence Mismatch

**What goes wrong:** REQUIREMENTS.md says Triangle steps "1/4 → 1/8 → 1/16 → 1/32" but APVTS declaration is `{ "1/4", "1/8T", "1/8", "1/16T", "1/16", "1/32" }` (6 items including triplets). The requirement shorthand omits triplet values.

**Why it happens:** Requirements were written as shorthand. The actual implementation must step through all 6 defined options including triplets.

**How to avoid:** The CONTEXT.md is authoritative: "Step Arp Rate UP one step (1/4 → 1/8T → 1/8 → 1/16T → 1/16 → 1/32 → wraps to 1/4)." All 6 subdivisions are included. stepWrappingParam with range 0..5 steps through all of them. Do not skip triplet indices.

---

## Code Examples

### New Atomic Signals in GamepadInput.h

```cpp
// Source: GamepadInput.h (new additions, Pattern matches existing pendingOptionDpadDelta_)
// Face-button arp dispatch signals (Option Mode 1 only)
// +1 = single press, -2 = fast double-click within kDpadDoubleClickMs
std::atomic<int>  pendingArpRateDelta_  {0};
std::atomic<int>  pendingArpOrderDelta_ {0};
std::atomic<bool> pendingArpCircle_     {false};
std::atomic<bool> pendingRndSyncToggle_ {false};

uint32_t          arpRateLastPressMs_   = 0;
uint32_t          arpOrderLastPressMs_  = 0;

// ButtonState trackers for Mode 1 face buttons (separate from looper button states)
ButtonState       btnMode1Circle_;
ButtonState       btnMode1Triangle_;
ButtonState       btnMode1Square_;
ButtonState       btnMode1Cross_;
```

**Consume methods in GamepadInput.h (public):**
```cpp
int  consumeArpRateDelta()   { return pendingArpRateDelta_.exchange(0, std::memory_order_relaxed); }
int  consumeArpOrderDelta()  { return pendingArpOrderDelta_.exchange(0, std::memory_order_relaxed); }
bool consumeArpCircle()      { return pendingArpCircle_.exchange(false); }
bool consumeRndSyncToggle()  { return pendingRndSyncToggle_.exchange(false); }
```

### New Detection in GamepadInput.cpp timerCallback()

```cpp
// Source: Mirrors D-pad double-click block at GamepadInput.cpp:247–257
// Add AFTER the existing D-pad block (inside the if (!optionFrameFired_) guard):

if (optionMode_.load(std::memory_order_relaxed) == 1)
{
    // Mode 1 face buttons: Circle=arpToggle  Triangle=arpRate  Square=arpOrder  Cross=rndSync
    const bool curCircle   = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_B) != 0;
    const bool curTriangle = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_Y) != 0;
    const bool curSquare   = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_X) != 0;
    const bool curCross    = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_A) != 0;

    // Circle: simple rising-edge toggle
    if (debounce(curCircle, btnMode1Circle_) && btnMode1Circle_.prev)
        pendingArpCircle_.store(true);

    // Triangle: double-click delta for arp rate
    if (debounce(curTriangle, btnMode1Triangle_) && btnMode1Triangle_.prev)
    {
        const uint32_t elapsed = now - arpRateLastPressMs_;
        const int delta = (arpRateLastPressMs_ != 0 && elapsed < kDpadDoubleClickMs) ? -2 : +1;
        pendingArpRateDelta_.fetch_add(delta, std::memory_order_relaxed);
        arpRateLastPressMs_ = (delta == +1) ? now : 0;
    }

    // Square: double-click delta for arp order
    if (debounce(curSquare, btnMode1Square_) && btnMode1Square_.prev)
    {
        const uint32_t elapsed = now - arpOrderLastPressMs_;
        const int delta = (arpOrderLastPressMs_ != 0 && elapsed < kDpadDoubleClickMs) ? -2 : +1;
        pendingArpOrderDelta_.fetch_add(delta, std::memory_order_relaxed);
        arpOrderLastPressMs_ = (delta == +1) ? now : 0;
    }

    // Cross: simple rising-edge toggle
    if (debounce(curCross, btnMode1Cross_) && btnMode1Cross_.prev)
        pendingRndSyncToggle_.store(true);
}
```

### Dispatch in PluginProcessor.cpp processBlock()

```cpp
// Source: Fits inside existing optMode == 1 block at PluginProcessor.cpp:530–538
// Add AFTER the existing D-pad octave lines (~line 538):

// Mode 1 face-button arp dispatch
if (gamepad_.consumeArpCircle())
{
    auto* arpParam = dynamic_cast<juce::AudioParameterBool*>(
        apvts.getParameter("arpEnabled"));
    if (arpParam != nullptr)
    {
        const bool wasOn = arpParam->get();
        arpParam->setValueNotifyingHost(wasOn ? 0.0f : 1.0f);
        if (!wasOn)  // turning ON
        {
            if (!looper_.isSyncToDaw() && !looper_.isPlaying())
            {
                looper_.reset();
                looper_.startStop();
            }
            // DAW sync: existing arpWaitingForPlay_ arm logic fires automatically
            // (it reads arpOn from APVTS next time it evaluates; the param is now true)
        }
    }
}
{ const int d = gamepad_.consumeArpRateDelta();  if (d) stepWrappingParam(apvts, "arpSubdiv",      0, 5, d); }
{ const int d = gamepad_.consumeArpOrderDelta(); if (d) stepWrappingParam(apvts, "arpOrder",        0, 6, d); }
if (gamepad_.consumeRndSyncToggle())
{
    auto* rndParam = dynamic_cast<juce::AudioParameterBool*>(
        apvts.getParameter("randomClockSync"));
    if (rndParam != nullptr)
        rndParam->setValueNotifyingHost(rndParam->get() ? 0.0f : 1.0f);
}

// Mode 1 looper face-button drain: prevent Triangle/Square/Circle/Cross from
// firing looper commands while Mode 1 arp dispatch is active.
gamepad_.consumeLooperStartStop();
gamepad_.consumeLooperRecord();
gamepad_.consumeLooperReset();
gamepad_.consumeLooperDelete();
```

**Note on the drain approach:** The "drain unconsumed looper atoms" approach is used instead of moving the looper consume block behind an optMode==0 guard. This is safer because Mode 2 continues to have looper face-button control (the looper consume block at lines 504–507 remains unconditional; it only fires when optMode is NOT 1, because the drain in Mode 1 beats it... actually the drain needs to happen before the unconditional consume calls).

**Revised approach:** Move the unconditional looper consume block (lines 504–507) INSIDE `if (optMode == 0 || optMode == 2)`. This is cleaner — Mode 1 gets arp dispatch, Modes 0 and 2 get looper dispatch. No drain needed.

```cpp
// Revised looper face-button dispatch — gate by mode
if (optMode == 0 || optMode == 2)  // NOT Mode 1 (face buttons → arp in Mode 1)
{
    if (gamepad_.consumeLooperStartStop()) looper_.startStop();
    if (gamepad_.consumeLooperRecord())    looper_.record();
    if (gamepad_.consumeLooperReset())   { looper_.reset(); flashLoopReset_.fetch_add(1, ...); }
    if (gamepad_.consumeLooperDelete())  { looper_.deleteLoop(); flashLoopDelete_.fetch_add(1, ...); }
}
```

### UI Label Change in PluginEditor.cpp

```cpp
// Source: PluginEditor.cpp line ~2893 — SINGLE LINE CHANGE
// Before:
: (optMode == 1) ? "OCTAVE"
// After:
: (optMode == 1) ? "ARP"
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| R3 = MIDI panic | R3 alone = no-op; panic is UI button | Phase 19 | OPT1-06 is already implemented |
| Pad+R3 = no-op | Pad+R3 = Sub Oct toggle | Phase 19 | OPT1-05 is already implemented |
| Mode 2 = PC scroll | Mode 2 = INTRVL (globalTranspose + intervals) | Phase 15 | OPT1-07 description in REQUIREMENTS.md is stale — Mode 2 is INTRVL, not PC scroll |
| arpGateTime param | Unified gateLength (0–1) | Phase 20 | No impact on Phase 24 |

**Deprecated/outdated:**
- REQUIREMENTS.md OPT1-07 description says "D-pad Program Change scrolling" — this is stale documentation. Mode 2 is INTRVL. OPT1-07 means "Mode 2 D-pad INTRVL behavior continues working unchanged." No code changes needed.

---

## Open Questions

1. **arpWaitingForPlay_ interaction with Circle toggle**
   - What we know: `arpWaitingForPlay_` is set when `arpOn && !prevArpOn_ && clockRunning` (line 1318). It is cleared when arpOn goes false (line 1321).
   - What's unclear: If Circle toggles arpEnabled to true in the same processBlock as the arpWaitingForPlay_ evaluation, does `arpOn` (line 1143) reflect the new value? YES — `getRawParameterValue` reads the APVTS atomic which is updated by `setValueNotifyingHost` immediately. The Circle dispatch happens before line 1143, so `arpOn` will be `true` in the same block.
   - Recommendation: The Circle toggle-on in processBlock will cause `arpWaitingForPlay_` to arm in the same block IF the clock is running. This is correct behavior for DAW sync. For looper-stopped case, the looper.reset()+startStop() fires before looper_.process() so the looper starts and looperJustStarted becomes true in the NEXT block — which clears arpWaitingForPlay_. This is correct.

2. **prevArpOn_ and same-block toggle**
   - What we know: `prevArpOn_` (line 1340) is set from `arpOn` at the END of the arm-and-wait block. Circle dispatch happens BEFORE arpOn is read.
   - What's unclear: If arpOn was false last block (prevArpOn_=false) and Circle fires this block setting arpOn to true, the condition `arpOn && !prevArpOn_ && clockRunning` (line 1318) correctly fires.
   - Recommendation: No issue. The existing arm logic handles this correctly.

3. **Mode 1 face-button detection placement in timerCallback**
   - What we know: The D-pad double-click detection runs inside `if (!optionFrameFired_)` guard and inside `if (optionMode != 0)`.
   - What's unclear: Should the Mode 1 face-button detection also be inside `!optionFrameFired_`? YES — if the Option button was pressed this frame, the mode just changed and it would be wrong to simultaneously fire a face-button action.
   - Recommendation: Wrap Mode 1 face-button detection inside the same `!optionFrameFired_` guard (or inside a separate `if (!optionFrameFired_ && optionMode == 1)` block).

---

## Sources

### Primary (HIGH confidence)
- `Source/GamepadInput.h` + `Source/GamepadInput.cpp` — direct code inspection of existing atomic signal patterns, ButtonState/debounce architecture, kDpadDoubleClickMs, pendingOptionDpadDelta_ pattern
- `Source/PluginProcessor.h` lines 318–339 — stepWrappingParam and stepClampingParam implementations
- `Source/PluginProcessor.cpp` lines 504–546 — existing looper face-button dispatch and Mode 1/2 D-pad dispatch (direct inspection)
- `Source/PluginProcessor.cpp` lines 1307–1340 — arpWaitingForPlay_ arm-and-wait logic
- `Source/PluginProcessor.cpp` lines 241–246 — APVTS arpSubdiv (0..5) and arpOrder (0..6) declarations
- `Source/PluginEditor.cpp` lines 2889–2901 — optionLabel_ update site in timerCallback

### Secondary (MEDIUM confidence)
- `.planning/phases/24-gamepad-option-mode-1/24-CONTEXT.md` — user decisions, implementation notes, edge cases
- `.planning/REQUIREMENTS.md` — OPT1-01 through OPT1-07 definitions

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all required components directly inspected in codebase
- Architecture patterns: HIGH — exact code locations identified, patterns verified against existing implementations
- Pitfalls: HIGH — identified from direct code inspection of interaction points
- OPT1-05/06/07 verify-only status: HIGH — directly confirmed in Phase 19 commit notes and source code

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable, internal codebase — no external dependency changes)
