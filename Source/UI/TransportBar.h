#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common.h"

class ChopprProcessor;

//==============================================================================
/**
 * TransportBar
 *
 * Horizontal control strip containing:
 *   - Play / Stop / Record buttons
 *   - BPM label + slider (50–250)
 *   - Tap Tempo button
 *   - Time-signature label
 *   - Grid-size toggle button (4x4 / 4x8)
 */
class TransportBar : public juce::Component,
                     public juce::Slider::Listener,
                     public juce::Button::Listener
{
public:
    //==========================================================================
    explicit TransportBar (ChopprProcessor& processor);
    ~TransportBar() override = default;

    //==========================================================================
    // juce::Component
    void paint   (juce::Graphics& g) override;
    void resized () override;

    //==========================================================================
    // juce::Slider::Listener
    void sliderValueChanged (juce::Slider* slider) override;

    //==========================================================================
    // juce::Button::Listener
    void buttonClicked (juce::Button* button) override;

    //==========================================================================
    /** Sync widget states to processor (call after loading a preset). */
    void refresh();

private:
    //==========================================================================
    void applyButtonStyle (juce::TextButton& btn,
                           juce::Colour      normalBg,
                           juce::Colour      overBg,
                           juce::Colour      textColour);

    void updateGridSizeLabel();

    //==========================================================================
    ChopprProcessor& processor_;

    juce::TextButton playButton_     { "Play" };
    juce::TextButton stopButton_     { "Stop" };
    juce::TextButton recordButton_   { "Rec"  };
    juce::TextButton tapTempoButton_ { "Tap"  };
    juce::TextButton gridSizeButton_ { "4x4"  };

    juce::Label  bpmLabel_;
    juce::Slider bpmSlider_;

    juce::Label  timeSigLabel_;

    //==========================================================================
    // Internal state
    bool isGridSize4x8_ { false };   ///< false = 4x4, true = 4x8

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};
