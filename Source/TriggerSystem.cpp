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
        case RandomSubdiv::DblWholeT:        return 16.0/3.0;  // 2/1T  = 8 beats × 2/3
        case RandomSubdiv::QuadWholeT:       return 32.0/3.0;  // 4/1T  = 16 beats × 2/3
    }
    return 1.0;
};


void TriggerSystem::processBlock(const ProcessParams& p)
{
    // ── Per-axis delta magnitude for per-voice gate detection ─────────────────
    prevJoystickX_ = p.joystickX;
    prevJoystickY_ = p.joystickY;

    // ── Transport restart detection ───────────────────────────────────────────
    const bool justStarted = p.isDawPlaying && !wasPlaying_;
    wasPlaying_ = p.isDawPlaying;

    if (justStarted)
    {
        for (int v = 0; v < 4; ++v)
            prevSubdivIndex_[v] = -1;
    }

    // ── Population → random subdivision range ────────────────────────────────
    // kPop -1..+1 (0 = center). Center: deterministic, per-voice dropdown value.
    // >0: range expands upward from anchor. <0: range expands downward from anchor.
    // Subdivision is picked randomly at each note-fire (trigger block below).
    const float kPop = (p.randomPopulation - 32.0f) / 32.0f;

    for (int v = 0; v < 4; ++v)
    {
        if (std::abs(kPop) <= 0.02f)
        {
            // At center: always use per-voice dropdown value, reset random-mode flag.
            subdivRandInit_[v] = false;
            const int anchorIdx = juce::jlimit(0, 16, static_cast<int>(p.randomSubdiv[v]));
            const auto newSub = static_cast<RandomSubdiv>(anchorIdx);
            if (newSub != activeSubdiv_[v])
            {
                if (p.randomClockSync && p.ppqPosition >= 0.0)
                {
                    const double newB = subdivBeatsFor(newSub);
                    prevSubdivIndex_[v] = static_cast<int64_t>(p.ppqPosition / newB);
                }
                activeSubdiv_[v] = newSub;
            }
        }
        else if (!subdivRandInit_[v])
        {
            // First block in random mode: seed activeSubdiv_ to anchor so timing
            // is correct before the first note fire triggers the random picker.
            subdivRandInit_[v] = true;
            const int anchorIdx = juce::jlimit(0, 16, static_cast<int>(p.randomSubdiv[v]));
            const auto newSub = static_cast<RandomSubdiv>(anchorIdx);
            if (newSub != activeSubdiv_[v])
            {
                if (p.randomClockSync && p.ppqPosition >= 0.0)
                {
                    const double newB = subdivBeatsFor(newSub);
                    prevSubdivIndex_[v] = static_cast<int64_t>(p.ppqPosition / newB);
                }
                activeSubdiv_[v] = newSub;
            }
        }
        // else: off-center and already seeded — activeSubdiv_[v] is managed at note-fire time.
    }

    // ── Per-voice random subdivision clock ────────────────────────────────────
    const bool allTrig = allTrigger_.exchange(false);

    bool randomFired[4] = {};
    for (int v = 0; v < 4; ++v)
    {
        const double effectiveBpm     = (!p.randomClockSync || !p.isDawPlaying)
                                            ? static_cast<double>(p.randomFreeTempo)
                                            : p.bpm;
        const double beats            = subdivBeatsFor(activeSubdiv_[v]);
        const double beatsPerSec      = effectiveBpm / 60.0;
        const double samplesPerSubdiv = (p.sampleRate / beatsPerSec) * beats;

        if (!p.randomClockSync)
        {
            // RND SYNC OFF: countdown timer fires every effective-subdivision interval.
            // Population has already shifted activeSubdiv_[v] above.
            // randomProbability gates each firing independently.
            randomPhase_[v] -= static_cast<double>(p.blockSize);
            if (randomPhase_[v] <= 0.0)
            {
                if (nextRandom() < p.randomProbability)
                    randomFired[v] = true;

                // Fixed interval: advance by one subdivision period.
                randomPhase_[v] += samplesPerSubdiv;
                if (randomPhase_[v] <= 0.0)  // safety: avoid tight loop on tiny interval
                    randomPhase_[v] = samplesPerSubdiv > 0.0 ? samplesPerSubdiv
                                                             : static_cast<double>(p.blockSize);
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
        if (src != prevSrcV)
        {
            if (gateOpen_[v].load())
                fireNoteOff(v, ch - 1, 0, p);
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
                fireNoteOff(v, ch - 1, 0, p);
            }
        }
        else if (src == TriggerSource::Joystick)
        {
            // Movement-based gate model with optional gate-length timer.
            // Voices 0/1 (Root/Third) use Y delta; voices 2/3 (Fifth/Tension) use X delta.
            const float axisDelta = (v < 2) ? std::abs(p.deltaY) : std::abs(p.deltaX);
            const bool  isMoving  = axisDelta > p.joystickThreshold;
            const int   newPitch  = p.heldPitches[v];

            if (isMoving)
            {
                joystickStillSamples_[v] = 0;

                if (newPitch != joyOpenPitch_[v])
                {
                    joyOpenPitch_[v]     = newPitch;
                    joySettleSamples_[v] = static_cast<int>(0.050 * p.sampleRate);
                }
            }
            else
            {
                joystickStillSamples_[v] += p.blockSize;
                // When gate length > 0, gate-time timer closes the note (see countdown below).
                // When gate length == 0 (manual), use stillness to close.
                const int kClose = static_cast<int>(p.joystickGateTime * p.sampleRate);
                if (p.gateLength <= 0.0f
                    && joystickStillSamples_[v] >= kClose
                    && joySettleSamples_[v] <= 0
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

                        // Start gate-length countdown when gateLength > 0.
                        if (p.gateLength > 0.0f)
                        {
                            const double beats = subdivBeatsFor(activeSubdiv_[v]);
                            const double bps   = p.bpm / 60.0;
                            const int    minD  = static_cast<int>(0.010 * p.sampleRate);
                            randomGateRemaining_[v] = std::max(minD,
                                static_cast<int>(p.gateLength * (p.sampleRate / bps) * beats));
                        }
                    }
                    joystickStillSamples_[v] = 0;
                }
            }

            trigger = false;
        }
        else if (src == TriggerSource::RandomFree)
        {
            if (randomFired[v])
            {
                if (!p.randomClockSync)
                {
                    // SYNC OFF: Poisson interval already controls timing — trigger unconditionally
                    trigger = true;
                }
                else
                {
                    // SYNC ON: per-slot gate using randomProbability
                    if (nextRandom() < p.randomProbability)
                        trigger = true;
                    // activeSubdiv_[v] is already set from the population offset above.
                }
            }
        }
        else if (src == TriggerSource::RandomHold)
        {
            const bool padHeld = padPressed_[v].load();
            padJustFired_[v].exchange(false);

            if (!padHeld && gateOpen_[v].load())
            {
                fireNoteOff(v, ch - 1, 0, p);
                randomGateRemaining_[v] = 0;
            }
            else if (padHeld && randomFired[v])
            {
                if (!p.randomClockSync)
                {
                    trigger = true;
                }
                else
                {
                    if (nextRandom() < p.randomProbability)
                        trigger = true;
                    // activeSubdiv_[v] is already set from the population offset above.
                }
            }
        }

        if (trigger)
        {
            if (src == TriggerSource::RandomFree || src == TriggerSource::RandomHold)
            {
                if (gateOpen_[v].load())
                    fireNoteOff(v, ch - 1, 0, p);
                fireNoteOn(v, p.heldPitches[v], ch - 1, 0, p);

                // Pick random subdivision for the NEXT interval (weighted toward anchor).
                // Squared distribution: lower steps (near anchor) are more likely.
                if (std::abs(kPop) > 0.02f)
                {
                    const int anchorIdx = juce::jlimit(0, 16, static_cast<int>(p.randomSubdiv[v]));
                    const int maxRange  = kPop > 0.0f
                        ? static_cast<int>(std::round( kPop * (16.0f - anchorIdx)))
                        : static_cast<int>(std::round(-kPop *  static_cast<float>(anchorIdx)));
                    int newIdx = anchorIdx;
                    if (maxRange > 0)
                    {
                        const float r    = nextRandom();
                        const int   step = static_cast<int>(maxRange * r * r);
                        newIdx = anchorIdx + (kPop > 0.0f ? step : -step);
                    }
                    const auto newSub = static_cast<RandomSubdiv>(juce::jlimit(0, 16, newIdx));
                    if (newSub != activeSubdiv_[v])
                    {
                        activeSubdiv_[v] = newSub;
                        if (p.randomClockSync && p.ppqPosition >= 0.0)
                        {
                            const double newB = subdivBeatsFor(newSub);
                            prevSubdivIndex_[v] = static_cast<int64_t>(p.ppqPosition / newB);
                        }
                    }
                }

                if (p.gateLength <= 0.0f)
                {
                    randomGateRemaining_[v] = -1;
                }
                else
                {
                    const double beats            = subdivBeatsFor(activeSubdiv_[v]);
                    const double beatsPerSec      = p.bpm / 60.0;
                    const double samplesPerSubdiv = (p.sampleRate / beatsPerSec) * beats;
                    const int minDurationSamples  = static_cast<int>(0.010 * p.sampleRate);
                    randomGateRemaining_[v] = std::max(minDurationSamples,
                        static_cast<int>(p.gateLength * samplesPerSubdiv));
                }
            }
            else
            {
                if (gateOpen_[v].load())
                    fireNoteOff(v, ch - 1, 0, p);
                fireNoteOn(v, p.heldPitches[v], ch - 1, 0, p);
            }
        }

        // Auto note-off countdown for RandomFree, RandomHold, and Joystick (gate-length mode).
        // The -1 sentinel (manual open gate) is correctly skipped by the > 0 guard.
        if ((src == TriggerSource::RandomFree || src == TriggerSource::RandomHold
                || src == TriggerSource::Joystick)
            && gateOpen_[v].load() && randomGateRemaining_[v] > 0)
        {
            randomGateRemaining_[v] -= p.blockSize;
            if (randomGateRemaining_[v] <= 0)
            {
                randomGateRemaining_[v] = 0;
                fireNoteOff(v, ch - 1, p.blockSize - 1, p);
                if (src == TriggerSource::Joystick)
                {
                    joyActivePitch_[v] = -1;
                    joyOpenPitch_[v]   = -1;
                }
            }
        }
        // Clear gate countdown when source is not an auto-gate type.
        if (src == TriggerSource::TouchPlate)
            randomGateRemaining_[v] = 0;

        prevSrc_[v] = src;
    }

    joystickTrig_.store(false);
}

void TriggerSystem::resetAllGates()
{
    for (int v = 0; v < 4; ++v)
    {
        gateOpen_[v].store(false);
        activePitch_[v]          = -1;
        joyActivePitch_[v]       = -1;
        joyOpenPitch_[v]         = -1;
        joySettleSamples_[v]     = 0;
        joyLastBend_[v]          = 0;
        joystickStillSamples_[v] = 0;
        padJustFired_[v].store(false);

        randomPhase_[v]         = 0.0;
        prevSubdivIndex_[v]     = -1;
        randomGateRemaining_[v] = 0;
        activeSubdiv_[v]        = RandomSubdiv::Quarter;
        prevSrc_[v]             = TriggerSource::TouchPlate;
    }
    allTrigger_.store(false);
    joystickTrig_.store(false);
    wasPlaying_ = false;
}

void TriggerSystem::syncRandomFreePhases()
{
    // Reset free-running phase counters so all voices re-align from the same position.
    // Does not close open gates or disturb non-random sources.
    for (int v = 0; v < 4; ++v)
    {
        randomPhase_[v]     = 0.0;
        prevSubdivIndex_[v] = -1;
    }
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
