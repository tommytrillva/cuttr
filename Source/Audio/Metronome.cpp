#include "Metronome.h"

#include <cmath>

//==============================================================================
void Metronome::prepare (double sampleRate, int /*blockSize*/)
{
    sampleRate_ = sampleRate;
    recalcSamplesPerBeat();
    reset();
}

void Metronome::recalcSamplesPerBeat()
{
    samplesPerBeat_ = sampleRate_ * 60.0 / static_cast<double> (bpm_);
}

//==============================================================================
void Metronome::processBlock (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (! enabled_)
    {
        samplePosition_ += static_cast<double> (numSamples);
        return;
    }

    const double clickDurationSamples = kClickDurationS * sampleRate_;
    const double twoPi  = juce::MathConstants<double>::twoPi;
    const double phaseIncrement = twoPi * kClickFreqHz / sampleRate_;

    const int numChannels = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        // Detect beat crossings
        const double prevPos    = samplePosition_;
        const double nextPos    = prevPos + 1.0;
        const double prevBeat   = prevPos / samplesPerBeat_;
        const double nextBeat   = nextPos / samplesPerBeat_;

        const bool beatCrossing = (static_cast<int> (nextBeat) > static_cast<int> (prevBeat));

        if (beatCrossing)
        {
            // Determine whether this is a downbeat (beat 1 of a bar)
            const int beatIndex = static_cast<int> (nextBeat) % timeSigNum_;
            clickGain_ = (beatIndex == 0) ? 1.0f : 0.5f;
            clickPhase_ = 0.0;
        }

        // Generate click: active for the first kClickDurationSamples after a beat crossing
        // We track click presence by clickPhase_ wrapping: phase > 0 means we're in a click
        // Reset happens on beat crossing above, and we let phase advance naturally;
        // once the click duration has elapsed the sine amplitude is zero (we gate by time).
        //
        // Simple gate: compute sample offset since last beat start
        const double beatFraction  = std::fmod (samplePosition_, samplesPerBeat_);
        const bool   inClick       = beatFraction < clickDurationSamples;

        if (inClick)
        {
            const float sample = static_cast<float> (
                std::sin (clickPhase_) * static_cast<double> (clickGain_));

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] += sample;

            clickPhase_ += phaseIncrement;
            if (clickPhase_ >= twoPi)
                clickPhase_ -= twoPi;
        }
        else
        {
            // Outside click window – reset phase so next click starts cleanly
            clickPhase_ = 0.0;
        }

        samplePosition_ = nextPos;
    }
}

//==============================================================================
void Metronome::setTempo (float bpm)
{
    bpm_ = std::max (1.0f, bpm);
    recalcSamplesPerBeat();
}

void Metronome::setTimeSignature (int numerator, int denominator)
{
    timeSigNum_ = std::max (1, numerator);
    timeSigDen_ = std::max (1, denominator);
}

void Metronome::setEnabled (bool enabled)
{
    enabled_ = enabled;
}

void Metronome::reset()
{
    samplePosition_ = 0.0;
    clickPhase_     = 0.0;
}

//==============================================================================
void Metronome::tapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();

    // Discard taps that are more than 3 seconds apart (implies a new tap sequence)
    if (tapTimes_.size() > 0)
    {
        const double elapsed = now - tapTimes_.getLast();
        if (elapsed > 3000.0)
            tapTimes_.clear();
    }

    tapTimes_.add (now);

    // Keep only the most recent kMaxTaps taps
    while (tapTimes_.size() > kMaxTaps)
        tapTimes_.remove (0);

    // Need at least 2 taps to compute an interval
    if (tapTimes_.size() < 2)
        return;

    // Average the intervals between consecutive taps
    double totalIntervalMs = 0.0;
    const int n = tapTimes_.size();
    for (int i = 1; i < n; ++i)
        totalIntervalMs += tapTimes_[i] - tapTimes_[i - 1];

    const double avgIntervalMs = totalIntervalMs / static_cast<double> (n - 1);
    if (avgIntervalMs <= 0.0)
        return;

    const float newBPM = static_cast<float> (60000.0 / avgIntervalMs);
    setTempo (newBPM);
}

//==============================================================================
float Metronome::getBPM() const
{
    return bpm_;
}

double Metronome::getPlayheadPositionSamples() const
{
    return samplePosition_;
}

double Metronome::getBeatPosition() const
{
    if (samplesPerBeat_ <= 0.0)
        return 0.0;
    return samplePosition_ / samplesPerBeat_;
}
