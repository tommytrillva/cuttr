#pragma once

#include "../Common.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

//==============================================================================
/**
 * Pitch shifting engine supporting three modes:
 *
 *  - Chromatic   : Rate-based (pitch AND tempo change together).
 *                  Implemented via getPlaybackRate(); PadVoice resamples at
 *                  the returned rate.
 *
 *  - Tonal       : Phase-vocoder pitch shift — pitch changes WITHOUT changing
 *                  tempo / buffer length.  Call processBuffer() in-place.
 *
 *  - Percussive  : Granular resynthesis pitch shift — pitch changes WITHOUT
 *                  changing tempo / buffer length, optimised for transient-
 *                  heavy material.  Call processBuffer() in-place.
 *
 * Usage for Tonal / Percussive:
 *   1. Call prepare(sampleRate, maxBlockSize) once after construction.
 *   2. Check requiresBufferProcessing() — true for Tonal & Percussive.
 *   3. Call processBuffer(buffer, semitones) to pitch-shift in place.
 *
 * NOTE: PadVoice currently only uses getPlaybackRate().  For Tonal /
 * Percussive modes to be heard, PadVoice must be updated to call
 * processBuffer() when requiresBufferProcessing() returns true.
 */
class PitchShiftEngine
{
public:
    PitchShiftEngine()  = default;
    ~PitchShiftEngine() = default;

    //==========================================================================
    /** Select the pitch-shift algorithm. */
    void setMode (PitchMode mode);

    /** Set a global pitch offset applied to all pads (range: -24 .. +24 semitones). */
    void setGlobalOffset (float semitones);

    /** Current global offset in semitones. */
    float getGlobalOffset() const;

    /** Current mode. */
    PitchMode getMode() const;

    //==========================================================================
    /** Prepare internal state.  Must be called before processBuffer().
     *  @param sampleRate    Host sample rate in Hz.
     *  @param maxBlockSize  Maximum expected buffer size in samples (unused
     *                       for now; reserved for future pre-allocation). */
    void prepare (double sampleRate, int maxBlockSize);

    //==========================================================================
    /** Returns true for Tonal and Percussive modes, which require in-place
     *  buffer processing rather than a playback-rate multiplier.
     *
     *  PadVoice should check this and call processBuffer() instead of
     *  getPlaybackRate() when it returns true. */
    bool requiresBufferProcessing() const;

    //==========================================================================
    /** Pitch-shift an entire buffer in place (Tonal / Percussive modes).
     *
     *  The buffer length is unchanged — pitch shifts without affecting tempo.
     *  The total shift applied is (globalOffset_ + padOffsetSemitones).
     *
     *  For Chromatic mode this falls through to a no-op (use getPlaybackRate
     *  instead).
     *
     *  @param buffer               Audio buffer to process in place.
     *  @param padOffsetSemitones   Per-pad semitone offset (-24..+24). */
    void processBuffer (juce::AudioBuffer<float>& buffer, float padOffsetSemitones);

    //==========================================================================
    /** Returns the playback rate multiplier for Chromatic mode.
     *
     *  rate = 2 ^ ((globalOffset_ + padOffsetSemitones) / 12)
     *
     *  A rate of 1.0 means no pitch change.
     *  @param padOffsetSemitones  Per-pad pitch offset in semitones (-24..+24). */
    double getPlaybackRate (float padOffsetSemitones) const;

private:
    //==========================================================================
    // Phase-vocoder state (one instance per channel, allocated in prepare())
    struct PhaseVocoderState
    {
        std::vector<float> lastPhase;
        std::vector<float> synthPhase;
        // fftSize and hopSize are stored here for convenience but are
        // currently fixed constants in processBufferTonal().
        int hopSize { 256 };
        int fftSize { 2048 };
    };

    //==========================================================================
    void processBufferTonal      (juce::AudioBuffer<float>& buffer, float semitones);
    void processBufferPercussive (juce::AudioBuffer<float>& buffer, float semitones);

    //==========================================================================
    PitchMode mode_         { PitchMode::Chromatic };
    float     globalOffset_ { 0.0f };
    double    sampleRate_   { 44100.0 };

    // Per-channel phase vocoder state (resized in prepare())
    std::vector<PhaseVocoderState> pvState_;
};
