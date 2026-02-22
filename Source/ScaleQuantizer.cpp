#include "ScaleQuantizer.h"
#include <cmath>
#include <climits>

// ─── Scale patterns (semitones from root, sorted ascending) ──────────────────
// Note: these are static arrays, each row padded with -1 up to 12 entries.
// We store them as raw arrays and use sSizes[] for actual length.

static const int kMajor[]            = {0,2,4,5,7,9,11};
static const int kNatMinor[]         = {0,2,3,5,7,8,10};
static const int kHarmMinor[]        = {0,2,3,5,7,8,11};
static const int kMelMinor[]         = {0,2,3,5,7,9,11};
static const int kDorian[]           = {0,2,3,5,7,9,10};
static const int kPhrygian[]         = {0,1,3,5,7,8,10};
static const int kLydian[]           = {0,2,4,6,7,9,11};
static const int kMixolydian[]       = {0,2,4,5,7,9,10};
static const int kLocrian[]          = {0,1,3,5,6,8,10};
static const int kPentaMajor[]       = {0,2,4,7,9};
static const int kPentaMinor[]       = {0,3,5,7,10};
static const int kBlues[]            = {0,3,5,6,7,10};
static const int kWholeTone[]        = {0,2,4,6,8,10};
static const int kDiminished[]       = {0,1,3,4,6,7,9,10};   // HW
static const int kDiminishedWH[]     = {0,2,3,5,6,8,9,11};   // WH
static const int kAugmented[]        = {0,3,4,7,8,11};
static const int kHungarianMinor[]   = {0,2,3,6,7,8,11};
static const int kPhrygDominant[]    = {0,1,4,5,7,8,10};
static const int kDoubleHarmonic[]   = {0,1,4,5,7,8,11};
static const int kChromatic[]        = {0,1,2,3,4,5,6,7,8,9,10,11};

struct ScaleEntry { const int* pattern; int size; const char* name; };

static const ScaleEntry kScaleTable[] =
{
    { kMajor,           7,  "Major"              },
    { kNatMinor,        7,  "Natural Minor"      },
    { kHarmMinor,       7,  "Harmonic Minor"     },
    { kMelMinor,        7,  "Melodic Minor"      },
    { kDorian,          7,  "Dorian"             },
    { kPhrygian,        7,  "Phrygian"           },
    { kLydian,          7,  "Lydian"             },
    { kMixolydian,      7,  "Mixolydian"         },
    { kLocrian,         7,  "Locrian"            },
    { kPentaMajor,      5,  "Pentatonic Major"   },
    { kPentaMinor,      5,  "Pentatonic Minor"   },
    { kBlues,           6,  "Blues"              },
    { kWholeTone,       6,  "Whole Tone"         },
    { kDiminished,      8,  "Diminished (HW)"    },
    { kDiminishedWH,    8,  "Diminished (WH)"    },
    { kAugmented,       6,  "Augmented"          },
    { kHungarianMinor,  7,  "Hungarian Minor"    },
    { kPhrygDominant,   7,  "Phrygian Dominant"  },
    { kDoubleHarmonic,  7,  "Double Harmonic"    },
    { kChromatic,       12, "Chromatic"          },
};

static_assert((int)ScalePreset::COUNT == 20, "Update kScaleTable");

// ─── Public API ──────────────────────────────────────────────────────────────

const int* ScaleQuantizer::getScalePattern(ScalePreset s)
{
    return kScaleTable[(int)s].pattern;
}

int ScaleQuantizer::getScaleSize(ScalePreset s)
{
    return kScaleTable[(int)s].size;
}

const char* ScaleQuantizer::getScaleName(ScalePreset s)
{
    return kScaleTable[(int)s].name;
}

void ScaleQuantizer::buildCustomPattern(const bool scaleNotes[12],
                                        int outPattern[12],
                                        int& outSize)
{
    outSize = 0;
    for (int i = 0; i < 12; ++i)
        if (scaleNotes[i])
            outPattern[outSize++] = i;

    if (outSize == 0)                 // fallback: just the root
    {
        outPattern[0] = 0;
        outSize = 1;
    }
}

int ScaleQuantizer::quantize(int rawPitch,
                             const int* scalePattern,
                             int scaleSize)
{
    if (scaleSize <= 0)
        return juce::jlimit(0, 127, rawPitch);

    // Clamp input to valid MIDI range first
    rawPitch = juce::jlimit(0, 127, rawPitch);

    int bestPitch    = -1;
    int bestDistance = INT_MAX;

    // Search across 3 octaves (one below, current, one above) so we always
    // find the nearest note even at octave boundaries.
    const int searchOctave = rawPitch / 12;

    for (int oct = searchOctave - 1; oct <= searchOctave + 1; ++oct)
    {
        for (int i = 0; i < scaleSize; ++i)
        {
            const int candidate = oct * 12 + scalePattern[i];
            if (candidate < 0 || candidate > 127)
                continue;

            const int dist = std::abs(candidate - rawPitch);

            // Prefer smaller distance; on tie prefer the lower note
            if (dist < bestDistance ||
               (dist == bestDistance && candidate < bestPitch))
            {
                bestDistance = dist;
                bestPitch    = candidate;
            }
        }
    }

    return (bestPitch < 0) ? rawPitch : bestPitch;
}
