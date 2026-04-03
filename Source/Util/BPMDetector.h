#pragma once

#include "../Common.h"

#include <vector>

//==============================================================================
/**
 * BPMDetector estimates the tempo of an audio buffer using an autocorrelation
 * approach applied to an RMS onset-strength envelope.
 *
 * Usage:
 * @code
 *     BPMDetector detector;
 *     detector.setBPMRange (60.0f, 200.0f);
 *     auto result = detector.detectBPM (buffer, sampleRate);
 *     if (result.valid)
 *         DBG ("Detected BPM: " << result.bpm);
 * @endcode
 */
class BPMDetector
{
public:
    //==========================================================================
    struct Result
    {
        float bpm        = 0.0f;   ///< Detected tempo in beats per minute.
        float confidence = 0.0f;   ///< Normalised confidence [0..1]; 1 = perfect autocorr peak.
        bool  valid      = false;  ///< false if detection failed (e.g. silent buffer).
    };

    //==========================================================================
    BPMDetector();

    /**
     * Detect the tempo of @p buffer.
     *
     * @param buffer      Source audio (any channel count; mixed to mono internally).
     * @param sampleRate  Sample rate of the buffer in Hz.
     * @return            Detection result; check result.valid before using bpm.
     */
    Result detectBPM (const juce::AudioBuffer<float>& buffer,
                      double                           sampleRate);

    /**
     * Constrain tempo search to the given BPM range.
     * Defaults to [60, 200] BPM.
     *
     * @param minBPM  Lower bound (must be > 0).
     * @param maxBPM  Upper bound (must be > minBPM).
     */
    void setBPMRange (float minBPM, float maxBPM);

private:
    //==========================================================================
    /** Compute a per-frame RMS onset-strength envelope from mono audio.
     *
     *  For each hop, the RMS energy is calculated.  The onset strength is the
     *  positive (half-wave rectified) first difference of the RMS envelope,
     *  which emphasises energy increases (onsets).
     *
     *  @param mono     Mono audio samples.
     *  @param hopSize  Analysis hop size in samples.
     *  @return         Non-negative onset strength values, one per frame.
     */
    std::vector<float> computeOnsetStrength (const std::vector<float>& mono,
                                             int hopSize) const;

    /** Unnormalised autocorrelation of @p signal at lag @p lag.
     *  @param signal   Input signal.
     *  @param lag      Lag in samples (frames).
     *  @return         Sum of element-wise products at the given lag.
     */
    float autocorrelation (const std::vector<float>& signal, int lag) const;

    //==========================================================================
    float minBPM_ = 60.0f;
    float maxBPM_ = 200.0f;

    /** Hop size (in audio samples) used for the onset-strength envelope. */
    static constexpr int kHopSize = 512;
};
