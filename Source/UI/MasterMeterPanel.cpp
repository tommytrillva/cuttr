#include "MasterMeterPanel.h"
#include "../PluginProcessor.h"

MasterMeterPanel::MasterMeterPanel (ChopprProcessor& processor)
    : processor_ (processor)
{
    startTimerHz (30);
}

void MasterMeterPanel::updateLevels (float l, float r) noexcept
{
    // Store raw peaks (called from audio thread)
    float cur = leftPeak_.load (std::memory_order_relaxed);
    while (l > cur && !leftPeak_.compare_exchange_weak (cur, l, std::memory_order_relaxed)) {}
    cur = rightPeak_.load (std::memory_order_relaxed);
    while (r > cur && !rightPeak_.compare_exchange_weak (cur, r, std::memory_order_relaxed)) {}
}

void MasterMeterPanel::timerCallback()
{
    const float newL = leftPeak_.exchange  (0.0f, std::memory_order_relaxed);
    const float newR = rightPeak_.exchange (0.0f, std::memory_order_relaxed);

    displayLeft_  = juce::jmax (newL, displayLeft_  * kDecayPerFrame);
    displayRight_ = juce::jmax (newR, displayRight_ * kDecayPerFrame);

    // Peak hold
    if (newL >= holdLeft_)  { holdLeft_  = newL; holdTimerLeft_  = kHoldFrames; }
    else if (holdTimerLeft_  > 0) --holdTimerLeft_;
    else holdLeft_ = 0.0f;

    if (newR >= holdRight_) { holdRight_ = newR; holdTimerRight_ = kHoldFrames; }
    else if (holdTimerRight_ > 0) --holdTimerRight_;
    else holdRight_ = 0.0f;

    repaint();
}

void MasterMeterPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll (juce::Colour (0xff1a1a1a));

    // Draw label
    g.setColour (juce::Colours::grey);
    g.setFont (juce::Font (10.0f));
    g.drawText ("OUT", bounds.removeFromTop (12), juce::Justification::centred);

    // Two vertical bars (L and R)
    const int barW   = (bounds.getWidth() - 6) / 2;
    const auto lRect = bounds.removeFromLeft (barW);
    bounds.removeFromLeft (6);
    const auto rRect = bounds;

    auto drawBar = [&](juce::Rectangle<int> r, float level, float hold, bool clip)
    {
        // Background
        g.setColour (juce::Colour (0xff2a2a2a));
        g.fillRect (r);

        // Level fill (bottom to top)
        const float normalised = juce::jlimit (0.0f, 1.0f, level);
        const int   fillH      = juce::roundToInt (normalised * r.getHeight());
        const auto  fillRect   = r.removeFromBottom (fillH);

        // Colour: green -> yellow -> red
        const juce::Colour col = normalised < 0.75f ? juce::Colour (0xff00cc44)
                                                     : normalised < 0.9f
                                                           ? juce::Colour (0xffddcc00)
                                                           : juce::Colour (0xffdd2222);
        g.setColour (col);
        g.fillRect (fillRect);

        // Peak hold line
        if (hold > 0.01f)
        {
            const int holdY = r.getY() + juce::roundToInt ((1.0f - hold) * r.getHeight());
            g.setColour (clip ? juce::Colours::red : juce::Colours::white);
            g.drawHorizontalLine (holdY, (float)r.getX(), (float)r.getRight());
        }
    };

    drawBar (lRect, displayLeft_,  holdLeft_,  holdLeft_  >= kClipThreshold);
    drawBar (rRect, displayRight_, holdRight_, holdRight_ >= kClipThreshold);
}

void MasterMeterPanel::resized() {}
