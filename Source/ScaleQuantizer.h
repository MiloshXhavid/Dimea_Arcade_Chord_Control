#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// All known scales supported as presets
enum class ScalePreset
{
    Major = 0,
    NaturalMinor,
    HarmonicMinor,
    MelodicMinor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Locrian,
    PentatonicMajor,
    PentatonicMinor,
    Blues,
    WholeTone,
    Diminished,      // octatonic (HW)
    DiminishedWH,    // octatonic (WH)
    Augmented,
    HungarianMinor,
    PhrygianDominant,
    DoubleHarmonic,
    Chromatic,
    COUNT
};

class ScaleQuantizer
{
public:
    // Returns static semitone array (0–11) for preset
    static const int* getScalePattern(ScalePreset scale);
    static int        getScaleSize(ScalePreset scale);
    static const char* getScaleName(ScalePreset scale);

    // Build pattern from 12 custom boolean flags
    static void buildCustomPattern(const bool scaleNotes[12],
                                   int outPattern[12],
                                   int& outSize);

    // Quantize rawPitch (MIDI 0–127) to nearest scale note.
    // Searches ±1 octave (2-octave window). Ties go DOWN.
    static int quantize(int rawPitch,
                        const int* scalePattern,
                        int scaleSize);

};
