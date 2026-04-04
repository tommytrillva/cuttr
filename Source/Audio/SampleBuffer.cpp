#include "SampleBuffer.h"

#include <cmath>

//==============================================================================
SampleBuffer::SampleBuffer()
{
    formatManager_.registerBasicFormats();
}

//==============================================================================
void SampleBuffer::prepareToPlay (double sampleRate)
{
    // Store the session rate; subsequent loadFromFile calls will resample to it.
    sessionSampleRate_ = (sampleRate > 0.0) ? sampleRate : 44100.0;
}

//==============================================================================
bool SampleBuffer::loadFromFile (const juce::File& file, juce::String& errorMessage)
{
    if (! file.existsAsFile())
    {
        errorMessage = "File does not exist: " + file.getFullPathName();
        return false;
    }

    if (file.getSize() > kMaxFileSizeBytes)
    {
        errorMessage = "File exceeds 500 MB limit: " + file.getFullPathName();
        return false;
    }

    isLoading_.store (true);

    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager_.createReaderFor (file));

    if (reader == nullptr)
    {
        isLoading_.store (false);
        errorMessage = "Unsupported audio format or could not open file: " + file.getFullPathName();
        return false;
    }

    const int numChannels  = static_cast<int> (reader->numChannels);
    const int numSamples   = static_cast<int> (reader->lengthInSamples);
    const double fileSR    = reader->sampleRate;

    juce::AudioBuffer<float> newBuffer (numChannels, numSamples);

    if (! reader->read (&newBuffer, 0, numSamples, 0, true, true))
    {
        isLoading_.store (false);
        errorMessage = "Failed to read audio data from: " + file.getFullPathName();
        return false;
    }

    // --- Sample-rate conversion (off the audio thread) -----------------------
    // If the file's sample rate differs from the session sample rate, resample
    // so playback runs at the correct pitch and speed without any per-sample
    // rate compensation in the audio callback.
    double effectiveSR = fileSR;
    if (std::abs (fileSR - sessionSampleRate_) > 0.5 && fileSR > 0.0)
    {
        const double ratio       = sessionSampleRate_ / fileSR;
        const int    newNumSamples = juce::roundToInt (newBuffer.getNumSamples() * ratio);

        if (newNumSamples > 0)
        {
            juce::AudioBuffer<float> resampled (numChannels, newNumSamples);

            juce::LagrangeInterpolator interp;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                interp.reset();
                interp.process (1.0 / ratio,
                                newBuffer.getReadPointer (ch),
                                resampled.getWritePointer (ch),
                                newNumSamples);
            }

            newBuffer    = std::move (resampled);
            effectiveSR  = sessionSampleRate_;
        }
    }
    // -------------------------------------------------------------------------

    // Swap under write lock so audio-thread readers stay consistent
    {
        juce::ScopedWriteLock writeLock (lock_);
        buffer_            = std::move (newBuffer);
        nativeSampleRate_  = effectiveSR;
        sourceFile_        = file;
    }

    hasAudio_.store (true);
    isLoading_.store (false);
    return true;
}

//==============================================================================
void SampleBuffer::clear()
{
    juce::ScopedWriteLock writeLock (lock_);
    buffer_.setSize (0, 0);
    nativeSampleRate_ = 44100.0;
    sourceFile_ = juce::File();
    hasAudio_.store (false);
}

//==============================================================================
bool SampleBuffer::hasAudio() const
{
    return hasAudio_.load();
}

//==============================================================================
void SampleBuffer::getInterpolatedSample (double position,
                                          float& left,
                                          float& right) const
{
    // Use tryEnterRead() so the audio thread never blocks if a write is in
    // progress (e.g. a new file is being loaded on another thread).
    // If we cannot acquire the read lock immediately, output silence and return.
    if (! lock_.tryEnterRead())
    {
        left = right = 0.0f;
        return;
    }

    if (! hasAudio_.load() || buffer_.getNumSamples() == 0)
    {
        lock_.exitRead();
        left = right = 0.0f;
        return;
    }

    const int numSamples  = buffer_.getNumSamples();
    const int numChannels = buffer_.getNumChannels();

    // Clamp position to valid interpolation range
    const double clampedPos = juce::jlimit (0.0, static_cast<double> (numSamples - 1), position);

    const int    pos0  = static_cast<int> (clampedPos);
    const int    pos1  = juce::jmin (pos0 + 1, numSamples - 1);
    const float  frac  = static_cast<float> (clampedPos - static_cast<double> (pos0));

    if (numChannels >= 2)
    {
        const float* chL = buffer_.getReadPointer (0);
        const float* chR = buffer_.getReadPointer (1);
        left  = chL[pos0] + frac * (chL[pos1] - chL[pos0]);
        right = chR[pos0] + frac * (chR[pos1] - chR[pos0]);
    }
    else
    {
        // Mono: duplicate to both outputs
        const float* ch = buffer_.getReadPointer (0);
        left = right = ch[pos0] + frac * (ch[pos1] - ch[pos0]);
    }

    lock_.exitRead();
}

//==============================================================================
const juce::AudioBuffer<float>& SampleBuffer::getBuffer() const
{
    // Caller is responsible for taking the lock if needed outside audio thread
    return buffer_;
}

juce::AudioBuffer<float>& SampleBuffer::getBuffer()
{
    return buffer_;
}

double SampleBuffer::getSampleRate() const
{
    juce::ScopedReadLock readLock (lock_);
    return nativeSampleRate_;
}

int SampleBuffer::getNumSamples() const
{
    juce::ScopedReadLock readLock (lock_);
    return buffer_.getNumSamples();
}

int SampleBuffer::getNumChannels() const
{
    juce::ScopedReadLock readLock (lock_);
    return buffer_.getNumChannels();
}

double SampleBuffer::getDurationSeconds() const
{
    juce::ScopedReadLock readLock (lock_);
    if (nativeSampleRate_ <= 0.0 || buffer_.getNumSamples() == 0)
        return 0.0;
    return static_cast<double> (buffer_.getNumSamples()) / nativeSampleRate_;
}

juce::File SampleBuffer::getSourceFile() const
{
    juce::ScopedReadLock readLock (lock_);
    return sourceFile_;
}
