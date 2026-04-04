#include "PitchShiftEngine.h"

#include <cmath>
#include <vector>
#include <algorithm>

//==============================================================================
void PitchShiftEngine::setMode (PitchMode mode)
{
    mode_ = mode;
}

void PitchShiftEngine::setGlobalOffset (float semitones)
{
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
void PitchShiftEngine::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = sampleRate;

    // Pre-allocate phase-vocoder state for up to 2 channels.
    // Actual resizing per channel happens lazily inside processBufferTonal(),
    // but we initialise the vector here so the size is always known.
    pvState_.resize (2);
    for (auto& s : pvState_)
    {
        s.lastPhase.clear();
        s.synthPhase.clear();
    }
}

//==============================================================================
bool PitchShiftEngine::requiresBufferProcessing() const
{
    return mode_ == PitchMode::Tonal || mode_ == PitchMode::Percussive;
}

//==============================================================================
double PitchShiftEngine::getPlaybackRate (float padOffsetSemitones) const
{
    const double totalSemitones = static_cast<double> (globalOffset_)
                                + static_cast<double> (padOffsetSemitones);
    return std::pow (2.0, totalSemitones / 12.0);
}

//==============================================================================
void PitchShiftEngine::processBuffer (juce::AudioBuffer<float>& buffer,
                                      float padOffsetSemitones)
{
    if (buffer.getNumSamples() == 0)
        return;

    const float totalSemitones = globalOffset_ + padOffsetSemitones;

    // Nothing to do for Chromatic — caller should use getPlaybackRate() instead.
    if (mode_ == PitchMode::Chromatic)
        return;

    // Grow pvState_ if the buffer has more channels than we pre-allocated.
    if (buffer.getNumChannels() > static_cast<int> (pvState_.size()))
        pvState_.resize (static_cast<std::size_t> (buffer.getNumChannels()));

    if (mode_ == PitchMode::Tonal)
        processBufferTonal (buffer, totalSemitones);
    else if (mode_ == PitchMode::Percussive)
        processBufferPercussive (buffer, totalSemitones);
}

//==============================================================================
// Tonal mode — block-based phase vocoder
//==============================================================================
void PitchShiftEngine::processBufferTonal (juce::AudioBuffer<float>& buffer,
                                            float semitones)
{
    const float ratio     = std::pow (2.0f, semitones / 12.0f);
    const int   fftOrder  = 11;               // 2048-point FFT
    const int   fftSize   = 1 << fftOrder;    // 2048
    const int   hopSize   = fftSize / 8;      // 256  (75 % overlap)

    juce::dsp::FFT fft (fftOrder);

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const int halfSize    = fftSize / 2 + 1;

    // Hann analysis window (computed once, shared across channels)
    std::vector<float> window (static_cast<std::size_t> (fftSize));
    for (int i = 0; i < fftSize; ++i)
        window[static_cast<std::size_t> (i)] =
            0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi
                                     * static_cast<float> (i)
                                     / static_cast<float> (fftSize)));

    const float expectedPhaseDiff =
        2.0f * juce::MathConstants<float>::pi
        * static_cast<float> (hopSize)
        / static_cast<float> (fftSize);

    // Overlap-add normalisation for Hann window with 75 % overlap
    const float scale = 2.0f * static_cast<float> (hopSize)
                        / (static_cast<float> (fftSize) * 0.375f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        // Ensure per-channel phase state is properly sized
        auto& state = pvState_[static_cast<std::size_t> (ch)];
        if (static_cast<int> (state.lastPhase.size()) != halfSize)
        {
            state.lastPhase.assign (static_cast<std::size_t> (halfSize), 0.0f);
            state.synthPhase.assign (static_cast<std::size_t> (halfSize), 0.0f);
        }

        std::vector<float> output (static_cast<std::size_t> (numSamples), 0.0f);

        // Working buffers — allocated once per channel
        std::vector<juce::dsp::Complex<float>> fftBuf (static_cast<std::size_t> (fftSize));
        std::vector<juce::dsp::Complex<float>> fftOut (static_cast<std::size_t> (fftSize));
        std::vector<float> mag  (static_cast<std::size_t> (halfSize));
        std::vector<float> freq (static_cast<std::size_t> (halfSize));
        std::vector<float> newMag  (static_cast<std::size_t> (fftSize), 0.0f);
        std::vector<float> newFreq (static_cast<std::size_t> (fftSize), 0.0f);

        int outPos = 0;
        for (int pos = 0; pos - fftSize < numSamples; pos += hopSize)
        {
            // --- Analysis: fill FFT input with windowed signal ---
            for (int i = 0; i < fftSize; ++i)
            {
                const int srcIdx = pos + i;
                const float sample = (srcIdx >= 0 && srcIdx < numSamples)
                                         ? data[srcIdx]
                                         : 0.0f;
                fftBuf[static_cast<std::size_t> (i)] = { sample * window[static_cast<std::size_t> (i)], 0.0f };
            }

            fft.perform (fftBuf.data(), fftOut.data(), false);

            // --- Phase vocoder: compute instantaneous frequencies ---
            for (int k = 0; k < halfSize; ++k)
            {
                const auto& c = fftOut[static_cast<std::size_t> (k)];
                mag[static_cast<std::size_t> (k)] =
                    std::sqrt (c.real() * c.real() + c.imag() * c.imag());

                float phase  = std::atan2 (c.imag(), c.real());
                float dPhase = phase
                               - state.lastPhase[static_cast<std::size_t> (k)]
                               - static_cast<float> (k) * expectedPhaseDiff;

                // Wrap to [-pi, pi]
                dPhase -= 2.0f * juce::MathConstants<float>::pi
                          * std::round (dPhase
                                        / (2.0f * juce::MathConstants<float>::pi));

                freq[static_cast<std::size_t> (k)] =
                    (static_cast<float> (k) * 2.0f
                     * juce::MathConstants<float>::pi
                     / static_cast<float> (fftSize))
                    + dPhase / static_cast<float> (hopSize);

                state.lastPhase[static_cast<std::size_t> (k)] = phase;
            }

            // --- Pitch scaling: map bins to shifted positions ---
            std::fill (newMag.begin(),  newMag.end(),  0.0f);
            std::fill (newFreq.begin(), newFreq.end(), 0.0f);

            for (int k = 0; k < halfSize; ++k)
            {
                const int newK = juce::roundToInt (static_cast<float> (k) * ratio);
                if (newK < halfSize)
                {
                    newMag[static_cast<std::size_t> (newK)]  += mag[static_cast<std::size_t> (k)];
                    newFreq[static_cast<std::size_t> (newK)]  = freq[static_cast<std::size_t> (k)] * ratio;
                }
            }

            // --- Synthesis: accumulate phases and build IFFT input ---
            for (int k = 0; k < halfSize; ++k)
            {
                state.synthPhase[static_cast<std::size_t> (k)] +=
                    newFreq[static_cast<std::size_t> (k)] * static_cast<float> (hopSize);

                const float s = state.synthPhase[static_cast<std::size_t> (k)];
                const float m = newMag[static_cast<std::size_t> (k)];

                fftOut[static_cast<std::size_t> (k)] = { m * std::cos (s),
                                                          m * std::sin (s) };

                // Mirror for real-valued IFFT
                if (k > 0 && k < fftSize / 2)
                {
                    fftOut[static_cast<std::size_t> (fftSize - k)] =
                        { fftOut[static_cast<std::size_t> (k)].real(),
                          -fftOut[static_cast<std::size_t> (k)].imag() };
                }
            }

            fft.perform (fftOut.data(), fftBuf.data(), true);

            // --- Overlap-add synthesis output ---
            for (int i = 0; i < fftSize && (outPos + i) < numSamples; ++i)
            {
                output[static_cast<std::size_t> (outPos + i)] +=
                    fftBuf[static_cast<std::size_t> (i)].real()
                    * window[static_cast<std::size_t> (i)]
                    * scale;
            }

            outPos += hopSize;
        }

        std::copy (output.begin(), output.end(), data);
    }
}

//==============================================================================
// Percussive mode — granular pitch shift
//==============================================================================
void PitchShiftEngine::processBufferPercussive (juce::AudioBuffer<float>& buffer,
                                                 float semitones)
{
    const float ratio      = std::pow (2.0f, semitones / 12.0f);
    // 20 ms grains; fall back to a sensible minimum if sampleRate_ is zero
    const int   grainSize  = juce::roundToInt (
                                 static_cast<float> (sampleRate_) * 0.02f);
    if (grainSize <= 0)
        return;

    const int hopSize    = grainSize / 4;
    const int numSamples = buffer.getNumSamples();

    // Hann window for the grain
    std::vector<float> window (static_cast<std::size_t> (grainSize));
    for (int i = 0; i < grainSize; ++i)
        window[static_cast<std::size_t> (i)] =
            0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi
                                     * static_cast<float> (i)
                                     / static_cast<float> (grainSize)));

    // Overlap-add gain normalisation: 4x overlap with Hann window ~ 0.5 gain
    const float norm = (hopSize > 0)
                           ? static_cast<float> (hopSize)
                             / static_cast<float> (grainSize) * 2.0f
                           : 1.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* src = buffer.getReadPointer (ch);
        std::vector<float> output (static_cast<std::size_t> (numSamples), 0.0f);

        juce::LagrangeInterpolator interp;

        for (int pos = 0; pos < numSamples; pos += hopSize)
        {
            // Number of output samples after resampling by 1/ratio
            // (ratio > 1 = pitch up = fewer output samples for the same grain)
            const int outGrainSize =
                juce::roundToInt (static_cast<float> (grainSize) / ratio);
            if (outGrainSize <= 0)
                continue;

            // Build source grain (windowed)
            std::vector<float> srcGrain (static_cast<std::size_t> (grainSize));
            for (int i = 0; i < grainSize; ++i)
            {
                const int idx = pos + i;
                srcGrain[static_cast<std::size_t> (i)] =
                    (idx < numSamples)
                        ? src[idx] * window[static_cast<std::size_t> (i)]
                        : 0.0f;
            }

            // Resample the grain — LagrangeInterpolator::process() reads
            // `ratio` source samples per output sample.
            std::vector<float> grain (static_cast<std::size_t> (outGrainSize));
            interp.reset();
            interp.process (static_cast<double> (ratio),
                            srcGrain.data(),
                            grain.data(),
                            outGrainSize);

            // Overlap-add (no secondary Hann window on output — the source
            // grain was already windowed)
            for (int i = 0; i < outGrainSize && (pos + i) < numSamples; ++i)
                output[static_cast<std::size_t> (pos + i)] +=
                    grain[static_cast<std::size_t> (i)];
        }

        // Normalise overlap gain
        for (int i = 0; i < numSamples; ++i)
            output[static_cast<std::size_t> (i)] *= norm;

        juce::FloatVectorOperations::copy (
            buffer.getWritePointer (ch), output.data(), numSamples);
    }
}
