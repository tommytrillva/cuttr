#pragma once

#include "../Common.h"
#include "PadVoice.h"

#include <array>

//==============================================================================
/**
 * Fixed-size pool of kMaxVoices PadVoice instances.
 *
 * Manages voice allocation, choke-group logic, voice stealing, and block
 * rendering.  All methods called from the audio thread are realtime-safe.
 */
class PadVoicePool
{
public:
    PadVoicePool()  = default;
    ~PadVoicePool() = default;

    //==========================================================================
    /** Prepare all voices.  Call from processBlock's prepareToPlay. */
    void prepare (double sampleRate, int blockSize);

    //==========================================================================
    /** Trigger a pad, applying choke logic and stealing a voice if needed.
     *
     *  @param padIndex     Index of the triggering pad (0..kMaxPads-1).
     *  @param settings     PadSettings for this pad.
     *  @param velocity     MIDI velocity (0..1).
     *  @param sample       The loaded SampleBuffer.
     *  @param slices       Current SliceEngine state.
     *  @param playbackRate Combined rate from pitch + time-stretch engines.
     */
    void triggerPad (int               padIndex,
                     const PadSettings& settings,
                     float             velocity,
                     const SampleBuffer& sample,
                     const SliceEngine&  slices,
                     double            playbackRate);

    /** Stop all active voices for the given pad index. */
    void stopPad (int padIndex);

    /** Stop all voices immediately. */
    void allNotesOff();

    //==========================================================================
    /** Mix all active voices into @p outputBuffer.  Realtime-safe. */
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample,
                          int numSamples,
                          const SampleBuffer& sample);

    /** Number of voices currently active (for UI display / metering). */
    int getActiveVoiceCount() const;

    /** Returns true if any voice is currently active for the given pad index. */
    bool isAnyVoiceActiveForPad (int padIndex) const noexcept;

    //==========================================================================
    /** Set the solo mask used during rendering.
     *
     *  @param soloMask  Bitmask of pad indices that are soloed (bit N = pad N).
     *                   Pass 0 to disable solo gating (all pads audible).
     *
     *  Call this from the audio thread before renderNextBlock each block.
     */
    void setSoloMask (uint32_t soloMask) noexcept { soloMask_ = soloMask; }

private:
    //==========================================================================
    std::array<PadVoice, kMaxVoices> voices_;

    /** Bitmask of soloed pad indices.  0 means no solo active. */
    uint32_t soloMask_ { 0 };

    /** Returns a pointer to the first inactive voice, or nullptr if all busy. */
    PadVoice* findFreeVoice();

    /** Returns a pointer to the first active voice (oldest-first steal). */
    PadVoice* stealVoice();
};
