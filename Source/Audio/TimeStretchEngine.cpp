#include "TimeStretchEngine.h"

#include <algorithm>
#include <cmath>
#include <vector>

//==============================================================================
void TimeStretchEngine::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
}

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

TimeStretchMode TimeStretchEngine::getMode()       const { return mode_; }
float           TimeStretchEngine::getProjectBPM() const { return projectBPM_; }
float           TimeStretchEngine::getSampleBPM()  const { return sampleBPM_; }

//==============================================================================
double TimeStretchEngine::getStretchRatio() const
{
    switch (mode_)
    {
        case TimeStretchMode::MatchBPM:
            // Ratio > 1 means the sample plays faster to match a higher project BPM.
            // NOTE: this affects pitch when used as a playback-rate multiplier.
            // Use stretchBuffer() for offline pitch-preserving stretch instead.
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
            // Global BPM match; per-pad ratio is ignored.
            // NOTE: multiplying playback rate changes both speed AND pitch.
            // For pitch-preserving stretch, use stretchBuffer() (offline).
            return pitch * getStretchRatio();

        case TimeStretchMode::PerPad:
        case TimeStretchMode::Free:
            // Per-pad ratio supplied by caller; global stretch is 1.0
            return pitch * static_cast<double> (padStretchRatio);

        case TimeStretchMode::None:
        default:
            // No stretch — return pitch rate unchanged (1.0 if no pitch shift).
            return pitch;
    }
}

//==============================================================================
juce::AudioBuffer<float> TimeStretchEngine::stretchBuffer (
    const juce::AudioBuffer<float>& input,
    float stretchRatio) const
{
    // stretchRatio: output_length / input_length
    // e.g. 0.5 = twice as fast (half as many samples), 2.0 = twice as slow.

    const int numChannels   = input.getNumChannels();
    const int inputSamples  = input.getNumSamples();
    const int outputSamples = juce::roundToInt (static_cast<float> (inputSamples) * stretchRatio);

    if (outputSamples <= 0 || inputSamples <= 0 || numChannels <= 0)
        return input;

    // If ratio is effectively 1.0 there is nothing to do — return a copy.
    if (std::abs (stretchRatio - 1.0f) < 1e-5f)
        return input;

    juce::AudioBuffer<float> output (numChannels, outputSamples);
    output.clear();

    // --- Grain parameters ---
    // 40 ms grain at the current sample rate, minimum 64 samples.
    const int grainSize = std::max (64, juce::roundToInt (static_cast<float> (sampleRate_) * 0.040f));
    // 10 ms hop in the output stream, minimum 16 samples.
    const int hopOut    = std::max (16, juce::roundToInt (static_cast<float> (sampleRate_) * 0.010f));
    // Corresponding input hop — shorter for speedup, longer for slowdown.
    const int hopIn     = std::max (1, juce::roundToInt (static_cast<float> (hopOut) / stretchRatio));

    // --- Hann window ---
    std::vector<float> window (static_cast<std::size_t> (grainSize));
    for (int i = 0; i < grainSize; ++i)
        window[static_cast<std::size_t> (i)] =
            0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi
                                     * static_cast<float> (i)
                                     / static_cast<float> (grainSize)));

    // --- Overlap-add normalisation ---
    // With a Hann window and 4:1 overlap (hopOut = grainSize/4), the OLA sum
    // of squared window values is constant => unity gain.  For other overlap
    // ratios the scale factor is 2 * hopOut / grainSize.
    const float gain = 2.0f * static_cast<float> (hopOut) / static_cast<float> (grainSize);

    int inPos  = 0;
    int outPos = 0;

    while (outPos < outputSamples)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* src = input.getReadPointer (ch);
            float*       dst = output.getWritePointer (ch);

            for (int i = 0; i < grainSize; ++i)
            {
                const int si = inPos + i;
                const int di = outPos + i;

                if (si >= 0 && si < inputSamples && di >= 0 && di < outputSamples)
                    dst[di] += src[si] * window[static_cast<std::size_t> (i)] * gain;
            }
        }

        inPos  += hopIn;
        outPos += hopOut;
    }

    return output;
}
