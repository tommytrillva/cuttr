#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common.h"

class ChopprProcessor;

//==============================================================================
/**
 * TimeStretchPanel
 *
 * Side-panel for time-stretch controls:
 *   - Section title: "TIME STRETCH"
 *   - Mode combo box  (None / Match BPM / Per-Pad / Free)
 *   - Stretch ratio slider  (0.5 – 2.0, active in Free mode)
 */
class TimeStretchPanel : public juce::Component,
                         public juce::ComboBox::Listener,
                         public juce::Slider::Listener
{
public:
    //==========================================================================
    explicit TimeStretchPanel (ChopprProcessor& processor);
    ~TimeStretchPanel() override = default;

    //==========================================================================
    // juce::Component
    void paint   (juce::Graphics& g) override;
    void resized () override;

    //==========================================================================
    // juce::ComboBox::Listener
    void comboBoxChanged (juce::ComboBox* box) override;

    //==========================================================================
    // juce::Slider::Listener
    void sliderValueChanged (juce::Slider* slider) override;

    //==========================================================================
    /** Sync widgets to current processor state. */
    void refresh();

private:
    //==========================================================================
    /** Enable / disable the ratio slider depending on the selected mode. */
    void updateRatioSliderState();

    //==========================================================================
    ChopprProcessor& processor_;

    juce::Label    titleLabel_;
    juce::Label    modeLabel_;
    juce::ComboBox modeComboBox_;
    juce::Label    stretchRatioLabel_;
    juce::Slider   stretchRatioSlider_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeStretchPanel)
};
