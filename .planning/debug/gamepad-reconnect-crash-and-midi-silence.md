---
status: resolved
trigger: "gamepad-reconnect-crash-and-midi-silence"
created: 2026-03-06T00:00:00Z
updated: 2026-03-06T02:00:00Z
---

## Current Focus

hypothesis: CONFIRMED — two root causes found, fixes applied.
test: build and verify against original reproduction steps
expecting: reconnect no longer crashes; synth stays audible after disconnect
next_action: user builds and tests

## Symptoms

expected: Controller can disconnect and reconnect (USB or Bluetooth) without crashing; MIDI flow to external synth continues uninterrupted regardless of controller state.
actual:
  1. Crash when controller was disconnected and tried to reconnect via USB or Bluetooth.
  2. After controller disconnects, synth goes silent. Plugin still sends MIDI to the intermediate MIDI track in Ableton, but the synth stops responding. "Seems stuck on the second MIDI track."
errors: Plugin crash on reconnect (Ableton crash or plugin unload). No error shown for MIDI issue.
reproduction:
  - Load plugin in Ableton. Controller connected → works fine, MIDI reaches synth.
  - Disconnect controller (physically or BT drop). Synth goes silent.
  - Reconnect controller → plugin/Ableton crashes.
started: Crash has happened before; reconnect handling was partially fixed with pendingReopenTick_ defer mechanism. MIDI silence: always present on controller disconnect.
routing:
  - Plugin (ChordJoystick VST3) on MIDI Track 1
  - MIDI Track 1 output → MIDI Track 2 (synth MIDI input)
  - MIDI Track 2 output → external hardware synth
  - Plugin single-channel routing mode: Channel 1
  - Synth configured for: Channel 2

## Eliminated

- hypothesis: Double SDL_GameControllerOpen crash (pendingReopenTick_ double-open)
  evidence: Guard `if (!controller_) pendingReopenTick_=true` was correct against double-open. Bug was different.
  timestamp: 2026-03-06T01:00:00Z

- hypothesis: Crash from SDL_GameControllerGetAttached on stale BT handle
  evidence: SDL_GameControllerGetAttached() is safe to call on disconnected handles (returns false, no crash).
  timestamp: 2026-03-06T01:00:00Z

## Evidence

- timestamp: 2026-03-06T01:00:00Z
  checked: GamepadInput.cpp CONTROLLERDEVICEREMOVED path
  found: Handler calls SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_)). On BT reconnect, SDL's background joystick thread (enabled by HINT_JOYSTICK_THREAD=1) can invalidate the joystick handle between the REMOVED event and the main thread's call. SDL_GameControllerGetJoystick() then returns nullptr. SDL_JoystickInstanceID(nullptr) → crash (SDL_SetError or internal assert depending on build).
  implication: CRASH ROOT CAUSE. Need null check on joy before calling SDL_JoystickInstanceID.

- timestamp: 2026-03-06T01:00:00Z
  checked: GamepadInput.cpp CONTROLLERDEVICEADDED path
  found: Original guard `if (!controller_) pendingReopenTick_ = true` skips the reopen when controller_ is still non-null (stale BT handle from a BT-drop where no REMOVED fired). Reconnect is silently dropped — user must physically disconnect and reconnect again to get the controller working. This is the "missed reconnect" scenario that can also lead to subsequent crashes when REMOVED finally arrives and finds a stale non-matching handle.
  implication: CRASH/SILENT-RECONNECT ROOT CAUSE. Fix: call closeController() to clear stale handle before setting pendingReopenTick_.

- timestamp: 2026-03-06T01:00:00Z
  checked: onConnectionChange(false) → pendingCcReset_ → processBlock CC emit
  found: pendingCcReset_ sends CC74=0 and CC71=0 on the filter MIDI channel (ch1 default). CC74=0 = filter cutoff fully closed (0 Hz equivalent). This passes through Ableton's MIDI Track 2 rechanneling to the hardware synth on ch2. The synth receives CC74=0 and closes its VCF. All subsequent note-ons play through a fully closed filter → inaudible. Synth appears "stuck silent." The value 0 was chosen as a generic "reset" value, but for a filter cutoff it is the worst possible value.
  implication: MIDI SILENCE ROOT CAUSE. Fix: send base-knob value (64 + filterXOffset/filterYOffset) instead of 0 for CC74 and CC71.

## Resolution

root_cause:
  BUG 1 — CRASH ON RECONNECT (GamepadInput.cpp):
    Two issues in the SDL event handler:
    (a) CONTROLLERDEVICEREMOVED calls SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_))
        without checking if SDL_GameControllerGetJoystick() returned nullptr. With
        SDL_HINT_JOYSTICK_THREAD=1, SDL's background thread can invalidate the joystick
        handle concurrently. Result: SDL_JoystickInstanceID(nullptr) → crash.
    (b) CONTROLLERDEVICEADDED guard `if (!controller_)` skips the reconnect when a stale
        non-null controller_ handle is held (BT reconnect without prior REMOVED event).
        Controller never re-opens, and subsequent events can find the handle in invalid state.

  BUG 2 — MIDI SILENCE ON DISCONNECT (PluginProcessor.cpp):
    pendingCcReset_ handler sends CC74=0 and CC71=0 as a "reset." CC74=0 = filter cutoff
    fully closed. The message reaches the hardware synth (via Ableton's rechanneling track)
    and permanently closes the VCF. All subsequent note-ons are inaudible. The fix sends
    the base-knob resting value (64 + filterXOffset/filterYOffset) so the filter returns
    to its configured position rather than being forced to minimum.

fix:
  GamepadInput.cpp:
    - ADDED handler: call closeController() before setting pendingReopenTick_ if controller_ is non-null
    - REMOVED handler: guard SDL_JoystickInstanceID call with null check on SDL_GameControllerGetJoystick result

  PluginProcessor.cpp:
    - pendingCcReset_ block: compute ccCutRest = clamp(64 + filterXOffset, 0, 127) and
      ccResRest = clamp(64 + filterYOffset, 0, 127); send those values for CC74/CC71 instead of 0.
      prevCcCut_/prevCcRes_ updated to match.

verification: pending user build + test
files_changed:
  - Source/GamepadInput.cpp
  - Source/PluginProcessor.cpp

routing_note:
  The plugin single-channel mode targets ch1; the synth is on ch2. Ableton's MIDI Track 2
  is rechanneling ch1→ch2. This works for note traffic but also rechannels CC74/CC71 to ch2,
  which is why the synth filter gets silenced. The CC fix above makes this safe regardless of
  the routing. No routing change required — the fix is in the plugin.
