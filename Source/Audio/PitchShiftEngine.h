#pragma once

#include "../Common.h"

//==============================================================================
/**
 * Rate-based pitch shifting.
 *
 * Computes a playback rate multiplier from a global semitone offset plus a
 * per-pad semitone offset.  The actual resampling is performed by PadVoice
 * using the returned rate.
 *
 * Tonal and Percussive modes are reserved for future formant-aware
 * implementations; they currently behave identically to Chromatic.
 */
class PitchShiftEngine
{
public:
    PitchShiftEngine()  = default;
    ~PitchShiftEngine() = default;

    //==========================================================================
    /** Select the pitch-shift algorithm.
     *  Currently only Chromatic (rate-based) is implemented. */
    void setMode (PitchMode mode);

    /** Set a global pitch offset applied to all pads (range: -24 .. +24 semitones). */
    void setGlobalOffset (float semitones);

    /** Current global offset in semitones. */
    float getGlobalOffset() const;

    /** Current mode. */
    PitchMode getMode() const;

    //==========================================================================
    /** Returns the playback rate for a given per-pad offset.
     *
     *  rate = 2 ^ ((globalOffset_ + padOffsetSemitones) / 12)
     *
     *  A rate of 1.0 means no pitch change.
     *  @param padOffsetSemitones  Per-pad pitch offset in semitones (-24..+24).
     */
    double getPlaybackRate (float padOffsetSemitones) const;

private:
    //==========================================================================
    PitchMode mode_         { PitchMode::Chromatic };
    float     globalOffset_ { 0.0f };
};
