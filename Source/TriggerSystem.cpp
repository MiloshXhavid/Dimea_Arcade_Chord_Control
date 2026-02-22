#include "TriggerSystem.h"
#include <cmath>
#include <limits>

TriggerSystem::TriggerSystem()
{
    for (auto& a : padPressed_)   a.store(false);
    for (auto& a : padJustFired_) a.store(false);
    for (auto& a : gateOpen_)     a.store(false);
    for (int v = 0; v < 4; ++v)
        joyLastBendValue_[v] = 0;
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
    const double samplesPerBeat  = (p.sampleRate * 60.0) / p.bpm;
    const double subdivBeats     = subdivisionBeats(p.randomSubdiv);
    const double samplesPerSubdiv = samplesPerBeat * subdivBeats;

    // ── Joystick continuous gate model ───────────────────────────────────────
    // Magnitude is Chebyshev distance of per-block delta (not absolute position).
    const float dx = p.joystickX - static_cast<float>(prevJoystickX_);
    const float dy = p.joystickY - static_cast<float>(prevJoystickY_);
    prevJoystickX_ = p.joystickX;
    prevJoystickY_ = p.joystickY;
    const float magnitude         = std::max(std::abs(dx), std::abs(dy));  // Chebyshev distance
    const bool  joyAboveThreshold = magnitude > p.joystickThreshold;
    // 500ms stillness window for retrigger check (NOT a gate-close timer)
    const int   retriggerSamples  = static_cast<int>(0.500 * p.sampleRate);

    // ── Per-sample random clock ───────────────────────────────────────────────
    // We advance the random clock and check for subdivision crossings.
    // For simplicity we fire once at the start of the block if a subdivision
    // boundary fell within this block.  Full sample-accurate scheduling is a
    // future enhancement.

    const bool allTrig = allTrigger_.exchange(false);

    // Check if a subdivision boundary crossed this block
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
        const int       ch    = p.midiChannels[v] - 1;   // 0-based for MidiBuffer
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
                fireNoteOff(v, ch, 0, p);
            }
        }
        else if (src == TriggerSource::Joystick)
        {
            if (joyAboveThreshold)
            {
                // Joystick is moving — reset stillness counter
                joystickStillSamples_[v] = 0;

                if (!gateOpen_[v].load())
                {
                    // Gate was closed — set pitch-bend range via RPN, reset bend,
                    // then open the gate with a note-on at the current pitch.
                    int pitch = p.heldPitches[v];
                    sendBendRangeRPN(v, ch, 0, p);
                    joyLastBendValue_[v] = 0;
                    fireNoteOn(v, pitch, ch, 0, p);
                    joyActivePitch_[v] = pitch;
                    // trigger flag already handled by fireNoteOn — skip common trigger path
                }
                else
                {
                    // Gate already open — update pitch via pitch bend instead of retrigger.
                    int targetPitch  = p.heldPitches[v];
                    int bendSemitones = targetPitch - joyActivePitch_[v];
                    // Clamp to ±12 (matches the RPN range we set)
                    bendSemitones = juce::jlimit(-12, 12, bendSemitones);
                    // Convert to MIDI pitch bend value (±8191 for ±12 semitones)
                    int bendValue = static_cast<int>(bendSemitones / 12.0f * 8191.0f);

                    if (bendValue != joyLastBendValue_[v])
                        sendPitchBend(v, ch, bendValue, 0, p);
                }
            }
            else
            {
                // Joystick is still — accumulate stillness samples
                joystickStillSamples_[v] += p.blockSize;

                if (gateOpen_[v].load() && joystickStillSamples_[v] >= retriggerSamples)
                {
                    // 500ms of stillness — settle: reset pitch bend and optionally retrigger
                    int currentPitch = p.heldPitches[v];

                    if (currentPitch != joyActivePitch_[v])
                    {
                        // Pitch changed while joystick was moving — retrigger at new pitch
                        // Reset bend first so the note-off goes to the original sounding pitch
                        sendPitchBend(v, ch, 0, 0, p);
                        fireNoteOff(v, ch, 0, p);
                        fireNoteOn(v, currentPitch, ch, 0, p);
                        joyActivePitch_[v] = currentPitch;
                    }
                    else
                    {
                        // Pitch unchanged — just snap the bend back to 0 (exact semitone)
                        if (joyLastBendValue_[v] != 0)
                            sendPitchBend(v, ch, 0, 0, p);
                    }

                    // Reset counter so we don't retrigger again next block
                    joystickStillSamples_[v] = 0;
                }
                // Gate stays open indefinitely — never closed by stillness alone
            }
            // Skip the common trigger path for Joystick source (handled inline above)
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
                fireNoteOff(v, ch, 0, p);   // stop previous note first
            fireNoteOn(v, p.heldPitches[v], ch, 0, p);
        }
    }

    // joystickTrig_ is kept for external callers (notifyJoystickMoved) but the
    // continuous gate model in the per-voice loop no longer uses it here.
    joystickTrig_.store(false);
}

void TriggerSystem::resetAllGates()
{
    for (int v = 0; v < 4; ++v)
    {
        gateOpen_[v].store(false);
        activePitch_[v] = -1;
        padPressed_[v].store(false);
        padJustFired_[v].store(false);
        joystickStillSamples_[v] = 0;
        joyActivePitch_[v]       = -1;
        joyLastBendValue_[v]     = 0;
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

// Send RPN 0 (pitch bend range) CC sequence: ±12 semitones.
// Must be called before the gate-open note-on so the synth is configured.
// ch is 0-based (passed straight through to onBend which will convert to 1-based).
void TriggerSystem::sendBendRangeRPN(int voice, int ch, int sampleOff,
                                      const ProcessParams& p)
{
    if (!p.onBend) return;
    // Encode the RPN CC sequence as a special sentinel bend value that the
    // PluginProcessor's onBend lambda recognises.
    // We use a separate path: pass INT_MIN as a flag so the callback emits
    // the full RPN setup instead of a plain pitchWheel message.
    p.onBend(voice, std::numeric_limits<int>::min(), sampleOff);
}

// Send a plain pitch-bend message and track the last value.
// ch is 0-based.
void TriggerSystem::sendPitchBend(int voice, int ch, int bendValue, int sampleOff,
                                   const ProcessParams& p)
{
    if (!p.onBend) return;
    joyLastBendValue_[voice] = bendValue;
    p.onBend(voice, bendValue, sampleOff);
}
