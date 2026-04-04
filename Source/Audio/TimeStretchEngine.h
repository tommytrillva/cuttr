#pragma once

#include "../Common.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>
#include <cmath>

//==============================================================================
/**
 * BPM-based time-stretch ratio calculator, plus an offline granular
 * overlap-add (SOLA) time-stretch processor.
 *
 * --- Playback-rate API (real-time, used by PadVoice) ---
 *
 * getEffectivePlaybackRate() returns a multiplier that PadVoice advances its
 * read-head by.  A rate > 1 speeds up playback (shrinks duration); a rate < 1
 * slows down playback (stretches duration).  NOTE: changing the playback rate
 * also changes pitch — this is the legacy "Chromatic" behaviour.
 *
 * --- stretchBuffer API (offline, pitch-preserving) ---
 *
 * stretchBuffer() processes a full slice buffer and returns a new buffer whose
 * duration is scaled by stretchRatio WITHOUT changing pitch.  Call this before
 * handing the slice to PadVoice so that PadVoice plays at rate 1.0.
 *
 * Modes:
 *   None     – no stretching; getEffectivePlaybackRate returns pitchRate * 1.0.
 *   MatchBPM – BPM ratio applied via playback rate (affects pitch; superseded by
 *              stretchBuffer for offline use).
 *   PerPad   – per-pad stretch ratio supplied by the caller (padStretchRatio).
 *   Free     – same as PerPad at this layer; caller supplies the ratio.
 *
 * NOTE: ChopprProcessor::prepareToPlay should call
 *       timeStretchEngine_.prepare(sampleRate, samplesPerBlock)
 *       so that grain sizes are computed at the correct sample rate.
 */
class TimeStretchEngine
{
public:
    TimeStretchEngine()  = default;
    ~TimeStretchEngine() = default;

    //==========================================================================
    /** Prepare the engine.  Must be called before stretchBuffer().
     *
     *  NOTE: ChopprProcessor::prepareToPlay should call
     *        timeStretchEngine_.prepare(sampleRate, samplesPerBlock).
     */
    void prepare (double sampleRate, int maxBlockSize);

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
     *
     *  NOTE: When mode == None, returns pitchRate unchanged (1.0 if no pitch shift).
     *  NOTE: When mode == MatchBPM, multiplying the playback rate changes both
     *        speed AND pitch.  For pitch-preserving stretch, use stretchBuffer()
     *        instead (offline, pre-processes the slice before playback).
     */
    double getEffectivePlaybackRate (float pitchRate, float padStretchRatio) const;

    //==========================================================================
    /** Offline pitch-preserving time-stretch via synchronous overlap-add (SOLA).
     *
     *  Returns a new AudioBuffer<float> whose duration is
     *      outputSamples = round(input.getNumSamples() * stretchRatio)
     *  The pitch of the audio is unchanged.
     *
     *  @param input         Source audio (any number of channels).
     *  @param stretchRatio  output_length / input_length.
     *                       > 1.0 => slower / longer output.
     *                       < 1.0 => faster / shorter output.
     *  @return              New buffer with stretched audio.
     *
     *  prepare() must have been called before using this method.
     */
    juce::AudioBuffer<float> stretchBuffer (const juce::AudioBuffer<float>& input,
                                            float stretchRatio) const;

private:
    //==========================================================================
    TimeStretchMode mode_       { TimeStretchMode::None };
    float           projectBPM_ { 120.0f };
    float           sampleBPM_  { 120.0f };
    double          sampleRate_ { 44100.0 };
};
