#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common.h"

class ChopprProcessor;

//==============================================================================
/**
 * PitchPanel
 *
 * Side-panel for pitch controls:
 *   - Section title: "PITCH"
 *   - Global pitch offset slider  (-24 to +24 semitones)
 *   - Pitch mode combo box  (Chromatic / Tonal / Percussive)
 *   - Reset button
 */
class PitchPanel : public juce::Component,
                   public juce::Slider::Listener,
                   public juce::ComboBox::Listener,
                   public juce::Button::Listener
{
public:
    //==========================================================================
    explicit PitchPanel (ChopprProcessor& processor);
    ~PitchPanel() override = default;

    //==========================================================================
    // juce::Component
    void paint   (juce::Graphics& g) override;
    void resized () override;

    //==========================================================================
    // juce::Slider::Listener
    void sliderValueChanged (juce::Slider* slider) override;

    //==========================================================================
    // juce::ComboBox::Listener
    void comboBoxChanged (juce::ComboBox* box) override;

    //==========================================================================
    // juce::Button::Listener
    void buttonClicked (juce::Button* button) override;

    //==========================================================================
    /** Sync all widgets to the current processor state. */
    void refresh();

private:
    //==========================================================================
    ChopprProcessor& processor_;

    juce::Label    titleLabel_;
    juce::Label    globalPitchLabel_;
    juce::Slider   globalPitchSlider_;
    juce::Label    pitchModeLabel_;
    juce::ComboBox pitchModeComboBox_;
    juce::TextButton resetButton_ { "Reset" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchPanel)
};
