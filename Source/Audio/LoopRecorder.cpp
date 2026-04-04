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

    // In non-overdub mode, discard previous layers for a fresh start.
    if (! overdubMode_)
        layers_.clear();

    // Allocate a fresh currentLayer_ for this pass.
    const int numChannels = 2; // stereo capture
    currentLayer_.setSize (numChannels,
                           std::max (1, totalLoopSamples_),
                           /*keepExistingContent=*/ false,
                           /*clearExtraSpace=*/     true,
                           /*avoidReallocating=*/   false);
    currentLayer_.clear();

    // Also (re)size recordBuffer_ so getRecordedBuffer() is always valid.
    recordBuffer_.setSize (numChannels,
                           std::max (1, totalLoopSamples_),
                           /*keepExistingContent=*/ false,
                           /*clearExtraSpace=*/     true,
                           /*avoidReallocating=*/   false);
    recordBuffer_.clear();

    recordPos_      = 0;
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

    // Commit the current layer to the stack and rebuild the mixed output.
    if (currentLayer_.getNumSamples() > 0)
    {
        layers_.push_back (std::move (currentLayer_));
        currentLayer_.setSize (0, 0); // reset after move
        mixLayersToBuffer (recordBuffer_);
    }
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
    const int recChannels = currentLayer_.getNumChannels();

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

        currentLayer_.copyFrom (ch,                             // destChannel
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
    currentLayer_.setSize (0, 0);
    layers_.clear();
    recordPos_      = 0;
    preRollCounter_ = 0;
}

//==============================================================================
bool LoopRecorder::undoLastLayer()
{
    if (layers_.empty())
        return false;

    layers_.pop_back();
    mixLayersToBuffer (recordBuffer_);
    return true;
}

//==============================================================================
void LoopRecorder::mixLayersToBuffer (juce::AudioBuffer<float>& dest) const
{
    if (layers_.empty())
    {
        dest.clear();
        return;
    }

    // Size dest to the longest layer
    int maxSamples = 0;
    int numCh = 2;
    for (const auto& layer : layers_)
    {
        maxSamples = juce::jmax (maxSamples, layer.getNumSamples());
        numCh      = juce::jmax (numCh,      layer.getNumChannels());
    }

    dest.setSize (numCh, maxSamples, false, true, true);
    dest.clear();

    // Additive mix
    for (const auto& layer : layers_)
        for (int ch = 0; ch < juce::jmin (numCh, layer.getNumChannels()); ++ch)
            dest.addFrom (ch, 0, layer, ch, 0, layer.getNumSamples());

    // Normalise to prevent clipping if multiple loud layers
    const float peak = dest.getMagnitude (0, maxSamples);
    if (peak > 1.0f)
        dest.applyGain (1.0f / peak);
}
