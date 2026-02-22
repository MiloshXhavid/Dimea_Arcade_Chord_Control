#include "TriggerSystem.h"
#include <cmath>

TriggerSystem::TriggerSystem()
{
    for (auto& a : padPressed_)   a.store(false);
    for (auto& a : padJustFired_) a.store(false);
    for (auto& a : gateOpen_)     a.store(false);
    joyActivePitch_.fill(-1);
    joystickStillSamples_.fill(0);
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

static double subdivisionBeats(RandomSubdiv s)
{
    switch (s)
    {
        case RandomSubdiv::Quarter:      return 1.0;
        case RandomSubdiv::Eighth:       return 0.5;
        case RandomSubdiv::Sixteenth:    return 0.25;
        case RandomSubdiv::ThirtySecond: return 0.125;
    }
    return 0.5;
}

void TriggerSystem::processBlock(const ProcessParams& p)
{
    const double samplesPerBeat   = (p.sampleRate * 60.0) / p.bpm;
    const double subdivBeats      = subdivisionBeats(p.randomSubdiv);
    const double samplesPerSubdiv = samplesPerBeat * subdivBeats;

    // ── Joystick magnitude (Chebyshev distance of per-block delta) ────────────
    const float dx = p.joystickX - static_cast<float>(prevJoystickX_);
    const float dy = p.joystickY - static_cast<float>(prevJoystickY_);
    prevJoystickX_ = p.joystickX;
    prevJoystickY_ = p.joystickY;
    const float magnitude         = std::max(std::abs(dx), std::abs(dy));
    const bool  joyAboveThreshold = magnitude > p.joystickThreshold;

    // ── Per-sample random clock ───────────────────────────────────────────────
    const bool allTrig = allTrigger_.exchange(false);

    bool randomFired = false;
    randomPhase_ += static_cast<double>(p.blockSize);
    if (randomPhase_ >= samplesPerSubdiv && samplesPerSubdiv > 0.0)
    {
        randomPhase_ = std::fmod(randomPhase_, samplesPerSubdiv);
        randomFired = true;
    }

    // ── Per-voice processing ─────────────────────────────────────────────────
    for (int v = 0; v < 4; ++v)
    {
        const int         ch  = p.midiChannels[v];   // 1-based (JUCE MidiMessage convention)
        const TriggerSource src = p.sources[v];
        bool trigger = false;

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
            if (joyAboveThreshold)
            {
                // Joystick is moving — reset stillness counter.
                joystickStillSamples_[v] = 0;

                if (!gateOpen_[v].load())
                {
                    // ── Gate OPENS ────────────────────────────────────────────
                    const int pitch = p.heldPitches[v];
                    fireNoteOn(v, pitch, ch - 1, 0, p);   // sets gateOpen_ and activePitch_
                    joyActivePitch_[v] = pitch;
                }
                else
                {
                    // ── Gate OPEN + above threshold (joystick moving) ─────────
                    const int newPitch = p.heldPitches[v];
                    if (newPitch != joyActivePitch_[v])
                    {
                        fireNoteOff(v, ch - 1, 0, p);     // old pitch off first
                        fireNoteOn (v, newPitch, ch - 1, 0, p);  // new pitch on
                        joyActivePitch_[v] = newPitch;
                    }
                }
            }
            else
            {
                // Below threshold — accumulate stillness samples.
                joystickStillSamples_[v] += p.blockSize;
                const int closeAfter = static_cast<int>(0.050f * p.sampleRate); // 50ms debounce

                if (gateOpen_[v].load() && joystickStillSamples_[v] >= closeAfter)
                {
                    // Joystick has been still for 50ms — close the gate.
                    fireNoteOff(v, ch - 1, 0, p);   // clears gateOpen_ and activePitch_
                    joyActivePitch_[v]       = -1;
                    joystickStillSamples_[v] = 0;
                }
            }

            // Skip the common trigger path for Joystick source (handled inline above).
            trigger = false;
        }
        else if (src == TriggerSource::Random)
        {
            if (randomFired && nextRandom() < p.randomDensity)
                trigger = true;
        }

        if (trigger)
        {
            if (gateOpen_[v].load())
                fireNoteOff(v, ch - 1, 0, p);   // stop previous note first
            fireNoteOn(v, p.heldPitches[v], ch - 1, 0, p);
        }
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
        joystickStillSamples_[v] = 0;
        padPressed_[v].store(false);
        padJustFired_[v].store(false);
    }
    allTrigger_.store(false);
    joystickTrig_.store(false);
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
