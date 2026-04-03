#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common.h"

class ChopprProcessor;

//==============================================================================
/**
 * SliceEditorPanel
 *
 * Side-panel for controlling automatic slice detection.
 * Contains:
 *   - Section title: "SLICER"
 *   - Slice mode combo box  (Transient / Equal / Beat / Bar)
 *   - "Auto Slice" button
 *   - "Clear" button
 *   - Slice count read-out label
 */
class SliceEditorPanel : public juce::Component,
                         public juce::ComboBox::Listener,
                         public juce::Button::Listener
{
public:
    //==========================================================================
    explicit SliceEditorPanel (ChopprProcessor& processor);
    ~SliceEditorPanel() override = default;

    //==========================================================================
    // juce::Component
    void paint   (juce::Graphics& g) override;
    void resized () override;

    //==========================================================================
    // juce::ComboBox::Listener
    void comboBoxChanged (juce::ComboBox* box) override;

    //==========================================================================
    // juce::Button::Listener
    void buttonClicked (juce::Button* button) override;

    //==========================================================================
    /** Re-read slice count from processor and update label. */
    void refreshSliceCount();

private:
    //==========================================================================
    SliceMode currentMode() const;

    //==========================================================================
    ChopprProcessor& processor_;

    juce::Label    titleLabel_;
    juce::Label    sliceModeLabel_;
    juce::ComboBox sliceModeComboBox_;
    juce::TextButton autoSliceButton_  { "Auto Slice" };
    juce::TextButton clearSlicesButton_{ "Clear"       };
    juce::Label    sliceCountLabel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceEditorPanel)
};
