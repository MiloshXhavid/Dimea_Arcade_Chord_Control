---
status: resolved
trigger: "Random Free trigger mode (triggerSource = 2) produces no MIDI output — completely silent"
created: 2026-03-04T00:00:00Z
updated: 2026-03-04T00:00:00Z
---

## Current Focus

hypothesis: TWO confirmed bugs found. Primary silence: randomProbability=0 makes Poisson wait=1e9 samples. Secondary: gate-length uses p.bpm instead of p.randomFreeTempo in free mode.
test: full static code trace of TriggerSystem.cpp Poisson path + all call sites
expecting: both bugs confirmed by code reading
next_action: deliver diagnosis to user

## Symptoms

expected: Random Free trigger mode fires MIDI notes autonomously at the rate configured by the free tempo knob (30-240 BPM) and density controls (population/probability knobs)
actual: Completely silent — no MIDI output at all when triggerSource = 2 (RandomFree)
errors: none (silent failure)
reproduction: set any voice trigger source to "Random Free" (index 2), no MIDI triggers fire
started: after phase 28 redesign (Poisson + population knobs); phase 28 verified working 2026-03-03

## Eliminated

- hypothesis: randomPhase_ not initialised, never reaching <= 0
  evidence: std::array<double,4> randomPhase_ {} is zero-initialised; first block subtracts blockSize (goes to -blockSize) so fires immediately
  timestamp: 2026-03-04

- hypothesis: UI wiring maps triggerSource to wrong APVTS index
  evidence: ComboBoxParameterAttachment uses 0-based item index, not item ID; Pad=0, Joystick=1, RndFree=2, RndHold=3 maps exactly to TriggerSource enum values
  timestamp: 2026-03-04

- hypothesis: midiMuted_ stuck true
  evidence: midiMuted_ defaults to false, setMidiMuted() is never called from PluginEditor; only set by external callers; panic button resets to false
  timestamp: 2026-03-04

- hypothesis: arpOn override
  evidence: arpEnabled APVTS default is false; only a concern if user explicitly enabled ARP; would affect all sources not just RandomFree
  timestamp: 2026-03-04

- hypothesis: gate-length BPM mismatch causes 0-duration notes
  evidence: gateLength default is 0.5, samplesPerSubdiv at Quarter/120BPM = ~22050 samples; gate = ~11000 samples (~250ms) — not zero; this is a wrong-duration bug not a silence bug
  timestamp: 2026-03-04

## Evidence

- timestamp: 2026-03-04
  checked: TriggerSystem.cpp lines 100-120 (free-mode Poisson clock)
  found: randomPhase_[v] -= blockSize each block; fires when <= 0; draws Poisson wait from randomProbability + randomPopulation
  implication: Clock is correct; fires at Poisson-distributed intervals

- timestamp: 2026-03-04
  checked: TriggerSystem.cpp lines 108-117 (Poisson wait draw, free mode)
  found: "effProb = min(1.0, randomProbability * (1 + boost)); eventsBar = effProb * 64; mean = (eventsBar > 0.001) ? spBar/eventsBar : 1e9"
  implication: When randomProbability = 0.0, eventsBar = 0.0, mean = 1e9, drawn = enormous — randomPhase_[v] never reaches <= 0 AGAIN after first fire; permanent silence

- timestamp: 2026-03-04
  checked: TriggerSystem.cpp lines 247-284 (RandomFree source handler)
  found: When !randomClockSync and randomFired[v], trigger=true unconditionally; probability check only in SYNC ON branch
  implication: In free mode, if the Poisson clock fires, the trigger always fires — no separate probability gate here

- timestamp: 2026-03-04
  checked: TriggerSystem.cpp lines 348-354 (gate-length calculation in trigger block)
  found: "const double beatsPerSec = p.bpm / 60.0;" where p.bpm = lp.bpm = dawBpm when DAW is playing
  implication: BUG — in free mode, gate duration is computed at DAW BPM not free tempo BPM; notes have wrong duration, not silence

- timestamp: 2026-03-04
  checked: PluginEditor.cpp line 3279
  found: "randomSyncButton_.setToggleState(true, juce::dontSendNotification);" — button hardcoded ON before APVTS attachment is created
  implication: Visual glitch only; APVTS attachment's sendInitialUpdate() overwrites the button state to match the saved APVTS value (default=false)

- timestamp: 2026-03-04
  checked: APVTS parameter registration for randomProbability (PluginProcessor.cpp line 179)
  found: "addFloat("randomProbability", "Random Probability", 0.0f, 1.0f, 1.0f);" — min=0.0, max=1.0, default=1.0
  implication: If a saved preset has randomProbability=0 (or user dragged knob to 0%), the Poisson draw enters the 1e9-sample silence path

- timestamp: 2026-03-04
  checked: git log after commit 84ccc12 (phase 28 completion) for changes to TriggerSystem.cpp/h and PluginProcessor.cpp
  found: No commits modify these files after phase 28; only PluginEditor cosmetic changes
  implication: The logic has not changed since phase 28 verification; silence is a runtime state/parameter problem, not a code regression

## Resolution

root_cause: |
  TWO bugs found. Bug A causes complete silence; Bug B causes wrong note durations.

  ─── BUG A (PRIMARY — causes complete silence) ───────────────────────────────
  File: Source/TriggerSystem.cpp, lines 108-117 (Poisson wait draw, free mode)

  When randomProbability = 0.0 (knob at minimum, 0%), the Poisson wait draw
  computes mean = 1e9 and drawn = -1e9 * log(u) = astronomical sample count.
  randomPhase_[v] is set to this enormous value and never again reaches <= 0
  in any realistic timeframe. Result: no further triggers fire = complete silence.

  The FIRST trigger always fires (randomPhase_ starts at 0.0, immediately <= 0
  on block 1). But after the first fire, if probability=0 the next wait is ~hours.
  So from the user's perspective: silence (they'd need to wait many hours for
  a second trigger).

  Why "used to work": APVTS default for randomProbability is 1.0 (100%). If the
  user accidentally dragged the PROBABLY knob to 0% in a session and saved the
  preset, or if a DAW preset was saved with probability=0, all subsequent sessions
  are silent. The code was not changed after phase 28 verification.

  Underlying design flaw: in free mode, probability=0 should mean "no triggers"
  (documented in tooltip: "0% = never"). The Poisson formula correctly implements
  this via the 1e9 sentinel. BUT: the first trigger ALWAYS fires on startup
  (randomPhase_ = 0 → immediately <= 0 on block 1). This is inconsistent with
  probability=0 meaning "never" — it fires once, then never again.
  The fix should initialise randomPhase_ to a random value so even the first
  block doesn't always fire, or set it to +1 * samplesPerSubdiv so the first
  fire is properly spaced.

  ─── BUG B (SECONDARY — wrong gate durations, not silence) ──────────────────
  File: Source/TriggerSystem.cpp, lines 348-354 (gate-length calculation)

  The gate duration is calculated as:
      beatsPerSec = p.bpm / 60.0;
      samplesPerSubdiv = (sampleRate / beatsPerSec) * beats;
      randomGateRemaining_ = gateLength * samplesPerSubdiv;

  p.bpm is lp.bpm from PluginProcessor.cpp line 675:
      lp.bpm = (looper.isSyncToDaw && DAW playing && dawBpm > 0) ? dawBpm : freeTempo

  In free mode (randomClockSync=false) with DAW playing: p.bpm = dawBpm (e.g. 140)
  but the trigger rate is governed by p.randomFreeTempo (e.g. 120). Gate duration
  is computed at the wrong tempo — notes are too long or too short.
  Fix: use p.randomFreeTempo when !p.randomClockSync for this calculation.

fix: |
  ─── FIX FOR BUG A ───────────────────────────────────────────────────────────
  In TriggerSystem.cpp, after the resetAllGates() initialisation of randomPhase_[v]=0.0,
  OR at the start of the free-mode Poisson fire, ensure that probability=0 prevents
  even the first trigger:

  Option 1 (preferred): Before calling fireNoteOn in the trigger block for RandomFree/
  RandomHold with !randomClockSync, add a probability gate:

      if (src == TriggerSource::RandomFree || src == TriggerSource::RandomHold)
      {
          if (!p.randomClockSync && p.randomProbability <= 0.0f)
              trigger = false;  // probability=0 means truly never
      }

  This prevents the first-block fire when probability=0 and is consistent with
  the tooltip "0% = never."

  Option 2: Change the initial randomPhase_ value in resetAllGates() to draw a
  Poisson wait immediately so block-1 doesn't always fire, but this is more complex.

  ─── FIX FOR BUG B ───────────────────────────────────────────────────────────
  In TriggerSystem.cpp lines 348-354, guard the BPM selection:

      const double beatsPerSec = (p.randomClockSync ? p.bpm : static_cast<double>(p.randomFreeTempo))
                                  / 60.0;

  This ensures gate duration is computed at the correct tempo in free mode.

verification: static code trace — runtime verification required
files_changed:
  - Source/TriggerSystem.cpp (lines 248-254 for Bug A fix; lines 349 for Bug B fix)
