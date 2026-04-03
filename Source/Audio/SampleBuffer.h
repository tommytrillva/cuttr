#pragma once

#include "../Common.h"
#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>

//==============================================================================
/**
 * Thread-safe holder for a decoded audio sample.
 *
 * Loading is done on a background-friendly caller; the internal ReadWriteLock
 * allows the audio thread to read concurrently while a load is in progress on
 * another thread.  All audio-thread paths are allocation-free.
 */
class SampleBuffer
{
public:
    SampleBuffer();
    ~SampleBuffer() = default;

    // Non-copyable – share via pointer/reference
    SampleBuffer (const SampleBuffer&) = delete;
    SampleBuffer& operator= (const SampleBuffer&) = delete;

    //==========================================================================
    /** Load audio from file.  May be called from any non-audio thread.
     *  Returns true on success; on failure writes a description to errorMessage.
     *  Files larger than 500 MB are rejected. */
    bool loadFromFile (const juce::File& file, juce::String& errorMessage);

    /** Release all audio data. */
    void clear();

    /** True when valid audio data is held. */
    bool hasAudio() const;

    //==========================================================================
    /** Linear interpolation read.  Safe to call from the audio thread.
     *  @param position   fractional sample position (0 .. getNumSamples()-1)
     *  @param left       output for left / mono channel
     *  @param right      output for right channel (equals left for mono files)
     */
    void getInterpolatedSample (double position, float& left, float& right) const;

    //==========================================================================
    const juce::AudioBuffer<float>& getBuffer() const;
    juce::AudioBuffer<float>&       getBuffer();

    double getSampleRate()     const;
    int    getNumSamples()     const;
    int    getNumChannels()    const;
    double getDurationSeconds() const;
    juce::File getSourceFile() const;

private:
    //==========================================================================
    juce::AudioFormatManager     formatManager_;
    juce::AudioBuffer<float>     buffer_;
    double                       sampleRate_  { 44100.0 };
    juce::File                   sourceFile_;
    mutable juce::ReadWriteLock  lock_;
    std::atomic<bool>            isLoading_   { false };
    std::atomic<bool>            hasAudio_    { false };

    static constexpr juce::int64 kMaxFileSizeBytes = 500LL * 1024 * 1024; // 500 MB
};
