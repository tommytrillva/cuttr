#include "LoopRecorder.h"

//==============================================================================
void LoopRecorder::prepare (double sampleRate, int /*blockSize*/)
{
    sampleRate_ = sampleRate;
}

//==============================================================================
void LoopRecorder::startRecording (double preRollBeats,
                                   double loopLengthBeats,
                                   float  bpm)
{
    const double safeBPM = static_cast<double> (std::max (1.0f, bpm));

    totalLoopSamples_ = static_cast<int> (
        loopLengthBeats * sampleRate_ * 60.0 / safeBPM);

    preRollSamples_ = static_cast<int> (
        preRollBeats * sampleRate_ * 60.0 / safeBPM);

    // Allocate the loop buffer.  This is the only allocation in the record
    // path – it happens here on a non-audio thread before the actual record
    // loop begins.
    const int numChannels = 2; // stereo capture
    recordBuffer_.setSize (numChannels,
                           std::max (1, totalLoopSamples_),
                           /*keepExistingContent=*/ false,
                           /*clearExtraSpace=*/     true,
                           /*avoidReallocating=*/   false);
    recordBuffer_.clear();

    recordPos_     = 0;
    preRollCounter_ = preRollSamples_;

    // Flag order matters: isPreRolling before isRecording
    isPreRolling_.store (preRollSamples_ > 0);
    isRecording_.store  (true);
}

//==============================================================================
void LoopRecorder::stopRecording()
{
    isRecording_.store  (false);
    isPreRolling_.store (false);
}

//==============================================================================
bool LoopRecorder::isRecording()  const { return isRecording_.load(); }
bool LoopRecorder::isPreRolling() const { return isPreRolling_.load(); }

//==============================================================================
void LoopRecorder::processBlock (const juce::AudioBuffer<float>& inputBuffer,
                                 int    numSamples,
                                 double /*currentBeatPosition*/)
{
    if (! isRecording_.load())
        return;

    const int inChannels  = inputBuffer.getNumChannels();
    const int recChannels = recordBuffer_.getNumChannels();

    int inputOffset = 0; // offset into the current block

    // ----- Pre-roll phase ---------------------------------------------------
    if (isPreRolling_.load())
    {
        const int preRollRemaining = preRollCounter_;
        const int consumeForPreRoll = std::min (numSamples, preRollRemaining);
        preRollCounter_ -= consumeForPreRoll;
        inputOffset      += consumeForPreRoll;

        if (preRollCounter_ <= 0)
            isPreRolling_.store (false);
    }

    // ----- Record phase -----------------------------------------------------
    if (isPreRolling_.load())
        return; // still in pre-roll; no samples to record yet

    const int samplesToRecord = numSamples - inputOffset;
    if (samplesToRecord <= 0)
        return;

    const int spaceRemaining = totalLoopSamples_ - recordPos_;
    const int numToCopy      = std::min (samplesToRecord, spaceRemaining);

    if (numToCopy <= 0)
    {
        // Buffer full – stop automatically
        stopRecording();
        return;
    }

    for (int ch = 0; ch < recChannels; ++ch)
    {
        const int srcCh = (ch < inChannels) ? ch : (inChannels - 1);
        if (srcCh < 0) continue; // no input channels at all

        recordBuffer_.copyFrom (ch,                             // destChannel
                                recordPos_,                     // destStartSample
                                inputBuffer,                    // source
                                srcCh,                          // sourceChannel
                                inputOffset,                    // sourceStartSample
                                numToCopy);                     // numSamples
    }

    recordPos_ += numToCopy;

    if (recordPos_ >= totalLoopSamples_)
        stopRecording();
}

//==============================================================================
const juce::AudioBuffer<float>& LoopRecorder::getRecordedBuffer() const
{
    return recordBuffer_;
}

void LoopRecorder::clearRecording()
{
    isRecording_.store  (false);
    isPreRolling_.store (false);
    recordBuffer_.setSize (0, 0);
    recordPos_     = 0;
    preRollCounter_ = 0;
}
