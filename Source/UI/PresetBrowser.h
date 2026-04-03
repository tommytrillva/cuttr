#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common.h"

class ChopprProcessor;

//==============================================================================
/**
 * PresetBrowser
 *
 * Panel for saving, loading, and deleting named presets:
 *   - Section title: "PRESETS"
 *   - ListBox of discovered .choppr preset files
 *   - Save / Save As / Load / Delete buttons
 *
 * Presets are stored as ValueTree XML in:
 *   <userApplicationDataDirectory>/CHOPPR/Presets/*.choppr
 */
class PresetBrowser : public juce::Component,
                      public juce::Button::Listener,
                      public juce::ListBoxModel
{
public:
    //==========================================================================
    explicit PresetBrowser (ChopprProcessor& processor);
    ~PresetBrowser() override = default;

    //==========================================================================
    // juce::Component
    void paint   (juce::Graphics& g) override;
    void resized () override;

    //==========================================================================
    // juce::Button::Listener
    void buttonClicked (juce::Button* button) override;

    //==========================================================================
    // juce::ListBoxModel
    int     getNumRows()                                            override;
    void    paintListBoxItem (int row, juce::Graphics& g,
                              int width, int height,
                              bool rowIsSelected)                   override;
    void    listBoxItemDoubleClicked (int row,
                                     const juce::MouseEvent&)      override;

    //==========================================================================
    /** Scan the preset directory and repopulate presetNames_. */
    void refreshPresets();

private:
    //==========================================================================
    /** Return the CHOPPR preset directory (creates it if absent). */
    juce::File presetDirectory() const;

    /** Return the full path for a preset by display name. */
    juce::File presetFile (const juce::String& name) const;

    /** Save current state under @p name, overwriting if the file exists. */
    void savePresetAs (const juce::String& name);

    /** Load the preset file whose display name equals @p name. */
    void loadPreset (const juce::String& name);

    /** Delete the preset file whose display name equals @p name. */
    void deletePreset (const juce::String& name);

    //==========================================================================
    ChopprProcessor& processor_;

    juce::Label    titleLabel_;
    juce::ListBox  presetListBox_;
    juce::TextButton saveButton_     { "Save"    };
    juce::TextButton saveAsButton_   { "Save As" };
    juce::TextButton loadButton_     { "Load"    };
    juce::TextButton deleteButton_   { "Delete"  };

    /** Display names (without extension) for all found .choppr files. */
    juce::Array<juce::String> presetNames_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};
