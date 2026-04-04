#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common.h"
#include "../Util/TransientDetector.h"

class ChopprProcessor;

//==============================================================================
/**
 * SliceEditorPanel
 *
 * Side-panel for controlling automatic slice detection and per-pad editing.
 *
 * Sections (top to bottom):
 *   1. Transient Detection parameters  (~120 px)
 *   2. Slice mode combo + Auto Slice + Clear buttons  (~80 px)
 *   3. Per-Pad editing controls (shown only when a pad is selected)
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

    /** Called by the pad grid / other UI to notify which pad is selected.
     *  Pass -1 to deselect (hides per-pad section). */
    void padSelected (int padIndex);

private:
    //==========================================================================
    SliceMode currentMode() const;

    /** Rebuild a TransientDetector::Settings from the current slider values. */
    TransientDetector::Settings buildDetectorSettings() const;

    /** Helper: style a horizontal Slider. */
    void styleSlider (juce::Slider& s, double lo, double hi, double def,
                      const juce::String& suffix = {});

    //==========================================================================
    // Core reference
    ChopprProcessor& processor_;

    //==========================================================================
    // --- Section header labels ---
    juce::Label sectionTransientLabel_;   // "TRANSIENT DETECTION"
    juce::Label sectionModeLabel_;        // "SLICER"
    juce::Label sectionPadLabel_;         // "PAD SETTINGS"

    //==========================================================================
    // --- Transient Detection parameters ---

    TransientDetector::Settings currentSettings_;

    // Row 1
    juce::Slider sensitivitySlider_;
    juce::Label  sensitivityLabel_;

    juce::Slider minSliceLenSlider_;      // ms  (50 – 2000)
    juce::Label  minSliceLenLabel_;

    // Row 2
    juce::Slider maxSlicesSlider_;        // 4 – 64  (integer)
    juce::Label  maxSlicesLabel_;

    juce::ComboBox bandFocusCombo_;       // Off / Low / Mid / High
    juce::Label    bandFocusLabel_;

    //==========================================================================
    // --- Slice mode / action controls ---
    juce::Label    sliceModeLabel_;
    juce::ComboBox sliceModeComboBox_;
    juce::TextButton autoSliceButton_  { "Auto Slice" };
    juce::TextButton clearSlicesButton_{ "Clear"       };
    juce::Label      sliceCountLabel_;

    // Warp to Grid controls
    juce::Label      warpSubdivLabel_;
    juce::ComboBox   warpSubdivCombo_;
    juce::TextButton warpToGridBtn_ { "Warp to Grid" };

    //==========================================================================
    // --- Per-pad editing section ---
    int selectedPad_ { -1 };

    juce::Label padTitleLabel_;            // "Pad 1" etc.

    juce::Slider padVolumeSlider_;         // 0.0 – 2.0, default 1.0
    juce::Label  padVolumeLabel_;

    juce::Slider padPanSlider_;            // -1.0 – 1.0
    juce::Label  padPanLabel_;

    juce::Slider padPitchSlider_;          // -12 – +12 semitones
    juce::Label  padPitchLabel_;

    juce::Slider padFineTuneSlider_;       // -100 – +100 cents
    juce::Label  padFineTuneLabel_;

    juce::ToggleButton padReverseBtn_ { "Reverse" };
    juce::ToggleButton padMuteBtn_    { "Mute"    };
    juce::ToggleButton padSoloBtn_    { "Solo"    };

    juce::ComboBox chokeGroupCombo_;       // "None", "Group 1"…"Group 8"
    juce::Label    chokeGroupLabel_;

    juce::ComboBox padModeCombo_;          // "One-shot", "Gate", "Loop"
    juce::Label    padModeLabel_;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceEditorPanel)
};
