#pragma once

#include "../Common.h"

//==============================================================================
/**
 * BPM-based time-stretch ratio calculator.
 *
 * This class does not perform sample-rate conversion itself; it provides the
 * effective playback-rate multiplier that PadVoice uses when advancing through
 * the sample buffer.  A rate > 1 speeds up playback (shrinks duration);
 * a rate < 1 slows down playback (stretches duration).
 *
 * Modes:
 *   None     – no stretching; returns pitchRate unchanged.
 *   MatchBPM – stretches all pads so the sample duration matches the project
 *              tempo (ratio = sampleBPM / projectBPM).
 *   PerPad   – per-pad stretch ratio supplied by the caller (padStretchRatio).
 *   Free     – same as PerPad at this layer; caller supplies the ratio.
 */
class TimeStretchEngine
{
public:
    TimeStretchEngine()  = default;
    ~TimeStretchEngine() = default;

    //==========================================================================
    void setMode (TimeStretchMode mode);
    void setProjectBPM (float bpm);
    /** Detected (or user-tagged) BPM of the loaded sample. */
    void setSampleBPM (float bpm);

    TimeStretchMode getMode()       const;
    float getProjectBPM()           const;
    float getSampleBPM()            const;

    //==========================================================================
    /** Returns the global stretch ratio determined by the current mode.
     *
     *  MatchBPM: sampleBPM_ / projectBPM_  (> 1 speeds up to match faster project)
     *  All others: 1.0  (per-pad ratios are handled in getEffectivePlaybackRate)
     */
    double getStretchRatio() const;

    /** Combines pitch rate with the appropriate stretch ratio.
     *
     *  @param pitchRate        Rate from PitchShiftEngine::getPlaybackRate().
     *  @param padStretchRatio  Per-pad ratio from PadSettings::stretchRatio
     *                         (used only in PerPad / Free modes).
     *  @return                 Final playback rate for PadVoice.
     */
    double getEffectivePlaybackRate (float pitchRate, float padStretchRatio) const;

private:
    //==========================================================================
    TimeStretchMode mode_       { TimeStretchMode::None };
    float           projectBPM_ { 120.0f };
    float           sampleBPM_  { 120.0f };
};
