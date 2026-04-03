#include "PadVoicePool.h"

//==============================================================================
void PadVoicePool::prepare (double sampleRate, int blockSize)
{
    for (auto& v : voices_)
        v.prepare (sampleRate, blockSize);
}

//==============================================================================
void PadVoicePool::triggerPad (int               padIndex,
                               const PadSettings& settings,
                               float             velocity,
                               const SampleBuffer& sample,
                               const SliceEngine&  slices,
                               double            playbackRate)
{
    // 1. Choke: stop any voices in the same choke group (non-zero group only)
    if (settings.chokeGroup != 0)
    {
        for (auto& v : voices_)
        {
            if (v.isActive() && v.getChokeGroup() == settings.chokeGroup)
                v.stop();
        }
    }

    // 2. Find a free voice, or steal one
    PadVoice* voice = findFreeVoice();
    if (voice == nullptr)
        voice = stealVoice();

    if (voice == nullptr)
        return; // Should never happen with kMaxVoices >= 1

    voice->trigger (padIndex, settings, velocity, sample, slices, playbackRate);
}

//==============================================================================
void PadVoicePool::stopPad (int padIndex)
{
    for (auto& v : voices_)
    {
        if (v.isActive() && v.getPadIndex() == padIndex)
            v.stop();
    }
}

void PadVoicePool::allNotesOff()
{
    for (auto& v : voices_)
        v.stop();
}

//==============================================================================
void PadVoicePool::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                    int startSample,
                                    int numSamples,
                                    const SampleBuffer& sample)
{
    for (auto& v : voices_)
    {
        if (v.isActive())
            v.renderNextBlock (outputBuffer, startSample, numSamples, sample);
    }
}

//==============================================================================
int PadVoicePool::getActiveVoiceCount() const
{
    int count = 0;
    for (const auto& v : voices_)
        if (v.isActive())
            ++count;
    return count;
}

//==============================================================================
PadVoice* PadVoicePool::findFreeVoice()
{
    for (auto& v : voices_)
        if (! v.isActive())
            return &v;
    return nullptr;
}

PadVoice* PadVoicePool::stealVoice()
{
    // Steal the first active voice (oldest-first, since voices_ is an array
    // and triggerPad always claims the lowest-index free/stolen voice).
    for (auto& v : voices_)
        if (v.isActive())
            return &v;
    return nullptr;
}
