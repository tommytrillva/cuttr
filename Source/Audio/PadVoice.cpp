#include "PadVoice.h"

#include <cmath>

//==============================================================================
void PadVoice::prepare (double sampleRate, int /*blockSize*/)
{
    sampleRate_ = sampleRate;
    active_.store (false);
    position_    = 0.0;
    playbackRate_ = 1.0;
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
    playbackRate_ = playbackRate;

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
    if (! active_.load())
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

    for (int i = 0; i < numSamples; ++i)
    {
        if (! active_.load())
            break;

        float sL = 0.0f;
        float sR = 0.0f;
        sample.getInterpolatedSample (position_, sL, sR);

        outL[i] += sL * gainL * ampScale;
        if (outR != nullptr)
            outR[i] += sR * gainR * ampScale;
        else
            outL[i] += sR * gainR * ampScale; // fold right into mono out

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
}
