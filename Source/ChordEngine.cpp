#include "ChordEngine.h"

// ─── Internal helpers ─────────────────────────────────────────────────────────

// Map normalised joystick axis (−1..+1) to a MIDI pitch offset (0..atten).
// Center (0.0) maps to atten/2 so the joystick sweeps the full range.
static float axisToPitchOffset(float axis, float atten)
{
    // axis −1..+1  →  0..atten
    return ((axis + 1.0f) * 0.5f) * atten;
}

// ─── ChordEngine::rawPitch ────────────────────────────────────────────────────

float ChordEngine::rawPitch(int voiceIndex, const Params& p)
{
    float base = 0.0f;

    // Voices 0 (Root) and 1 (Third) track the Y axis
    // Voices 2 (Fifth) and 3 (Tension) track the X axis
    if (voiceIndex == 0 || voiceIndex == 1)
        base = axisToPitchOffset(p.joystickY, p.yAtten);
    else
        base = axisToPitchOffset(p.joystickX, p.xAtten);

    // Add interval above root
    base += static_cast<float>(p.intervals[voiceIndex]);

    // Octave offset
    base += static_cast<float>(p.octaves[voiceIndex] * 12);

    return base;
}

// ─── ChordEngine::computePitch ────────────────────────────────────────────────

int ChordEngine::computePitch(int voiceIndex, const Params& p)
{
    jassert(voiceIndex >= 0 && voiceIndex < 4);

    const int rawMidi = juce::jlimit(0, 127,
                                     static_cast<int>(std::round(rawPitch(voiceIndex, p))));

    // Build the scale pattern to quantize against
    int quantized;
    if (p.useCustomScale)
    {
        int pattern[12];
        int size = 0;
        ScaleQuantizer::buildCustomPattern(p.customNotes, pattern, size);
        quantized = ScaleQuantizer::quantize(rawMidi, pattern, size);
    }
    else
    {
        const int*  pat  = ScaleQuantizer::getScalePattern(p.scalePreset);
        const int   size = ScaleQuantizer::getScaleSize(p.scalePreset);
        quantized = ScaleQuantizer::quantize(rawMidi, pat, size);
    }

    // Global transpose applied after quantization so it shifts the whole key chromatically
    return juce::jlimit(0, 127, quantized + p.globalTranspose);
}
