#pragma once

#include "../Common.h"
#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
/**
 * Software metronome: generates a short sine-wave click on each beat and
 * mixes it into the output buffer.
 *
 * Tap-tempo averages the last four tap intervals to compute BPM.
 * All processBlock state is updated sample-accurately without any allocation.
 */
class Metronome
{
public:
    Metronome()  = default;
    ~Metronome() = default;

    //==========================================================================
    /** Call once during prepareToPlay. */
    void prepare (double sampleRate, int blockSize);

    /** Mix click audio into @p buffer when enabled. */
    void processBlock (juce::AudioBuffer<float>& buffer, int numSamples);

    //==========================================================================
    void setTempo (float bpm);
    void setTimeSignature (int numerator, int denominator);
    void setEnabled (bool enabled);

    /** Reset playhead to the start of bar 1, beat 1. */
    void reset();

    /** Record a tap; once 2+ taps are recorded BPM is inferred from intervals. */
    void tapTempo();

    //==========================================================================
    float  getBPM()                        const;
    double getPlayheadPositionSamples()    const;
    double getBeatPosition()               const; ///< Fractional beat (0-based)

private:
    //==========================================================================
    void recalcSamplesPerBeat();

    //==========================================================================
    float  bpm_            { 120.0f };
    int    timeSigNum_     { 4 };
    int    timeSigDen_     { 4 };
    double sampleRate_     { 44100.0 };
    double samplePosition_ { 0.0 };
    double samplesPerBeat_ { 22050.0 };
    bool   enabled_        { false };

    // Tap-tempo ring buffer (stores high-res ms timestamps)
    juce::Array<double> tapTimes_;
    static constexpr int kMaxTaps = 4;

    // Click synthesis
    float  clickGain_  { 0.5f };
    double clickPhase_ { 0.0 };

    // Click parameters
    static constexpr double kClickFreqHz    = 1000.0;  // 1 kHz sine
    static constexpr double kClickDurationS = 0.004;   // 4 ms
};
