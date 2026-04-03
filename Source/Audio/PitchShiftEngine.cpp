#include "PitchShiftEngine.h"

#include <cmath>

//==============================================================================
void PitchShiftEngine::setMode (PitchMode mode)
{
    mode_ = mode;
}

void PitchShiftEngine::setGlobalOffset (float semitones)
{
    // Clamp to supported range
    globalOffset_ = juce::jlimit (-24.0f, 24.0f, semitones);
}

float PitchShiftEngine::getGlobalOffset() const
{
    return globalOffset_;
}

PitchMode PitchShiftEngine::getMode() const
{
    return mode_;
}

//==============================================================================
double PitchShiftEngine::getPlaybackRate (float padOffsetSemitones) const
{
    // All modes currently use the same rate-based formula.
    // Tonal / Percussive are placeholders for future phase-vocoder work.
    const double totalSemitones = static_cast<double> (globalOffset_)
                                + static_cast<double> (padOffsetSemitones);
    return std::pow (2.0, totalSemitones / 12.0);
}
