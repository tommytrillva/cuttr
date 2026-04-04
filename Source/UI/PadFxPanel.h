#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common.h"

class ChopprProcessor;

class PadFxPanel : public juce::Component
{
public:
    explicit PadFxPanel (ChopprProcessor& processor);
    ~PadFxPanel() override = default;

    void paint  (juce::Graphics&) override;
    void resized() override;

    /** Called by the editor when a pad is selected. -1 = none. */
    void padSelected (int padIndex);

private:
    ChopprProcessor& processor_;
    int selectedPad_ { -1 };

    // ── Helpers ──────────────────────────────────────────────────────────
    void loadFromPad();
    void saveToPad();

    // ── Section header labels ────────────────────────────────────────────
    juce::Label filterHeader_    { {}, "FILTER" };
    juce::Label distHeader_      { {}, "DISTORTION" };
    juce::Label reverbHeader_    { {}, "REVERB" };
    juce::Label delayHeader_     { {}, "DELAY" };

    // ── Filter ───────────────────────────────────────────────────────────
    juce::ToggleButton filterEnable_    { "On" };
    juce::ComboBox     filterTypeCombo_;
    juce::Slider       filterCutoff_;
    juce::Label        filterCutoffLabel_   { {}, "Cutoff" };
    juce::Slider       filterRes_;
    juce::Label        filterResLabel_      { {}, "Reso" };

    // ── Distortion ───────────────────────────────────────────────────────
    juce::ToggleButton distEnable_  { "On" };
    juce::Slider       distDrive_;
    juce::Label        distDriveLabel_  { {}, "Drive" };
    juce::Slider       distMix_;
    juce::Label        distMixLabel_    { {}, "Mix" };

    // ── Reverb ───────────────────────────────────────────────────────────
    juce::ToggleButton revEnable_   { "On" };
    juce::Slider       revRoom_;
    juce::Label        revRoomLabel_    { {}, "Room" };
    juce::Slider       revDamp_;
    juce::Label        revDampLabel_    { {}, "Damp" };
    juce::Slider       revWidth_;
    juce::Label        revWidthLabel_   { {}, "Width" };
    juce::Slider       revMix_;
    juce::Label        revMixLabel_     { {}, "Mix" };

    // ── Delay ────────────────────────────────────────────────────────────
    juce::ToggleButton delEnable_   { "On" };
    juce::Slider       delTime_;
    juce::Label        delTimeLabel_    { {}, "Time" };
    juce::Slider       delFeedback_;
    juce::Label        delFeedLabel_    { {}, "Fdbk" };
    juce::Slider       delMix_;
    juce::Label        delMixLabel_     { {}, "Mix" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadFxPanel)
};
