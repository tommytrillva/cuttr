#pragma once

#include "../Common.h"
#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

//==============================================================================
/**
 * Records live audio from the input buffer into a fixed-size loop buffer.
 *
 * A pre-roll period (counted in beats) precedes the actual capture; recording
 * begins automatically once the pre-roll elapses.
 *
 * The render / processBlock path is realtime-safe: no allocation occurs once
 * startRecording() has set up the buffers.
 */
class LoopRecorder
{
public:
    LoopRecorder()  = default;
    ~LoopRecorder() = default;

    //==========================================================================
    /** Call from prepareToPlay. */
    void prepare (double sampleRate, int blockSize);

    //==========================================================================
    /** Allocate the recording buffer and start the pre-roll countdown.
     *
     *  @param preRollBeats      Number of beats to count before recording starts.
     *  @param loopLengthBeats   Total length of the recording in beats.
     *  @param bpm               Tempo used to convert beats to samples.
     */
    void startRecording (double preRollBeats, double loopLengthBeats, float bpm);

    /** Stop recording immediately (buffer content is preserved). */
    void stopRecording();

    bool isRecording()  const;
    bool isPreRolling() const;

    //==========================================================================
    /** Feed input audio and advance the record head.
     *
     *  @param inputBuffer         Audio from the host input bus.
     *  @param numSamples          Number of samples in this block.
     *  @param currentBeatPosition Current playhead beat position (from Metronome).
     *                             Not used for timing logic in this implementation –
     *                             recording starts when the pre-roll sample count
     *                             elapses; the parameter is available for future use.
     */
    void processBlock (const juce::AudioBuffer<float>& inputBuffer,
                       int    numSamples,
                       double currentBeatPosition);

    //==========================================================================
    /** Const access to the recorded buffer (valid until clearRecording()). */
    const juce::AudioBuffer<float>& getRecordedBuffer() const;

    /** Erase the recording and reset all state. */
    void clearRecording();

private:
    //==========================================================================
    juce::AudioBuffer<float> recordBuffer_;
    int    recordPos_          { 0 };
    int    totalLoopSamples_   { 0 };
    int    preRollSamples_     { 0 };
    int    preRollCounter_     { 0 };  // counts down from preRollSamples_ to 0
    double sampleRate_         { 44100.0 };

    std::atomic<bool> isRecording_  { false };
    std::atomic<bool> isPreRolling_ { false };
};
