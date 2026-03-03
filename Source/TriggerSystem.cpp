#include "TriggerSystem.h"
#include <cmath>

TriggerSystem::TriggerSystem()
{
    for (auto& a : padPressed_)   a.store(false);
    for (auto& a : padJustFired_) a.store(false);
    for (auto& a : gateOpen_)     a.store(false);
    joyActivePitch_.fill(-1);
    joystickStillSamples_.fill(0);
    joyOpenPitch_.fill(-1);
    joyLastBend_.fill(0);
}

// ── UI / gamepad thread ───────────────────────────────────────────────────────

void TriggerSystem::setPadState(int voice, bool pressed)
{
    if (voice < 0 || voice >= 4) return;

    const bool wasPressed = padPressed_[voice].exchange(pressed);
    if (pressed && !wasPressed)
        padJustFired_[voice].store(true);  // rising edge
}

void TriggerSystem::notifyJoystickMoved()
{
    joystickTrig_.store(true);
}

void TriggerSystem::triggerAllNotes()
{
    allTrigger_.store(true);
}

// ── Audio thread ──────────────────────────────────────────────────────────────

// Subdivision helper lambdas (file-scope statics, computed once)
static auto subdivBeatsFor = [](RandomSubdiv s) -> double {
    switch (s) {
        case RandomSubdiv::QuadWhole:        return 16.0;
        case RandomSubdiv::DblWhole:         return 8.0;
        case RandomSubdiv::Whole:            return 4.0;
        case RandomSubdiv::Half:             return 2.0;
        case RandomSubdiv::Quarter:          return 1.0;
        case RandomSubdiv::Eighth:           return 0.5;
        case RandomSubdiv::Sixteenth:        return 0.25;
        case RandomSubdiv::ThirtySecond:     return 0.125;
        case RandomSubdiv::SixtyFourth:      return 0.0625;
        case RandomSubdiv::WholeT:           return 8.0/3.0;   // 1/1T  = 4 beats × 2/3
        case RandomSubdiv::HalfT:            return 4.0/3.0;   // 1/2T  = 2 beats × 2/3
        case RandomSubdiv::QuarterT:         return 2.0/3.0;   // 1/4T  = 1 beat  × 2/3
        case RandomSubdiv::EighthT:          return 1.0/3.0;   // 1/8T  = 0.5 × 2/3
        case RandomSubdiv::SixteenthT:       return 1.0/6.0;   // 1/16T = 0.25 × 2/3
        case RandomSubdiv::ThirtySecondT:    return 1.0/12.0;  // 1/32T = 0.125 × 2/3
    }
    return 1.0;
};

static float hitsPerBarToProbability(float population, RandomSubdiv subdiv)
{
    float subdivsPerBar;
    switch (subdiv) {
        case RandomSubdiv::QuadWhole:        subdivsPerBar = 0.25f; break;
        case RandomSubdiv::DblWhole:         subdivsPerBar = 0.5f;  break;
        case RandomSubdiv::Whole:            subdivsPerBar = 1.0f;  break;
        case RandomSubdiv::Half:             subdivsPerBar = 2.0f;  break;
        case RandomSubdiv::Quarter:          subdivsPerBar = 4.0f;  break;
        case RandomSubdiv::Eighth:           subdivsPerBar = 8.0f;  break;
        case RandomSubdiv::Sixteenth:        subdivsPerBar = 16.0f; break;
        case RandomSubdiv::ThirtySecond:     subdivsPerBar = 32.0f; break;
        case RandomSubdiv::WholeT:           subdivsPerBar = 1.5f;  break;  // 4 / (8/3)
        case RandomSubdiv::HalfT:            subdivsPerBar = 3.0f;  break;  // 4 / (4/3)
        case RandomSubdiv::QuarterT:         subdivsPerBar = 6.0f;  break;  // 4 / (2/3)
        case RandomSubdiv::EighthT:          subdivsPerBar = 12.0f; break;  // 4 / (1/3)
        case RandomSubdiv::SixteenthT:       subdivsPerBar = 24.0f; break;  // 4 / (1/6)
        case RandomSubdiv::ThirtySecondT:    subdivsPerBar = 48.0f; break;  // 4 / (1/12)
        default:                             subdivsPerBar = 64.0f; break;  // SixtyFourth
    }
    return juce::jlimit(0.0f, 1.0f, population / subdivsPerBar);
}

void TriggerSystem::processBlock(const ProcessParams& p)
{
    // ── Per-axis delta magnitude for per-voice gate detection ─────────────────
    // Gate detection uses how much the joystick MOVED this block (delta),
    // not its absolute position.  A joystick resting at Y=-1.0 has absY=1.0
    // which would permanently exceed any threshold; delta is 0 when still.
    //
    // Voices 0+1 (Root/Third) → Y-axis delta
    // Voices 2+3 (Fifth/Tension) → X-axis delta
    //
    // prevJoystickX_/Y_ are kept in sync here so legacy callers still compile,
    // but the JOY gate now uses the pre-computed deltaX/deltaY from ProcessParams.
    prevJoystickX_ = p.joystickX;
    prevJoystickY_ = p.joystickY;

    // ── Transport restart detection ───────────────────────────────────────────
    // Reset subdivision indices on transport restart to avoid stale index
    const bool justStarted = p.isDawPlaying && !wasPlaying_;
    const bool justStopped = !p.isDawPlaying && wasPlaying_;
    wasPlaying_ = p.isDawPlaying;

    if (justStarted)
    {
        for (int v = 0; v < 4; ++v)
            prevSubdivIndex_[v] = -1;
    }
    if (justStopped)
    {
        // On transport stop, drain any in-progress bursts so notes do not fire post-stop
        for (int v = 0; v < 4; ++v)
        {
            burstNotesRemaining_[v] = 0;
            burstPhase_[v]          = 0.0;
        }
    }

    // ── Per-voice random subdivision clock ────────────────────────────────────
    const bool allTrig = allTrigger_.exchange(false);

    bool randomFired[4] = {};
    for (int v = 0; v < 4; ++v)
    {
        // effectiveBpm: use randomFreeTempo when DAW is not playing or SYNC is off.
        // When SYNC ON + DAW playing, use DAW BPM for samplesPerSubdiv (ppq path doesn't
        // use it, but kept consistent). When SYNC ON + DAW stopped, use randomFreeTempo
        // for the internal-clock fallback.
        const double effectiveBpm     = (!p.randomClockSync || !p.isDawPlaying)
                                            ? static_cast<double>(p.randomFreeTempo)
                                            : p.bpm;
        const double beats            = subdivBeatsFor(p.randomSubdiv[v]);
        const double beatsPerSec      = effectiveBpm / 60.0;
        const double samplesPerSubdiv = (p.sampleRate / beatsPerSec) * beats;

        if (!p.randomClockSync)
        {
            // RND SYNC OFF: Poisson countdown — truly stochastic inter-onset intervals.
            // randomPhase_[v] counts DOWN toward 0. When it reaches/passes 0, fire burst
            // and draw a new exponential wait from the same LCG.
            randomPhase_[v] -= static_cast<double>(p.blockSize);
            if (randomPhase_[v] <= 0.0)
            {
                randomFired[v] = true;
                // Draw new Poisson wait (exponential inter-arrival, rate = population events/bar)
                const double spBar  = (p.sampleRate * 60.0 / static_cast<double>(p.randomFreeTempo)) * 4.0;
                const double mean   = spBar / static_cast<double>(std::max(1.0f, p.randomPopulation));
                const float  u      = std::max(1e-6f, nextRandom());
                const float  drawn  = static_cast<float>(-mean * std::log(static_cast<double>(u)));
                const float  minFlr = static_cast<float>(0.010 * p.sampleRate);
                randomPhase_[v] = static_cast<double>(std::max(minFlr, drawn));
            }
        }
        else if (p.isDawPlaying && p.ppqPosition >= 0.0)
        {
            // RND SYNC ON + DAW playing: fire on ppq subdivision boundary.
            const int64_t idx = static_cast<int64_t>(p.ppqPosition / beats);
            if (idx != prevSubdivIndex_[v])
            {
                prevSubdivIndex_[v] = idx;
                randomFired[v] = true;
            }
        }
        else
        {
            // RND SYNC ON + DAW stopped (or ppq unavailable): internal free-tempo sample counter.
            randomPhase_[v] += static_cast<double>(p.blockSize);
            if (samplesPerSubdiv > 0.0 && randomPhase_[v] >= samplesPerSubdiv)
            {
                randomPhase_[v] = std::fmod(randomPhase_[v], samplesPerSubdiv);
                randomFired[v] = true;
            }
        }
    }

    // ── Per-voice processing ─────────────────────────────────────────────────
    for (int v = 0; v < 4; ++v)
    {
        const int           ch       = p.midiChannels[v];   // 1-based (JUCE MidiMessage convention)
        const TriggerSource src      = p.sources[v];
        const TriggerSource prevSrcV = prevSrc_[v];
        bool trigger = false;

        // Universal mode-switch: close any open gate the moment the source changes.
        // Covers all transitions (non-random→random, random→non-random, joy→pad, etc.)
        // so stale gates never outlive the mode they were opened in.
        if (src != prevSrcV)
        {
            if (gateOpen_[v].load())
                fireNoteOff(v, ch - 1, 0, p);
            burstNotesRemaining_[v] = 0;
            burstPhase_[v]          = 0.0;
        }

        if (allTrig)
        {
            trigger = true;
        }
        else if (src == TriggerSource::TouchPlate)
        {
            const bool pressed   = padPressed_[v].load();
            const bool justFired = padJustFired_[v].exchange(false);

            if (justFired)
            {
                trigger = true;
            }
            else if (!pressed && gateOpen_[v].load())
            {
                // Released → note off
                fireNoteOff(v, ch - 1, 0, p);   // fireNoteOff expects 0-based ch
            }
        }
        else if (src == TriggerSource::Joystick)
        {
            // Movement-based gate model:
            //   Movement (|delta| > threshold) → start/reset 50ms settle timer.
            //   Settle timer expires → note fires at the pitch that has been
            //     stable for 50ms (fast sweeps only sound the landing pitch).
            //   Stillness (no movement for 60ms) → gate closes.
            //   Jitter below threshold is ignored; joystickThreshold controls
            //     both the movement sensitivity and the settle dead band.
            //
            // Voices 0/1 (Root/Third) use Y delta; voices 2/3 (Fifth/Tension) use X delta.
            const float axisDelta = (v < 2) ? std::abs(p.deltaY) : std::abs(p.deltaX);
            const bool  isMoving  = axisDelta > p.joystickThreshold;
            const int   newPitch  = p.heldPitches[v];

            if (isMoving)
            {
                // Movement detected — reset stillness counter.
                joystickStillSamples_[v] = 0;

                // If pitch changed, reset the settle timer so fast sweeps don't
                // produce notes at every intermediate pitch step.
                if (newPitch != joyOpenPitch_[v])
                {
                    joyOpenPitch_[v]     = newPitch;
                    joySettleSamples_[v] = static_cast<int>(0.050 * p.sampleRate);
                }
            }
            else
            {
                // No movement — accumulate stillness; close gate after joystickGateTime seconds.
                joystickStillSamples_[v] += p.blockSize;
                const int kClose = static_cast<int>(p.joystickGateTime * p.sampleRate);
                if (joystickStillSamples_[v] >= kClose
                    && joySettleSamples_[v] <= 0   // don't interrupt a pending settle
                    && gateOpen_[v].load())
                {
                    fireNoteOff(v, ch - 1, 0, p);
                    joyActivePitch_[v] = -1;
                    joyOpenPitch_[v]   = -1;
                }
            }

            // Settle countdown: fire the pending pitch once it has been stable.
            if (joySettleSamples_[v] > 0)
            {
                joySettleSamples_[v] -= p.blockSize;
                if (joySettleSamples_[v] <= 0)
                {
                    joySettleSamples_[v] = 0;
                    const int pitch = joyOpenPitch_[v];
                    if (pitch >= 0 && pitch != joyActivePitch_[v])
                    {
                        if (gateOpen_[v].load())
                            fireNoteOff(v, ch - 1, 0, p);
                        fireNoteOn(v, pitch, ch - 1, 0, p);
                        joyActivePitch_[v] = pitch;
                    }
                    // Reset stillness so the freshly-fired note has time to play
                    // before the 60 ms gate-close window starts.
                    joystickStillSamples_[v] = 0;
                }
            }

            // Skip the common trigger path — JOY source handled inline above.
            trigger = false;
        }
        else if (src == TriggerSource::RandomFree)
        {
            // On new burst trigger: compute burst size from Probability and start burst countdown.
            if (randomFired[v])
            {
                // SYNC ON: apply population slot-selection probability to gate this event.
                // SYNC OFF: burst rate is already controlled by Poisson wait in clock block;
                //           no additional slot gating — population is the λ rate.
                bool slotFires = true;
                if (p.randomClockSync)
                {
                    const float slotProb = hitsPerBarToProbability(p.randomPopulation, p.randomSubdiv[v]);
                    slotFires = (nextRandom() < slotProb);
                }

                if (slotFires)
                {
                    const int burstSize = static_cast<int>(std::round(p.randomProbability * 64.0f));
                    if (burstSize > 0)
                    {
                        burstNotesRemaining_[v] = burstSize;
                        burstPhase_[v]          = 0.0;  // fire first note immediately this block
                    }
                }
            }

            // Drain burst: fire one note per block, spaced by subdivision samples.
            if (burstNotesRemaining_[v] > 0)
            {
                burstPhase_[v] -= static_cast<double>(p.blockSize);
                if (burstPhase_[v] <= 0.0)
                {
                    trigger = true;
                    burstNotesRemaining_[v]--;
                    if (burstNotesRemaining_[v] > 0)
                    {
                        // Schedule next note in burst at subdivision spacing.
                        const double effBpm2   = (!p.randomClockSync || !p.isDawPlaying)
                                                   ? static_cast<double>(p.randomFreeTempo)
                                                   : p.bpm;
                        const double beats2    = subdivBeatsFor(p.randomSubdiv[v]);
                        const double spSub2    = (p.sampleRate / (effBpm2 / 60.0)) * beats2;
                        burstPhase_[v] = std::max(spSub2, static_cast<double>(0.010 * p.sampleRate));
                    }
                }
            }
        }
        else if (src == TriggerSource::RandomHold)
        {
            const bool padHeld = padPressed_[v].load();
            padJustFired_[v].exchange(false);  // consume edge flag — RandomHold does not use it

            // Pad released mid-note → hard cut, drain burst
            if (!padHeld && gateOpen_[v].load())
            {
                fireNoteOff(v, ch - 1, 0, p);
                randomGateRemaining_[v]  = 0;
                burstNotesRemaining_[v]  = 0;
                burstPhase_[v]           = 0.0;
            }
            // Pad held → random clock gates through with burst semantics (same as RandomFree)
            else if (padHeld && randomFired[v])
            {
                bool slotFires = true;
                if (p.randomClockSync)
                {
                    const float slotProb = hitsPerBarToProbability(p.randomPopulation, p.randomSubdiv[v]);
                    slotFires = (nextRandom() < slotProb);
                }
                if (slotFires)
                {
                    const int burstSize = static_cast<int>(std::round(p.randomProbability * 64.0f));
                    if (burstSize > 0)
                    {
                        burstNotesRemaining_[v] = burstSize;
                        burstPhase_[v]          = 0.0;
                    }
                }
            }

            // Drain burst (same pattern as RandomFree; only when pad is held)
            if (padHeld && burstNotesRemaining_[v] > 0)
            {
                burstPhase_[v] -= static_cast<double>(p.blockSize);
                if (burstPhase_[v] <= 0.0)
                {
                    trigger = true;
                    burstNotesRemaining_[v]--;
                    if (burstNotesRemaining_[v] > 0)
                    {
                        const double effBpm2  = (!p.randomClockSync || !p.isDawPlaying)
                                                  ? static_cast<double>(p.randomFreeTempo)
                                                  : p.bpm;
                        const double beats2   = subdivBeatsFor(p.randomSubdiv[v]);
                        const double spSub2   = (p.sampleRate / (effBpm2 / 60.0)) * beats2;
                        burstPhase_[v] = std::max(spSub2, static_cast<double>(0.010 * p.sampleRate));
                    }
                }
            }
            // Pad released mid-burst already handled above (burst drained on release)
        }

        if (trigger)
        {
            if (src == TriggerSource::RandomFree || src == TriggerSource::RandomHold)
            {
                // Auto-trigger source: fire note-on and start gate-time countdown
                if (gateOpen_[v].load())
                    fireNoteOff(v, ch - 1, 0, p);
                fireNoteOn(v, p.heldPitches[v], ch - 1, 0, p);

                if (p.gateLength <= 0.0f)
                {
                    // Manual mode: open gate, no timer — next trigger will close it via fireNoteOff at note-on
                    randomGateRemaining_[v] = -1;
                }
                else
                {
                    const double beats            = subdivBeatsFor(p.randomSubdiv[v]);
                    const double beatsPerSec      = p.bpm / 60.0;
                    const double samplesPerSubdiv = (p.sampleRate / beatsPerSec) * beats;
                    const int minDurationSamples  = static_cast<int>(0.010 * p.sampleRate); // 10ms floor
                    randomGateRemaining_[v] = std::max(minDurationSamples,
                        static_cast<int>(p.gateLength * samplesPerSubdiv));
                }
            }
            else
            {
                if (gateOpen_[v].load())
                    fireNoteOff(v, ch - 1, 0, p);   // stop previous note first
                fireNoteOn(v, p.heldPitches[v], ch - 1, 0, p);
            }
        }

        // Auto note-off countdown for RandomFree / RandomHold sources
        // The -1 sentinel (manual open gate) is correctly skipped by the > 0 guard.
        if ((src == TriggerSource::RandomFree || src == TriggerSource::RandomHold)
            && gateOpen_[v].load() && randomGateRemaining_[v] > 0)
        {
            randomGateRemaining_[v] -= p.blockSize;
            if (randomGateRemaining_[v] <= 0)
            {
                randomGateRemaining_[v] = 0;
                fireNoteOff(v, ch - 1, p.blockSize - 1, p);
            }
        }
        // Clear random gate countdown when source is not a random type.
        // The universal mode-switch noteOff above already closed any open gate on transition.
        if (src != TriggerSource::RandomFree && src != TriggerSource::RandomHold)
            randomGateRemaining_[v] = 0;
        prevSrc_[v] = src;
    }

    // joystickTrig_ is kept for external callers (notifyJoystickMoved) but the
    // continuous gate model in the per-voice loop no longer uses it.
    joystickTrig_.store(false);
}

void TriggerSystem::resetAllGates()
{
    for (int v = 0; v < 4; ++v)
    {
        // Any open JOY gate is silenced here.
        // Note: the caller (processBlockBypassed) fires noteOff via getActivePitch()
        // before calling this, so no MIDI is sent here.
        gateOpen_[v].store(false);
        activePitch_[v]          = -1;
        joyActivePitch_[v]       = -1;
        joyOpenPitch_[v]         = -1;
        joySettleSamples_[v]     = 0;
        joyLastBend_[v]          = 0;
        joystickStillSamples_[v] = 0;
        // NOTE: padPressed_ is intentionally NOT reset here.
        // The physical pad state is owned by the UI/gamepad thread; resetting it
        // would cause setPadState() to detect a false rising edge (re-triggering a
        // note) the next time it is called while the pad is still held.
        padJustFired_[v].store(false);

        // Per-voice random clock state
        randomPhase_[v]         = 0.0;
        prevSubdivIndex_[v]     = -1;
        randomGateRemaining_[v] = 0;
        burstNotesRemaining_[v] = 0;
        burstPhase_[v]          = 0.0;
        prevSrc_[v]             = TriggerSource::TouchPlate;

    }
    allTrigger_.store(false);
    joystickTrig_.store(false);
    wasPlaying_ = false;
}

void TriggerSystem::fireNoteOn(int voice, int pitch, int ch, int sampleOff,
                                const ProcessParams& p)
{
    if (pitch < 0 || pitch > 127) return;
    activePitch_[voice] = pitch;
    gateOpen_[voice].store(true);
    p.onNote(voice, pitch, true, sampleOff);
}

void TriggerSystem::fireNoteOff(int voice, int ch, int sampleOff,
                                 const ProcessParams& p)
{
    const int pitch = activePitch_[voice];
    if (pitch < 0) return;
    gateOpen_[voice].store(false);
    activePitch_[voice] = -1;
    p.onNote(voice, pitch, false, sampleOff);
}
