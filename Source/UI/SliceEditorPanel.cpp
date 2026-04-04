#include "SliceEditorPanel.h"
#include "../PluginProcessor.h"

//==============================================================================
// Colour palette
static const juce::Colour kBg        = juce::Colour (0xff1a1a2e);
static const juce::Colour kSurface   = juce::Colour (0xff16213e);
static const juce::Colour kSurface2  = juce::Colour (0xff1e2a40);
static const juce::Colour kAccent    = juce::Colour (0xffe94560);
static const juce::Colour kText      = juce::Colour (0xffeeeeee);
static const juce::Colour kDim       = juce::Colour (0xff888888);

//==============================================================================
// Static helpers
static void styleCombo (juce::ComboBox& cb)
{
    cb.setColour (juce::ComboBox::backgroundColourId,    juce::Colour (0xff1a1a2e));
    cb.setColour (juce::ComboBox::textColourId,           kText);
    cb.setColour (juce::ComboBox::outlineColourId,        juce::Colour (0xff333355));
    cb.setColour (juce::ComboBox::arrowColourId,          kAccent);
    cb.setColour (juce::ComboBox::focusedOutlineColourId, kAccent);
}

static void styleButton (juce::TextButton& btn)
{
    btn.setColour (juce::TextButton::buttonColourId,  kSurface);
    btn.setColour (juce::TextButton::buttonOnColourId, kAccent);
    btn.setColour (juce::TextButton::textColourOffId,  kText);
    btn.setColour (juce::TextButton::textColourOnId,   kText);
}

static void styleLabel (juce::Label& lbl,
                        const juce::String& text,
                        float fontSize = 11.0f,
                        juce::Colour colour = kDim,
                        bool bold = false)
{
    lbl.setText (text, juce::dontSendNotification);
    lbl.setFont (juce::Font (fontSize, bold ? juce::Font::bold : juce::Font::plain));
    lbl.setColour (juce::Label::textColourId, colour);
    lbl.setJustificationType (juce::Justification::centredLeft);
}

static void styleToggle (juce::ToggleButton& btn)
{
    btn.setColour (juce::ToggleButton::textColourId,     kText);
    btn.setColour (juce::ToggleButton::tickColourId,     kAccent);
    btn.setColour (juce::ToggleButton::tickDisabledColourId, kDim);
}

//==============================================================================
void SliceEditorPanel::styleSlider (juce::Slider& s,
                                    double lo, double hi, double def,
                                    const juce::String& suffix)
{
    s.setRange (lo, hi);
    s.setValue (def, juce::dontSendNotification);
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 18);
    if (suffix.isNotEmpty())
        s.setTextValueSuffix (suffix);
    s.setColour (juce::Slider::backgroundColourId,   kSurface);
    s.setColour (juce::Slider::trackColourId,         kAccent.withAlpha (0.5f));
    s.setColour (juce::Slider::thumbColourId,         kAccent);
    s.setColour (juce::Slider::textBoxTextColourId,   kText);
    s.setColour (juce::Slider::textBoxBackgroundColourId, kSurface2);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

//==============================================================================
SliceEditorPanel::SliceEditorPanel (ChopprProcessor& processor)
    : processor_ (processor)
{
    //==========================================================================
    // Section headers
    styleLabel (sectionTransientLabel_, "TRANSIENT DETECTION", 11.0f, kDim, true);
    addAndMakeVisible (sectionTransientLabel_);

    styleLabel (sectionModeLabel_, "SLICER", 13.0f, kText, true);
    addAndMakeVisible (sectionModeLabel_);

    styleLabel (sectionPadLabel_, "PAD SETTINGS", 11.0f, kDim, true);
    addAndMakeVisible (sectionPadLabel_);

    //==========================================================================
    // --- Transient Detection sliders ---

    // Sensitivity  0–100  (higher = more sensitive = lower detector threshold)
    styleLabel (sensitivityLabel_, "Sensitivity");
    addAndMakeVisible (sensitivityLabel_);

    styleSlider (sensitivitySlider_, 0.0, 100.0, 50.0);
    sensitivitySlider_.onValueChange = [this]
    {
        currentSettings_ = buildDetectorSettings();
    };
    addAndMakeVisible (sensitivitySlider_);

    // Min Slice Length  50–2000 ms
    styleLabel (minSliceLenLabel_, "Min Length");
    addAndMakeVisible (minSliceLenLabel_);

    styleSlider (minSliceLenSlider_, 50.0, 2000.0, 50.0, " ms");
    minSliceLenSlider_.onValueChange = [this]
    {
        currentSettings_ = buildDetectorSettings();
    };
    addAndMakeVisible (minSliceLenSlider_);

    // Max Slices  4–64
    styleLabel (maxSlicesLabel_, "Max Slices");
    addAndMakeVisible (maxSlicesLabel_);

    styleSlider (maxSlicesSlider_, 4.0, 64.0, 64.0);
    maxSlicesSlider_.setNumDecimalPlacesToDisplay (0);
    maxSlicesSlider_.onValueChange = [this]
    {
        currentSettings_ = buildDetectorSettings();
    };
    addAndMakeVisible (maxSlicesSlider_);

    // Band Focus combo
    styleLabel (bandFocusLabel_, "Band Focus");
    addAndMakeVisible (bandFocusLabel_);

    bandFocusCombo_.addItem ("Off",  1);
    bandFocusCombo_.addItem ("Low",  2);
    bandFocusCombo_.addItem ("Mid",  3);
    bandFocusCombo_.addItem ("High", 4);
    bandFocusCombo_.setSelectedId (1, juce::dontSendNotification);
    styleCombo (bandFocusCombo_);
    bandFocusCombo_.onChange = [this]
    {
        currentSettings_ = buildDetectorSettings();
    };
    addAndMakeVisible (bandFocusCombo_);

    // Initialise currentSettings_ from default slider positions
    currentSettings_ = buildDetectorSettings();

    //==========================================================================
    // --- Slice mode / action controls ---

    styleLabel (sliceModeLabel_, "Mode");
    addAndMakeVisible (sliceModeLabel_);

    sliceModeComboBox_.addItem ("Transient", 1);
    sliceModeComboBox_.addItem ("Equal",     2);
    sliceModeComboBox_.addItem ("Beat",      3);
    sliceModeComboBox_.addItem ("Bar",       4);
    sliceModeComboBox_.setSelectedId (1, juce::dontSendNotification);
    styleCombo (sliceModeComboBox_);
    sliceModeComboBox_.addListener (this);
    addAndMakeVisible (sliceModeComboBox_);

    styleButton (autoSliceButton_);
    autoSliceButton_.addListener (this);
    addAndMakeVisible (autoSliceButton_);

    styleButton (clearSlicesButton_);
    clearSlicesButton_.setColour (juce::TextButton::buttonColourId,
                                   juce::Colour (0xff2a1a1a));
    clearSlicesButton_.addListener (this);
    addAndMakeVisible (clearSlicesButton_);

    styleLabel (sliceCountLabel_, "Slices: 0", 11.0f, kDim);
    sliceCountLabel_.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (sliceCountLabel_);

    //==========================================================================
    // --- Warp to Grid controls ---

    styleLabel (warpSubdivLabel_, "Subdivide:");
    addAndMakeVisible (warpSubdivLabel_);

    warpSubdivCombo_.addItem ("1/1  (beats)",     1);
    warpSubdivCombo_.addItem ("1/2  (half)",      2);
    warpSubdivCombo_.addItem ("1/4  (quarter)",   4);
    warpSubdivCombo_.addItem ("1/8  (eighth)",    8);
    warpSubdivCombo_.addItem ("1/16 (sixteenth)", 16);
    // Restore persisted value (default 4 = 1/4 note)
    warpSubdivCombo_.setSelectedId (processor_.getWarpSubdivision(), juce::dontSendNotification);
    styleCombo (warpSubdivCombo_);
    warpSubdivCombo_.onChange = [this]
    {
        processor_.setWarpSubdivision (warpSubdivCombo_.getSelectedId());
    };
    addAndMakeVisible (warpSubdivCombo_);

    styleButton (warpToGridBtn_);
    warpToGridBtn_.onClick = [this]
    {
        processor_.warpToGrid (warpSubdivCombo_.getSelectedId());
    };
    addAndMakeVisible (warpToGridBtn_);

    //==========================================================================
    // --- Per-pad editing section ---

    styleLabel (padTitleLabel_, "No pad selected", 12.0f, kText, true);
    addAndMakeVisible (padTitleLabel_);

    // Volume  0.0 – 2.0
    styleLabel (padVolumeLabel_, "Volume");
    addAndMakeVisible (padVolumeLabel_);

    styleSlider (padVolumeSlider_, 0.0, 2.0, 1.0);
    padVolumeSlider_.onValueChange = [this]
    {
        if (selectedPad_ < 0) return;
        auto s = processor_.getPadSettings (selectedPad_);
        s.volume = static_cast<float> (padVolumeSlider_.getValue());
        processor_.setPadSettings (selectedPad_, s);
    };
    addAndMakeVisible (padVolumeSlider_);

    // Pan  -1 – +1
    styleLabel (padPanLabel_, "Pan");
    addAndMakeVisible (padPanLabel_);

    styleSlider (padPanSlider_, -1.0, 1.0, 0.0);
    padPanSlider_.onValueChange = [this]
    {
        if (selectedPad_ < 0) return;
        auto s = processor_.getPadSettings (selectedPad_);
        s.pan = static_cast<float> (padPanSlider_.getValue());
        processor_.setPadSettings (selectedPad_, s);
    };
    addAndMakeVisible (padPanSlider_);

    // Pitch  -12 – +12 semitones
    styleLabel (padPitchLabel_, "Pitch (st)");
    addAndMakeVisible (padPitchLabel_);

    styleSlider (padPitchSlider_, -12.0, 12.0, 0.0, " st");
    padPitchSlider_.setNumDecimalPlacesToDisplay (1);
    padPitchSlider_.onValueChange = [this]
    {
        if (selectedPad_ < 0) return;
        auto s = processor_.getPadSettings (selectedPad_);
        s.pitchOffsetSemitones = static_cast<float> (padPitchSlider_.getValue());
        processor_.setPadSettings (selectedPad_, s);
    };
    addAndMakeVisible (padPitchSlider_);

    // Fine tune  -100 – +100 cents
    styleLabel (padFineTuneLabel_, "Fine (cents)");
    addAndMakeVisible (padFineTuneLabel_);

    styleSlider (padFineTuneSlider_, -100.0, 100.0, 0.0, " c");
    padFineTuneSlider_.setNumDecimalPlacesToDisplay (0);
    padFineTuneSlider_.onValueChange = [this]
    {
        if (selectedPad_ < 0) return;
        auto s = processor_.getPadSettings (selectedPad_);
        s.fineTuneCents = static_cast<float> (padFineTuneSlider_.getValue());
        processor_.setPadSettings (selectedPad_, s);
    };
    addAndMakeVisible (padFineTuneSlider_);

    // Toggle buttons
    styleToggle (padReverseBtn_);
    padReverseBtn_.onClick = [this]
    {
        if (selectedPad_ < 0) return;
        auto s = processor_.getPadSettings (selectedPad_);
        s.reverse = padReverseBtn_.getToggleState();
        processor_.setPadSettings (selectedPad_, s);
    };
    addAndMakeVisible (padReverseBtn_);

    styleToggle (padMuteBtn_);
    padMuteBtn_.onClick = [this]
    {
        if (selectedPad_ < 0) return;
        auto s = processor_.getPadSettings (selectedPad_);
        s.mute = padMuteBtn_.getToggleState();
        processor_.setPadSettings (selectedPad_, s);
    };
    addAndMakeVisible (padMuteBtn_);

    styleToggle (padSoloBtn_);
    padSoloBtn_.onClick = [this]
    {
        if (selectedPad_ < 0) return;
        auto s = processor_.getPadSettings (selectedPad_);
        s.solo = padSoloBtn_.getToggleState();
        processor_.setPadSettings (selectedPad_, s);
    };
    addAndMakeVisible (padSoloBtn_);

    // Choke group  None / Group 1–8
    styleLabel (chokeGroupLabel_, "Choke Group");
    addAndMakeVisible (chokeGroupLabel_);

    chokeGroupCombo_.addItem ("None",    1);
    for (int i = 1; i <= 8; ++i)
        chokeGroupCombo_.addItem ("Group " + juce::String (i), i + 1);
    chokeGroupCombo_.setSelectedId (1, juce::dontSendNotification);
    styleCombo (chokeGroupCombo_);
    chokeGroupCombo_.onChange = [this]
    {
        if (selectedPad_ < 0) return;
        auto s = processor_.getPadSettings (selectedPad_);
        // selectedId 1 == None (chokeGroup=0), selectedId 2 == Group 1 (chokeGroup=1), etc.
        s.chokeGroup = chokeGroupCombo_.getSelectedId() - 1;
        processor_.setPadSettings (selectedPad_, s);
    };
    addAndMakeVisible (chokeGroupCombo_);

    // Pad play mode
    styleLabel (padModeLabel_, "Play Mode");
    addAndMakeVisible (padModeLabel_);

    padModeCombo_.addItem ("One-shot", 1);
    padModeCombo_.addItem ("Gate",     2);
    padModeCombo_.addItem ("Loop",     3);
    padModeCombo_.setSelectedId (1, juce::dontSendNotification);
    styleCombo (padModeCombo_);
    padModeCombo_.onChange = [this]
    {
        if (selectedPad_ < 0) return;
        auto s = processor_.getPadSettings (selectedPad_);
        s.playMode = static_cast<PadPlayMode> (padModeCombo_.getSelectedId() - 1);
        processor_.setPadSettings (selectedPad_, s);
    };
    addAndMakeVisible (padModeCombo_);

    // Hide pad section until a pad is actually selected
    padSelected (-1);

    refreshSliceCount();
}

//==============================================================================
void SliceEditorPanel::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Right border separator
    g.setColour (juce::Colour (0xff0a0a1a));
    g.drawLine (static_cast<float> (getWidth() - 1), 0.0f,
                static_cast<float> (getWidth() - 1), static_cast<float> (getHeight()),
                1.0f);

    const int pad = 10;
    const int w   = getWidth() - pad * 2;

    // Draw subtle separator lines between sections
    const juce::Colour sep = juce::Colour (0xff2a2a4a);

    // Below transient detection block
    int sepY1 = sectionModeLabel_.getY() - 6;
    g.setColour (sep);
    g.fillRect (pad, sepY1, w, 1);

    // Below slicer block (only if pad section is visible)
    if (selectedPad_ >= 0)
    {
        int sepY2 = sectionPadLabel_.getY() - 6;
        g.setColour (sep);
        g.fillRect (pad, sepY2, w, 1);
    }
}

void SliceEditorPanel::resized()
{
    const int pad    = 10;
    const int w      = getWidth() - pad * 2;
    const int halfW  = (w - 4) / 2;   // two-column half-width with 4 px gap
    const int col2x  = pad + halfW + 4;

    const int itemH  = 26;
    const int lblH   = 14;
    const int gap    = 6;
    const int secGap = 14;

    int y = pad;

    //==========================================================================
    // Section 1: Transient Detection  (~120 px)
    sectionTransientLabel_.setBounds (pad, y, w, lblH); y += lblH + 4;

    // Row 1: Sensitivity | Min Length
    sensitivityLabel_.setBounds (pad,   y, halfW, lblH);
    minSliceLenLabel_.setBounds (col2x, y, halfW, lblH);
    y += lblH + 2;

    sensitivitySlider_.setBounds (pad,   y, halfW, itemH);
    minSliceLenSlider_.setBounds (col2x, y, halfW, itemH);
    y += itemH + gap;

    // Row 2: Max Slices | Band Focus
    maxSlicesLabel_.setBounds (pad,   y, halfW, lblH);
    bandFocusLabel_.setBounds (col2x, y, halfW, lblH);
    y += lblH + 2;

    maxSlicesSlider_.setBounds (pad,   y, halfW, itemH);
    bandFocusCombo_.setBounds  (col2x, y, halfW, itemH);
    y += itemH + secGap;

    //==========================================================================
    // Section 2: Slicer controls  (~80 px)
    sectionModeLabel_.setBounds (pad, y, w, 20); y += 24;

    sliceModeLabel_.setBounds    (pad, y, w, lblH); y += lblH + 2;
    sliceModeComboBox_.setBounds (pad, y, w, itemH); y += itemH + gap;

    autoSliceButton_.setBounds   (pad, y, w, itemH); y += itemH + gap;
    clearSlicesButton_.setBounds (pad, y, w, itemH); y += itemH + gap;

    sliceCountLabel_.setBounds   (pad, y, w, 18); y += 18 + gap;

    // Warp to Grid row: label (40%) + combo (60%), then full-width button
    const int warpLblW  = (w * 4) / 10;
    const int warpCmbW  = w - warpLblW - 4;
    const int warpCmbX  = pad + warpLblW + 4;

    warpSubdivLabel_.setBounds (pad,      y, warpLblW, lblH);
    // (no label top row needed – label and combo share the same row)
    warpSubdivCombo_.setBounds (warpCmbX, y, warpCmbW, itemH);
    // vertically centre the label alongside the combo
    warpSubdivLabel_.setBounds (pad, y + (itemH - lblH) / 2, warpLblW, lblH);
    y += itemH + gap;

    warpToGridBtn_.setBounds (pad, y, w, itemH); y += itemH + secGap;

    //==========================================================================
    // Section 3: Per-Pad editing (only visible when selectedPad_ >= 0)
    if (selectedPad_ >= 0)
    {
        sectionPadLabel_.setBounds (pad, y, w, lblH); y += lblH + 4;
        padTitleLabel_.setBounds   (pad, y, w, 20);   y += 24;

        // Volume | Pan
        padVolumeLabel_.setBounds (pad,   y, halfW, lblH);
        padPanLabel_.setBounds    (col2x, y, halfW, lblH);
        y += lblH + 2;

        padVolumeSlider_.setBounds (pad,   y, halfW, itemH);
        padPanSlider_.setBounds    (col2x, y, halfW, itemH);
        y += itemH + gap;

        // Pitch | Fine Tune
        padPitchLabel_.setBounds    (pad,   y, halfW, lblH);
        padFineTuneLabel_.setBounds (col2x, y, halfW, lblH);
        y += lblH + 2;

        padPitchSlider_.setBounds    (pad,   y, halfW, itemH);
        padFineTuneSlider_.setBounds (col2x, y, halfW, itemH);
        y += itemH + gap;

        // Toggle buttons in a row
        const int btnW = (w - gap * 2) / 3;
        padReverseBtn_.setBounds (pad,                  y, btnW, itemH);
        padMuteBtn_.setBounds    (pad + btnW + gap,     y, btnW, itemH);
        padSoloBtn_.setBounds    (pad + (btnW + gap)*2, y, btnW, itemH);
        y += itemH + gap;

        // Choke Group | Play Mode
        chokeGroupLabel_.setBounds (pad,   y, halfW, lblH);
        padModeLabel_.setBounds    (col2x, y, halfW, lblH);
        y += lblH + 2;

        chokeGroupCombo_.setBounds (pad,   y, halfW, itemH);
        padModeCombo_.setBounds    (col2x, y, halfW, itemH);
    }
}

//==============================================================================
void SliceEditorPanel::comboBoxChanged (juce::ComboBox* /*box*/)
{
    // Nothing to push to the processor here; mode is read when Auto Slice fires.
}

void SliceEditorPanel::buttonClicked (juce::Button* button)
{
    if (button == &autoSliceButton_)
    {
        autoSliceButton_.setEnabled (false);
        autoSliceButton_.setButtonText ("Analysing...");

        processor_.onAutoSliceProgress = [this] (bool done)
        {
            if (done)
            {
                autoSliceButton_.setEnabled (true);
                autoSliceButton_.setButtonText ("Auto Slice");
                processor_.onAutoSliceProgress = nullptr;
                refreshSliceCount();
            }
        };

        processor_.autoSliceAsync (currentMode(),
                                   currentSettings_,
                                   static_cast<int> (maxSlicesSlider_.getValue()));
    }
    else if (button == &clearSlicesButton_)
    {
        processor_.getSliceEngine().clearSlices();
        processor_.sendChangeMessage();
        refreshSliceCount();
    }
}

//==============================================================================
void SliceEditorPanel::refreshSliceCount()
{
    const int n = processor_.getSliceEngine().getNumSlices();
    sliceCountLabel_.setText ("Slices: " + juce::String (n),
                               juce::dontSendNotification);

    // Sync warp subdivision combo from processor (handles state restore)
    const int sub = processor_.getWarpSubdivision();
    if (warpSubdivCombo_.getSelectedId() != sub)
        warpSubdivCombo_.setSelectedId (sub, juce::dontSendNotification);
}

//==============================================================================
void SliceEditorPanel::padSelected (int padIndex)
{
    selectedPad_ = padIndex;

    const bool show = (padIndex >= 0);

    sectionPadLabel_.setVisible (show);
    padTitleLabel_.setVisible   (show);
    padVolumeSlider_.setVisible (show);   padVolumeLabel_.setVisible (show);
    padPanSlider_.setVisible    (show);   padPanLabel_.setVisible    (show);
    padPitchSlider_.setVisible  (show);   padPitchLabel_.setVisible  (show);
    padFineTuneSlider_.setVisible (show); padFineTuneLabel_.setVisible (show);
    padReverseBtn_.setVisible   (show);
    padMuteBtn_.setVisible      (show);
    padSoloBtn_.setVisible      (show);
    chokeGroupCombo_.setVisible (show);   chokeGroupLabel_.setVisible (show);
    padModeCombo_.setVisible    (show);   padModeLabel_.setVisible    (show);

    if (show)
    {
        padTitleLabel_.setText ("Pad " + juce::String (padIndex + 1),
                                 juce::dontSendNotification);

        const PadSettings& s = processor_.getPadSettings (padIndex);

        padVolumeSlider_.setValue  (static_cast<double> (s.volume),                juce::dontSendNotification);
        padPanSlider_.setValue     (static_cast<double> (s.pan),                   juce::dontSendNotification);
        padPitchSlider_.setValue   (static_cast<double> (s.pitchOffsetSemitones),  juce::dontSendNotification);
        padFineTuneSlider_.setValue(static_cast<double> (s.fineTuneCents),         juce::dontSendNotification);

        padReverseBtn_.setToggleState (s.reverse, juce::dontSendNotification);
        padMuteBtn_.setToggleState    (s.mute,    juce::dontSendNotification);
        padSoloBtn_.setToggleState    (s.solo,    juce::dontSendNotification);

        // chokeGroup 0 = None (selectedId 1), 1 = Group 1 (selectedId 2), etc.
        const int chokeId = (s.chokeGroup >= 0 && s.chokeGroup <= 8)
                          ? s.chokeGroup + 1
                          : 1;
        chokeGroupCombo_.setSelectedId (chokeId, juce::dontSendNotification);

        // playMode: OneShot=0 -> id 1, Gate=1 -> id 2, Loop=2 -> id 3
        padModeCombo_.setSelectedId (static_cast<int> (s.playMode) + 1,
                                     juce::dontSendNotification);
    }

    resized();
    repaint();
}

//==============================================================================
SliceMode SliceEditorPanel::currentMode() const
{
    switch (sliceModeComboBox_.getSelectedId())
    {
        case 1:  return SliceMode::Transient;
        case 2:  return SliceMode::Equal;
        case 3:  return SliceMode::Beat;
        case 4:  return SliceMode::Bar;
        default: return SliceMode::Transient;
    }
}

TransientDetector::Settings SliceEditorPanel::buildDetectorSettings() const
{
    TransientDetector::Settings s;

    // Sensitivity 0–100: map to threshold 3.0 (insensitive) → 0.5 (very sensitive)
    const double sens = sensitivitySlider_.getValue();          // 0–100
    s.threshold = static_cast<float> (3.0 - (sens / 100.0) * 2.5);  // 3.0–0.5

    // Min slice length: slider is in ms, Settings wants seconds
    s.minSliceSeconds = static_cast<float> (minSliceLenSlider_.getValue() / 1000.0);

    // Band focus → spectralFlux / HFC flags
    // Off   = spectral flux only (default, broadband)
    // Low   = HFC off, spectral flux on  (low-freq emphasis via spectral flux default)
    // Mid   = both on
    // High  = HFC on, spectral flux off  (HFC emphasises high-frequency content)
    const int bandId = bandFocusCombo_.getSelectedId();
    switch (bandId)
    {
        case 1:  // Off  – broadband spectral flux
            s.useSpectralFlux = true;
            s.useHFC          = false;
            break;
        case 2:  // Low  – spectral flux only (naturally more sensitive to low-energy onsets)
            s.useSpectralFlux = true;
            s.useHFC          = false;
            break;
        case 3:  // Mid  – both detectors combined
            s.useSpectralFlux = true;
            s.useHFC          = true;
            break;
        case 4:  // High – HFC only (emphasises high-frequency transients)
            s.useSpectralFlux = false;
            s.useHFC          = true;
            break;
        default:
            s.useSpectralFlux = true;
            s.useHFC          = false;
            break;
    }

    return s;
}
