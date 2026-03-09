# Phase 38 Context — Quick Fixes & Rec Lane Undo

## Phase Goal
Seven targeted bug fixes: play button flash, rec lane undo, LFO cross-mod slider tracking,
looper force-stop on DAW transport stop, joystick additive offset model, cursor burst for all
trigger sources, and LFO Freq MOD FIX additive pattern.

---

## Decision A — Rec Lane Undo

### Visual states for rec lane buttons (Gates / Joy / Mod Whl)
Three distinct states:
- **Dim (gray)** — no content recorded, lane not armed
- **Subdued green** — has captured content, not currently recording
- **Bright green** — actively recording

### Behavior: press lit button while looper is PLAYING (not recording)
- Clear all events of that lane type from `playbackStore_`
- Button goes **dim (gray)**
- Looper keeps playing; other lanes unaffected
- Applies identically to: REC GATES, REC JOY, REC MOD WHL

### Behavior: press lit button while looper is RECORDING that lane
- Stop recording that lane immediately (set the `recXxx_` flag to false)
- **Keep** whatever was captured so far in the FIFO (do not discard)
- `finaliseRecording()` will merge it into `playbackStore_` as normal when record fully stops
- Button goes **subdued green** (has content, no longer recording)

### No cross-lane clearing
Clearing Gates does not affect Joy or Mod Whl, and vice versa.

### New LooperEngine API needed
`clearLane(LaneMask)` — removes events of the given type(s) from `playbackStore_` atomically.
Call from message thread; audio thread reads `playbackStore_` only during `process()`.
`LaneMask` enum: `Gates`, `Joy`, `Filter`.

---

## Decision B — Looper Force-Stop on DAW Transport Stop

### Trigger condition
Only when **DAW Sync is ON** (`looper_.isSyncToDaw() == true`).
In free-running mode (DAW Sync OFF) the looper continues when transport stops.

### What gets stopped
1. If looper is **playing** → call `looper_.startStop()` (stops playback, emits note-offs via `allNotesOff()`)
2. If looper is **recording** at the same time → recording is finalized first (same as calling `record()` to disarm, then `startStop()`)
3. All active looper voices get note-off before halt

### Implementation hook
In `processBlock`, detect falling edge of `isDawPlaying` (existing `prevDawPlaying_` already tracked).
When `prevDawPlaying_ && !isDawPlaying && looper_.isSyncToDaw()` → force stop.

---

## Decision C — Joystick Additive Offset (not override)

### Model
The looper joystick playback is the **base**. Mouse and gamepad right stick add an **offset**:

```
effectiveJoyX = clamp(looperJoyX + offsetX, -1, 1)
effectiveJoyY = clamp(looperJoyY + offsetY, -1, 1)
```

When no looper content or looper not playing, offset applies directly to the raw joystick position
(same behavior as today — no change for non-looper use).

### Mouse offset (sticky)
- Click/drag anywhere on JoystickPad sets `mouseOffsetX/Y = clickNormalizedPos - center(0,0)`
- Offset **persists after mouseUp** — click center to reset to zero
- The cursor visual shows `looperPos + offset` (the effective position)
- This means the cursor no longer springs back to center during looper playback

### Gamepad right stick offset (non-sticky)
- Gamepad right stick deflection adds to offset the same way
- Stick returns to center naturally → offset returns to zero automatically
- No stickiness for stick (physical spring does the work)

### Combined offset
`offsetX = mouseOffsetX + stickOffsetX` (both can be active simultaneously, clamped at ±1 total)

### New atomics needed in PluginProcessor
- `looperJoyOffsetX_` / `looperJoyOffsetY_` (atomic float) — written by UI thread (mouseDown/Drag)
  and gamepad dispatch; read by audio thread in `processBlock`
- The offset is NOT a parameter (not in APVTS), not saved with presets

### Audio thread logic change
Currently lines 548–551 gate the gamepad when looper has joystick content.
Currently lines 812–814 unconditionally write looper joystick output to atomics.

New logic:
1. Remove the `hasJoystickContent` gating of gamepad (gamepad stick becomes offset source)
2. After `loopOut` is computed, add offset rather than replacing:
   ```
   if (loopOut.hasJoystickX) joystickX.store(clamp(loopOut.joystickX + offsetX, -1, 1))
   if (loopOut.hasJoystickY) joystickY.store(clamp(loopOut.joystickY + offsetY, -1, 1))
   ```
3. When looper is NOT playing, offset still applies to the live `joystickX`/`joystickY` atomics
   (written by JoystickPad's timerCallback as today)

---

## Decision D — Play Button Flash States

### Four cases (DAW Sync ON):
| Transport | Looper state | Play button |
|-----------|-------------|-------------|
| Stopped   | Not playing, not armed | Off |
| Stopped   | REC WAIT armed | **Flashing** (D1 = 1) |
| Playing   | Waiting to sync-start | Flashing |
| Playing   | Playing | Solid on |

### Current bug
`syncArmed = proc_.looperIsSyncToDaw() && !proc_.looperIsPlaying()` — this is true when
transport is stopped + sync ON + looper not playing, which flashes incorrectly.

### Fix
Add DAW playing state check:
```cpp
const bool syncArmed = proc_.looperIsSyncToDaw()
                    && !proc_.looperIsPlaying()
                    && proc_.isDawPlaying();   // NEW
```
`isDawPlaying()` must be exposed as an atomic read from `PluginProcessor`.

REC WAIT armed flash is unaffected — it uses a separate `looperIsRecWaitArmed()` check
that does NOT gate on transport state (by design: D1 = user wants it to flash while stopped).

---

## Remaining Fixes (no gray areas — implementation-determined)

### Fix 3 — LFO cross-mod Level slider visual tracking
When `filterXMode == 10` (X stick → LFO-Y Level) or `filterYMode == 10` (Y stick → LFO-X Level),
the target LFO's Level slider display atom must be written in `timerCallback`.
Existing pattern for Phase/Level tracking in `timerCallback` already handles indices 6 and 9;
extend to also handle the cross-axis indices 10 for Level.

### Fix 6 — Cursor burst for looper playback and arpeggiator
`voiceTriggerFlash_[voice].fetch_add(1)` is only called inside `TriggerSystem::onNote`.
Looper gate-on events and arp note-on events emit MIDI directly without going through that callback.

Add `voiceTriggerFlash_[voice].fetch_add(1, std::memory_order_relaxed)` at:
- Looper gate-on path (around line 1372–1374 in PluginProcessor.cpp where `loopOut.gateOn[v]` is handled)
- Arp note-on path (around line 1882–1884 where `arpActivePitch_ = pitch` is set)

No new atomics needed — reuse existing `voiceTriggerFlash_[4]`.

### Fix 7 — LFO Freq target: MOD FIX additive offset + Rate slider tracking
When `filterXMode` or `filterYMode` points to an LFO Freq target (indices 4, cross-indices 8),
the stick value must add a normalized offset on top of the Rate slider base value — same
additive pattern already used for Phase (index 5) and Level (index 6) targets.

Four affected cases: X→LFO-X Freq (4), X→LFO-Y Freq (8), Y→LFO-Y Freq (4), Y→LFO-X Freq (8).

The Rate slider display atom (`lfoXRateDisplay_` / `lfoYRateDisplay_`) must be updated whenever
`filterModOn` is true, not only when a physical gamepad stick is active — so moving the MOD FIX
knob with no gamepad connected still moves the slider visually.

---

## Code Context

| File | Relevant area |
|------|--------------|
| `PluginProcessor.cpp:536–551` | Gamepad + looper joystick merge; needs offset model |
| `PluginProcessor.cpp:811–814` | Looper joystick write-back to atomics; needs additive offset |
| `PluginProcessor.cpp:848–854` | `prevDawPlaying_` edge detect (existing); add looper force-stop here |
| `PluginProcessor.cpp:1372–1374` | Looper gate-on path; needs `voiceTriggerFlash_` increment |
| `PluginProcessor.cpp:1504–1506` | TriggerSystem `onNote` — existing flash; reference pattern |
| `PluginProcessor.cpp:1882–1884` | Arp note-on path; needs `voiceTriggerFlash_` increment |
| `PluginEditor.cpp:4736–4741` | Play button flash logic; needs `isDawPlaying()` guard |
| `PluginEditor.cpp:2542–2547` | Rec lane button onClick; needs clear-lane + visual state update |
| `LooperEngine.h:100–103` | `setRecJoy/setRecGates/setRecFilter`; new `clearLane()` needed |
| `PluginProcessor.h:217` | `voiceTriggerFlash_[4]` declaration |
