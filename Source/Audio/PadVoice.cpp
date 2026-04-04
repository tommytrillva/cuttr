#include "PadVoice.h"

#include <cmath>

//==============================================================================
void PadVoice::prepare (double sampleRate, int blockSize)
{
    sampleRate_ = sampleRate;
    active_.store (false);
    position_    = 0.0;
    playbackRate_ = 1.0;
    pitchEngine_.prepare (sampleRate, blockSize);
}

//==============================================================================
void PadVoice::trigger (int                padIndex,
                        const PadSettings&  settings,
                        float              velocity,
                        const SampleBuffer& sample,
                        const SliceEngine&  slices,
                        double             playbackRate)
{
    // Muted pads produce no voice
    if (settings.mute)
        return;

    // Determine the slice boundaries
    const int sliceIdx = settings.sliceIndex;
    const int totalSamples = sample.getNumSamples();

    if (totalSamples <= 0)
        return;

    sliceStart_ = slices.getSliceStart (sliceIdx);
    sliceEnd_   = slices.getSliceEnd   (sliceIdx, totalSamples);

    // Guard against degenerate slices
    if (sliceEnd_ <= sliceStart_)
        return;

    padIndex_    = padIndex;
    chokeGroup_  = settings.chokeGroup;
    volume_      = settings.volume;
    pan_         = juce::jlimit (-1.0f, 1.0f, settings.pan);
    velocity_    = juce::jlimit (0.0f,  1.0f, velocity);
    reverse_     = settings.reverse;
    padSettings_ = settings;

    // For buffer-processing modes (Tonal / Percussive) the pitch shift is
    // applied to the filled output buffer — playback runs at normal speed.
    // For Chromatic / None we use the caller-supplied rate multiplier.
    playbackRate_ = pitchEngine_.requiresBufferProcessing() ? 1.0 : playbackRate;

    // Set read-head start position
    if (reverse_)
        position_ = static_cast<double> (sliceEnd_ - 1);
    else
        position_ = static_cast<double> (sliceStart_);

    active_.store (true);
}

//==============================================================================
void PadVoice::stop()
{
    active_.store (false);
}

//==============================================================================
bool PadVoice::isActive()    const { return active_.load(); }
int  PadVoice::getPadIndex() const { return padIndex_; }
int  PadVoice::getChokeGroup() const { return chokeGroup_; }

//==============================================================================
void PadVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                int startSample,
                                int numSamples,
                                const SampleBuffer& sample)
{
    // Cache active_ once with acquire semantics — avoids a redundant atomic
    // load on every sample in the inner loop.
    if (! active_.load (std::memory_order_acquire))
        return;

    const int outChannels = outputBuffer.getNumChannels();
    if (outChannels == 0)
        return;

    // Equal-power panning constants
    // pan_ is -1 (hard left) .. 0 (centre) .. +1 (hard right)
    // Angle: 0 (left) .. pi/2 (right), mapped from -1..+1
    // angle = (pan_ + 1) * pi/4    => 0..pi/2
    // L = cos(angle) * sqrt(2),  R = sin(angle) * sqrt(2)
    const float angle     = (pan_ + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    const float gainL     = std::cos (angle) * juce::MathConstants<float>::sqrt2;
    const float gainR     = std::sin (angle) * juce::MathConstants<float>::sqrt2;
    const float ampScale  = volume_ * velocity_;

    float* outL = outputBuffer.getWritePointer (0, startSample);
    float* outR = (outChannels >= 2) ? outputBuffer.getWritePointer (1, startSample) : nullptr;

    // Combined pitch offset: semitones + fine tune converted to semitones
    const float totalSemitones = padSettings_.pitchOffsetSemitones
                               + padSettings_.fineTuneCents / 100.0f;

    // For buffer-processing pitch modes (Tonal / Percussive) we write samples
    // into a temporary buffer at rate 1.0 so that timing is preserved.  The
    // pitch shift is then applied in-place by PitchShiftEngine::processBuffer()
    // and the result is mixed back into the output.
    const bool useBufferPitch = pitchEngine_.requiresBufferProcessing()
                                && std::abs (totalSemitones) > 0.001f;

    // Temporary per-voice buffer used only in buffer-pitch mode.
    juce::AudioBuffer<float> tempBuf;
    float* tempL = nullptr;
    float* tempR = nullptr;

    if (useBufferPitch)
    {
        tempBuf.setSize (outChannels, numSamples, false, true, true);
        tempL = tempBuf.getWritePointer (0);
        tempR = (outChannels >= 2) ? tempBuf.getWritePointer (1) : nullptr;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float sL = 0.0f;
        float sR = 0.0f;
        sample.getInterpolatedSample (position_, sL, sR);

        if (useBufferPitch)
        {
            // Write scaled sample into temp buffer; mix into output after pitch shift
            tempL[i] = sL * gainL * ampScale;
            if (tempR != nullptr)
                tempR[i] = sR * gainR * ampScale;
            else
                tempL[i] += sR * gainR * ampScale; // fold right into mono out
        }
        else
        {
            outL[i] += sL * gainL * ampScale;
            if (outR != nullptr)
                outR[i] += sR * gainR * ampScale;
            else
                outL[i] += sR * gainR * ampScale; // fold right into mono out
        }

        // Advance playhead
        if (reverse_)
        {
            position_ -= playbackRate_;
            if (position_ < static_cast<double> (sliceStart_))
                active_.store (false);
        }
        else
        {
            position_ += playbackRate_;
            if (position_ >= static_cast<double> (sliceEnd_))
                active_.store (false);
        }
    }

    // Apply buffer-based pitch shift and mix result into output
    if (useBufferPitch)
    {
        pitchEngine_.processBuffer (tempBuf, totalSemitones);

        for (int ch = 0; ch < outChannels; ++ch)
        {
            juce::FloatVectorOperations::add (
                outputBuffer.getWritePointer (ch, startSample),
                tempBuf.getReadPointer (ch),
                numSamples);
        }
    }
}
