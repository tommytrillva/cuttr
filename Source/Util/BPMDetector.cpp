#include "BPMDetector.h"

#include <algorithm>
#include <cmath>
#include <numeric>

//==============================================================================
BPMDetector::BPMDetector()
{
    // Default BPM range is set via member initialisation.
}

//==============================================================================
void BPMDetector::setBPMRange (float minBPM, float maxBPM)
{
    jassert (minBPM > 0.0f && maxBPM > minBPM);
    minBPM_ = minBPM;
    maxBPM_ = maxBPM;
}

//==============================================================================
std::vector<float> BPMDetector::computeOnsetStrength (const std::vector<float>& mono,
                                                       int hopSize) const
{
    const int numSamples = static_cast<int> (mono.size());
    if (numSamples <= 0 || hopSize <= 0)
        return {};

    // --- Step 1: RMS energy per hop frame ---
    std::vector<float> rmsEnvelope;
    for (int start = 0; start < numSamples; start += hopSize)
    {
        const int end    = std::min (start + hopSize, numSamples);
        const int length = end - start;

        float sumSq = 0.0f;
        for (int i = start; i < end; ++i)
            sumSq += mono[static_cast<size_t> (i)] * mono[static_cast<size_t> (i)];

        rmsEnvelope.push_back (std::sqrt (sumSq / static_cast<float> (length)));
    }

    if (rmsEnvelope.empty())
        return {};

    // --- Step 2: Half-wave rectified first difference (onset strength) ---
    //   onset[n] = max(0, rms[n] - rms[n-1])
    std::vector<float> onset;
    onset.reserve (rmsEnvelope.size());
    onset.push_back (0.0f); // frame 0 has no predecessor

    for (size_t i = 1; i < rmsEnvelope.size(); ++i)
    {
        const float diff = rmsEnvelope[i] - rmsEnvelope[i - 1];
        onset.push_back (diff > 0.0f ? diff : 0.0f);
    }

    return onset;
}

//==============================================================================
float BPMDetector::autocorrelation (const std::vector<float>& signal, int lag) const
{
    const int n = static_cast<int> (signal.size());
    if (lag >= n) return 0.0f;

    float sum = 0.0f;
    for (int i = 0; i < n - lag; ++i)
        sum += signal[static_cast<size_t> (i)] * signal[static_cast<size_t> (i + lag)];
    return sum;
}

//==============================================================================
BPMDetector::Result BPMDetector::detectBPM (const juce::AudioBuffer<float>& buffer,
                                             double                           sampleRate)
{
    Result result;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0 || sampleRate <= 0.0)
        return result;

    //--------------------------------------------------------------------------
    // 1. Mix to mono
    //--------------------------------------------------------------------------
    std::vector<float> mono (static_cast<size_t> (numSamples), 0.0f);
    const float invChannels = 1.0f / static_cast<float> (numChannels);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            mono[static_cast<size_t> (i)] += src[i] * invChannels;
    }

    //--------------------------------------------------------------------------
    // 2. Compute onset-strength envelope
    //--------------------------------------------------------------------------
    const int hopSize = kHopSize;
    auto onset = computeOnsetStrength (mono, hopSize);

    if (onset.empty())
        return result;

    // Self-correlation at lag 0 (used for normalisation)
    const float selfCorr = autocorrelation (onset, 0);
    if (selfCorr <= 0.0f)
        return result; // silent buffer

    //--------------------------------------------------------------------------
    // 3. Convert BPM range → lag range in onset frames
    //
    //   hopDuration = hopSize / sampleRate   (seconds per frame)
    //   beatPeriod  = 60.0 / bpm             (seconds per beat)
    //   lag         = beatPeriod / hopDuration = (60.0 * sampleRate) / (bpm * hopSize)
    //
    //   minBPM → maxLag,  maxBPM → minLag
    //--------------------------------------------------------------------------
    const double hopDuration = static_cast<double> (hopSize) / sampleRate;

    const int minLag = std::max (1, static_cast<int> (std::floor (60.0 / (static_cast<double> (maxBPM_) * hopDuration))));
    const int maxLag = static_cast<int> (std::ceil  (60.0 / (static_cast<double> (minBPM_) * hopDuration)));

    const int onsetLen = static_cast<int> (onset.size());
    const int searchMaxLag = std::min (maxLag, onsetLen - 1);

    if (minLag > searchMaxLag)
        return result; // not enough data

    //--------------------------------------------------------------------------
    // 4. Find the lag with the highest autocorrelation in range
    //--------------------------------------------------------------------------
    float bestCorr = -1.0f;
    int   bestLag  = minLag;

    for (int lag = minLag; lag <= searchMaxLag; ++lag)
    {
        const float corr = autocorrelation (onset, lag);
        if (corr > bestCorr)
        {
            bestCorr = corr;
            bestLag  = lag;
        }
    }

    if (bestCorr <= 0.0f)
        return result;

    //--------------------------------------------------------------------------
    // 5. Convert best lag back to BPM
    //--------------------------------------------------------------------------
    const double beatPeriodSeconds = static_cast<double> (bestLag) * hopDuration;
    const float  bpm               = static_cast<float> (60.0 / beatPeriodSeconds);

    result.bpm        = bpm;
    result.confidence = bestCorr / selfCorr; // normalised to [0..1]
    result.valid      = true;

    return result;
}
