---
status: resolved
trigger: "midi-teardown-safety — audit and harden plugin teardown"
created: 2026-03-06T00:00:00Z
updated: 2026-03-06T00:00:00Z
---

## Current Focus

hypothesis: Several teardown gaps exist that could cause use-after-free or state corruption on reload
test: Static code audit of all destructor / releaseResources paths
expecting: Confirm gaps, apply fixes, verify logic
next_action: Apply fixes to PluginProcessor.cpp

## Symptoms

expected: Plugin closes all MIDI ports cleanly on unload; releaseResources() handles DAW "about to unload" callback; teardown is wrapped so ports are released even if something throws.
actual: Unknown — proactive audit
errors: No crash yet
reproduction: Plugin unload from DAW, DAW quit while loaded, host-side plugin unload
started: Proactive — user wants this hardened now

## Eliminated

(none yet)

## Evidence

- timestamp: 2026-03-06
  checked: GamepadInput destructor
  found: stopTimer() + join(sdlInitThread_) + closeController() + SdlContext::release() — correct order
  implication: No gap here; timer stops before SDL cleanup

- timestamp: 2026-03-06
  checked: GamepadInput::closeController() called from destructor
  found: Fires onConnectionChange(false) and onConnectionChangeUI(empty) during destruction
  implication: GAP — onConnectionChange fires into PluginProcessor members that may already be
               destructed (pendingAllNotesOff_, pendingCcReset_ are atomic members of PluginProcessor;
               gamepad_ is declared AFTER them in PluginProcessor.h so gamepad_ destructs FIRST).
               WAIT — actually C++ destroys in reverse declaration order, so gamepad_ (declared last
               in the private section) destructs BEFORE the atomic members — BUT the existing
               destructor nulls onConnectionChange before gamepad_ destructs, so this is already safe.

- timestamp: 2026-03-06
  checked: PluginProcessor destructor — onConnectionChange null
  found: gamepad_.onConnectionChange = nullptr; — safe, guards the allNotesOff/ccReset stores
  implication: onConnectionChange path is guarded

- timestamp: 2026-03-06
  checked: PluginProcessor destructor — onConnectionChangeUI null
  found: NOT nulled in PluginProcessor destructor — only nulled in PluginEditor destructor
  implication: GAP (minor): if the editor is already gone but the processor destructor fires
               GamepadInput::closeController() via the gamepad_ dtor, onConnectionChangeUI may
               be non-null (stale). However, PluginEditor::~PluginEditor nulls it synchronously
               before the editor is freed, so as long as the editor outlives the processor
               (which is the JUCE contract — editor is deleted before processor), this is safe.
               VERDICT: safe in practice; add a defensive null in PluginProcessor dtor anyway.

- timestamp: 2026-03-06
  checked: sdlInitThread_ race — destructor joins BEFORE closeController
  found: Correct. stopTimer(); join(); closeController(); — safe sequence
  implication: No gap

- timestamp: 2026-03-06
  checked: releaseResources() body
  found: Only calls trigger_.resetAllGates() — MISSING:
         (1) resetNoteCount() — noteCount_[16][128] left dirty if prepareToPlay-releaseResources
             cycle happens without processBlock firing a full allNotesOff path
         (2) looperActivePitch_.fill(-1) — looper pitch snapshots left stale
         (3) looperActiveSubPitch_ — left stale
         (4) arpActivePitch_ = -1 — arp pitch snapshot left stale
         (5) subHeldPitch_ — left stale
         (6) subOctSounding_ — left stale
         (7) LFO ramp-out floats left stale (lfoXRampOut_, lfoYRampOut_)
         (8) prevFilterX_/prevFilterY_ not reset (causes spurious CC on first block after reload)
         These are all audio-thread-only fields — safe to reset from releaseResources()
         (which is also called on the audio thread after the last processBlock).
  implication: GAP — state from one prepare/release cycle leaks into the next.
               Most dangerous: noteCount_ mismatch could cause note-off suppression on
               first re-arm if any non-zero count survived.

- timestamp: 2026-03-06
  checked: JoystickPad — inherits juce::Timer, starts timer in ctor
  found: JoystickPad is a member of PluginEditor. Its dtor is default (no explicit stopTimer).
         JUCE's Timer destructor calls stopTimer() automatically, so this is safe.
  implication: No gap

- timestamp: 2026-03-06
  checked: PluginEditor destructor
  found: Nulls onConnectionChangeUI, then stopTimer() — correct order
  implication: No gap

- timestamp: 2026-03-06
  checked: ScaleKeyboard APVTS listeners
  found: Adds 15 parameter listeners in ctor, removes all 15 in dtor — correct
  implication: No gap

- timestamp: 2026-03-06
  checked: LfoEngine
  found: Pure DSP class — no threads, no timers, no callbacks. Default dtor is correct.
  implication: No gap

- timestamp: 2026-03-06
  checked: LooperEngine
  found: Pure data class — lock-free FIFOs, atomics, no threads. Default dtor is correct.
  implication: No gap

- timestamp: 2026-03-06
  checked: GamepadInput::timerCallback — sdlInitThread_ spawn path
  found: If GamepadInput destructs before kSdlInitDelayMs, sdlThreadSpawned_=false,
         sdlInitThread_ is never started, joinable()=false → join() is a no-op. Safe.
  implication: No gap

- timestamp: 2026-03-06
  checked: closeController() fires callbacks during destructor
  found: onConnectionChange already nulled in PluginProcessor dtor.
         onConnectionChangeUI nulled in PluginEditor dtor.
         BUT: PluginProcessor dtor only nulls onConnectionChange, not onConnectionChangeUI.
         If (somehow) the editor is gone and the processor dtor fires, the GamepadInput dtor
         calls closeController() which calls onConnectionChangeUI — which should already be
         null (editor nulled it). This is a defense-in-depth gap: add a null in
         PluginProcessor dtor for safety.
  implication: Minor safety gap

- timestamp: 2026-03-06
  checked: releaseResources — prevIsDawPlaying_, prevLooperWasPlaying_ etc.
  found: Not reset. These are edge-detect flags that govern note-off logic on DAW stop.
         If stale across a prepareToPlay cycle they could suppress the first-block
         edge-detect, causing the DAW-stop allNotesOff sequence to be skipped on first
         block after plugin reload while transport is running.
  implication: GAP — reset these in releaseResources() / prepareToPlay()

## Resolution

root_cause: |
  Two concrete gaps found:
  1. releaseResources() does not reset audio-thread pitch tracking arrays (noteCount_,
     looperActivePitch_, arpActivePitch_, subHeldPitch_, subOctSounding_, lfoRamp*,
     prevFilterX_/Y_, edge-detect booleans). Stale state leaks between
     prepare/release cycles, risking noteCount mismatch and suppressed note-offs on
     the first block after plugin re-arm.
  2. PluginProcessor::~PluginProcessor() nulls onConnectionChange but not
     onConnectionChangeUI — leaving a defense-in-depth gap (the editor destructor
     handles this in normal flow, but an extra null in the processor dtor is safe).

fix: |
  1. PluginProcessor::~PluginProcessor() — added `gamepad_.onConnectionChangeUI = nullptr;`
     alongside the existing `onConnectionChange = nullptr;` null. Defensive guard for
     the case where the editor was never created or was already torn down before the
     processor destructs.

  2. PluginProcessor::releaseResources() — expanded from single `trigger_.resetAllGates()`
     to a full state-reset sweep covering:
     - resetNoteCount() (noteCount_[16][128], subHeldPitch_[4], subOctSounding_[4])
     - looperActivePitch_.fill(-1) and looperActiveSubPitch_ fill
     - arpActivePitch_, arpActiveVoice_, arpNoteOffRemaining_, arpCurrentVoice_
     - lfoXRampOut_, lfoYRampOut_
     - prevCcCut_, prevCcRes_, prevLfoCcX_, prevLfoCcY_ (-1 = force-send)
     - prevFilterX_/Y_, prevBaseX_/Y_, prevAttenX_/Y_ (dedup sentinels)
     - prevIsDawPlaying_, prevLooperWasPlaying_, prevLooperRecording_,
       prevDawPlaying_, prevArpOn_ (transport edge-detect booleans)
     - sampleCounter_, prevBeatCount_ (free-tempo beat detection)
     - lfoInstantCaptureSamplesLeft_, lfoInstantCapture_
     - lfoXMidCycleRec_, lfoYMidCycleRec_

verification: pending build + human verify
files_changed:
  - Source/PluginProcessor.cpp
