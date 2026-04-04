#pragma once

#include "../Common.h"
#include "SampleBuffer.h"
#include "SliceEngine.h"
#include "PitchShiftEngine.h"

#include <atomic>

//==============================================================================
/**
 * A single polyphonic voice that plays one slice of a SampleBuffer.
 *
 * All render-path methods are realtime-safe (no allocation, no lock contention
 * on the fast path).  trigger() may allocate nothing and is called from the
 * audio thread via PadVoicePool.
 */
class PadVoice
{
public:
    PadVoice()  = default;
    ~PadVoice() = default;

    //==========================================================================
    /** Called once during plugin initialisation / format change. */
    void prepare (double sampleRate, int blockSize);

    //==========================================================================
    /** Start playing a slice.
     *
     *  @param padIndex     Which pad triggered this voice (for choke lookup).
     *  @param settings     PadSettings for this pad.
     *  @param velocity     MIDI velocity (0..1).
     *  @param sample       The loaded sample to read from.
     *  @param slices       Slice layout.
     *  @param playbackRate Combined rate from PitchShiftEngine + TimeStretchEngine.
     */
    void trigger (int               padIndex,
                  const PadSettings& settings,
                  float             velocity,
                  const SampleBuffer& sample,
                  const SliceEngine&  slices,
                  double            playbackRate);

    /** Hard-stop this voice immediately. */
    void stop();

    /** Called when the MIDI note-off arrives for this pad.
     *  Gate/Loop modes fade out; OneShot ignores the release. */
    void noteOff() noexcept;

    //==========================================================================
    bool isActive()    const;
    int  getPadIndex() const;
    int  getChokeGroup() const;

    //==========================================================================
    /** Mix this voice's output into @p outputBuffer[startSample .. +numSamples].
     *  Realtime-safe.  Deactivates itself when the slice endpoint is reached.
     */
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample,
                          int numSamples,
                          const SampleBuffer& sample);

private:
    //==========================================================================
    int    padIndex_    { -1 };
    int    chokeGroup_  { 0 };

    std::atomic<bool> active_         { false };
    std::atomic<bool> noteOffPending_ { false };

    double position_     { 0.0 };   // current fractional sample read-head
    double playbackRate_ { 1.0 };

    float  volume_   { 1.0f };
    float  pan_      { 0.0f };  // -1..+1
    float  velocity_ { 1.0f };
    bool   reverse_  { false };

    int    sliceStart_ { 0 };
    int    sliceEnd_   { 0 };

    double sampleRate_ { 44100.0 };

    PadSettings       padSettings_;   // Copy of settings from last trigger()
    PitchShiftEngine  pitchEngine_;   // Per-voice pitch shift engine
};
