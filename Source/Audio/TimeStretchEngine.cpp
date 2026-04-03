#include "TimeStretchEngine.h"

#include <algorithm>

//==============================================================================
void TimeStretchEngine::setMode (TimeStretchMode mode)
{
    mode_ = mode;
}

void TimeStretchEngine::setProjectBPM (float bpm)
{
    projectBPM_ = std::max (1.0f, bpm);
}

void TimeStretchEngine::setSampleBPM (float bpm)
{
    sampleBPM_ = std::max (1.0f, bpm);
}

TimeStretchMode TimeStretchEngine::getMode() const       { return mode_; }
float           TimeStretchEngine::getProjectBPM() const { return projectBPM_; }
float           TimeStretchEngine::getSampleBPM()  const { return sampleBPM_; }

//==============================================================================
double TimeStretchEngine::getStretchRatio() const
{
    switch (mode_)
    {
        case TimeStretchMode::MatchBPM:
            // Ratio > 1 means the sample plays faster to match a higher project BPM
            return static_cast<double> (sampleBPM_) / static_cast<double> (projectBPM_);

        case TimeStretchMode::None:
        case TimeStretchMode::PerPad:
        case TimeStretchMode::Free:
        default:
            return 1.0;
    }
}

//==============================================================================
double TimeStretchEngine::getEffectivePlaybackRate (float pitchRate,
                                                    float padStretchRatio) const
{
    const double pitch = static_cast<double> (pitchRate);

    switch (mode_)
    {
        case TimeStretchMode::MatchBPM:
            // Global BPM match; per-pad ratio is ignored
            return pitch * getStretchRatio();

        case TimeStretchMode::PerPad:
        case TimeStretchMode::Free:
            // Per-pad ratio supplied by caller; global stretch is 1.0
            return pitch * static_cast<double> (padStretchRatio);

        case TimeStretchMode::None:
        default:
            return pitch;
    }
}
