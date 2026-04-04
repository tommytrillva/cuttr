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

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
    spec.numChannels      = 2;

    filter_.prepare (spec);
    filter_.reset();

    reverb_.setSampleRate (sampleRate);
    reverb_.reset();
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

    noteOffPending_.store (false, std::memory_order_relaxed);
    active_.store (true);

    // Reset FX state so stale reverb tails / delay echoes don't bleed into
    // a newly-triggered voice on this pad.
    filter_.reset();
    reverb_.reset();
    delayWritePos_ = 0;
    delayBufL_.fill (0.0f);
    delayBufR_.fill (0.0f);
}

//==============================================================================
void PadVoice::stop()
{
    noteOffPending_.store (false, std::memory_order_relaxed);
    active_.store (false);
}

//==============================================================================
void PadVoice::noteOff() noexcept
{
    if (padSettings_.playMode == PadPlayMode::Gate ||
        padSettings_.playMode == PadPlayMode::Loop)
    {
        // Short fade-out to avoid click (applied over next ~64 samples in render)
        noteOffPending_.store (true, std::memory_order_relaxed);
    }
    // OneShot: ignore note-off, play to end
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

    // Gate / Loop: if a note-off arrived, apply a short linear fade-out
    // over the first fadeSamples samples of this block, then silence the rest.
    if (noteOffPending_.load (std::memory_order_relaxed))
    {
        const int fadeSamples = juce::jmin (64, numSamples);
        const int outChannels = outputBuffer.getNumChannels();
        for (int ch = 0; ch < outChannels; ++ch)
        {
            float* ptr = outputBuffer.getWritePointer (ch, startSample);
            for (int i = 0; i < fadeSamples; ++i)
                ptr[i] *= static_cast<float> (fadeSamples - i) / static_cast<float> (fadeSamples);
            for (int i = fadeSamples; i < numSamples; ++i)
                ptr[i] = 0.0f;
        }
        active_.store (false, std::memory_order_release);
        noteOffPending_.store (false, std::memory_order_relaxed);
        return;
    }

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
            {
                if (padSettings_.playMode == PadPlayMode::Loop)
                    position_ = static_cast<double> (sliceEnd_ - 1); // wrap to end for reverse loop
                else
                    active_.store (false); // OneShot / Gate: deactivate
            }
        }
        else
        {
            position_ += playbackRate_;
            if (position_ >= static_cast<double> (sliceEnd_))
            {
                if (padSettings_.playMode == PadPlayMode::Loop)
                    position_ = static_cast<double> (sliceStart_); // wrap back to start
                else
                    active_.store (false); // OneShot / Gate: deactivate
            }
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

    // Apply per-pad FX chain (Filter, Distortion, Reverb, Delay)
    applyFx (outputBuffer, startSample, numSamples);
}

//==============================================================================
void PadVoice::applyFx (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    // Read FX settings from the current pad
    const auto& fx = padSettings_.fx;
    const int numCh = buffer.getNumChannels();

    // ── Filter ───────────────────────────────────────────────────────────
    if (fx.filterEnabled)
    {
        using SVFType = juce::dsp::StateVariableTPTFilterType;
        switch (fx.filterType)
        {
            case 1:  filter_.setType (SVFType::highpass);  break;
            case 2:  filter_.setType (SVFType::bandpass);  break;
            default: filter_.setType (SVFType::lowpass);   break;
            // Note: JUCE 7 StateVariableTPTFilter doesn't have notch directly;
            // use lowpass for type 3 as fallback
        }
        filter_.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, fx.filterCutoff));
        filter_.setResonance       (juce::jlimit (0.1f,  10.0f,   fx.filterResonance));

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data = buffer.getWritePointer (ch, startSample);
            for (int i = 0; i < numSamples; ++i)
                data[i] = filter_.processSample (ch % 2, data[i]);
        }
    }

    // ── Distortion ───────────────────────────────────────────────────────
    if (fx.distortionEnabled)
    {
        const float drive    = juce::jlimit (1.0f, 20.0f, fx.distortionDrive);
        const float wetGain  = juce::jlimit (0.0f, 1.0f,  fx.distortionMix);
        const float dryGain  = 1.0f - wetGain;
        // Normalise so output doesn't exceed 0dBFS
        const float normGain = 1.0f / std::tanh (drive);

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data = buffer.getWritePointer (ch, startSample);
            for (int i = 0; i < numSamples; ++i)
            {
                const float dry = data[i];
                const float wet = std::tanh (drive * dry) * normGain;
                data[i] = dryGain * dry + wetGain * wet;
            }
        }
    }

    // ── Reverb ───────────────────────────────────────────────────────────
    if (fx.reverbEnabled)
    {
        juce::Reverb::Parameters p;
        p.roomSize   = juce::jlimit (0.0f, 1.0f, fx.reverbRoomSize);
        p.damping    = juce::jlimit (0.0f, 1.0f, fx.reverbDamping);
        p.width      = juce::jlimit (0.0f, 1.0f, fx.reverbWidth);
        p.wetLevel   = juce::jlimit (0.0f, 1.0f, fx.reverbMix);
        p.dryLevel   = 1.0f - p.wetLevel;
        p.freezeMode = 0.0f;
        reverb_.setParameters (p);

        if (numCh >= 2)
        {
            reverb_.processStereo (
                buffer.getWritePointer (0, startSample),
                buffer.getWritePointer (1, startSample),
                numSamples);
        }
        else if (numCh == 1)
        {
            reverb_.processMono (buffer.getWritePointer (0, startSample), numSamples);
        }
    }

    // ── Delay ────────────────────────────────────────────────────────────
    if (fx.delayEnabled)
    {
        const float wetGain = juce::jlimit (0.0f, 1.0f, fx.delayMix);
        const float dryGain = 1.0f - wetGain;
        const float fb      = juce::jlimit (0.0f, 0.95f, fx.delayFeedback);

        const int delaySamples = juce::jlimit (1,
            kMaxDelaySamples - 1,
            juce::roundToInt (fx.delayTimeMs * 0.001f * (float) sampleRate_));

        float* chL = buffer.getWritePointer (0, startSample);
        float* chR = (numCh > 1) ? buffer.getWritePointer (1, startSample) : chL;

        for (int i = 0; i < numSamples; ++i)
        {
            int readPos = (delayWritePos_ - delaySamples + kMaxDelaySamples) % kMaxDelaySamples;

            const float dL = delayBufL_[readPos];
            const float dR = delayBufR_[readPos];

            delayBufL_[delayWritePos_] = chL[i] + fb * dL;
            delayBufR_[delayWritePos_] = chR[i] + fb * dR;

            chL[i] = dryGain * chL[i] + wetGain * dL;
            chR[i] = dryGain * chR[i] + wetGain * dR;

            delayWritePos_ = (delayWritePos_ + 1) % kMaxDelaySamples;
        }
    }
}
