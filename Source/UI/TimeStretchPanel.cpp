#include "TimeStretchPanel.h"
#include "../PluginProcessor.h"

static const juce::Colour kBg      = juce::Colour (0xff1a1a2e);
static const juce::Colour kSurface = juce::Colour (0xff16213e);
static const juce::Colour kAccent  = juce::Colour (0xffe94560);
static const juce::Colour kText    = juce::Colour (0xffeeeeee);
static const juce::Colour kDim     = juce::Colour (0xff888888);

//==============================================================================
TimeStretchPanel::TimeStretchPanel (ChopprProcessor& processor)
    : processor_ (processor)
{
    // Title
    titleLabel_.setText ("TIME STRETCH", juce::dontSendNotification);
    titleLabel_.setFont (juce::Font (13.0f, juce::Font::bold));
    titleLabel_.setColour (juce::Label::textColourId, kText);
    titleLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel_);

    // Mode label
    modeLabel_.setText ("Mode", juce::dontSendNotification);
    modeLabel_.setFont (juce::Font (11.0f));
    modeLabel_.setColour (juce::Label::textColourId, kDim);
    modeLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (modeLabel_);

    // Mode combo
    modeComboBox_.addItem ("None",      1);
    modeComboBox_.addItem ("Match BPM", 2);
    modeComboBox_.addItem ("Per-Pad",   3);
    modeComboBox_.addItem ("Free",      4);

    modeComboBox_.setColour (juce::ComboBox::backgroundColourId,   kBg);
    modeComboBox_.setColour (juce::ComboBox::textColourId,          kText);
    modeComboBox_.setColour (juce::ComboBox::outlineColourId,       juce::Colour (0xff333355));
    modeComboBox_.setColour (juce::ComboBox::arrowColourId,         kAccent);
    modeComboBox_.setColour (juce::ComboBox::focusedOutlineColourId, kAccent);

    // Sync combo to current engine mode
    switch (processor_.getTimeStretchEngine().getMode())
    {
        case TimeStretchMode::None:     modeComboBox_.setSelectedId (1, juce::dontSendNotification); break;
        case TimeStretchMode::MatchBPM: modeComboBox_.setSelectedId (2, juce::dontSendNotification); break;
        case TimeStretchMode::PerPad:   modeComboBox_.setSelectedId (3, juce::dontSendNotification); break;
        case TimeStretchMode::Free:     modeComboBox_.setSelectedId (4, juce::dontSendNotification); break;
        default:                        modeComboBox_.setSelectedId (1, juce::dontSendNotification); break;
    }
    modeComboBox_.addListener (this);
    addAndMakeVisible (modeComboBox_);

    // Stretch ratio label
    stretchRatioLabel_.setText ("Stretch Ratio", juce::dontSendNotification);
    stretchRatioLabel_.setFont (juce::Font (11.0f));
    stretchRatioLabel_.setColour (juce::Label::textColourId, kDim);
    stretchRatioLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (stretchRatioLabel_);

    // Stretch ratio slider
    stretchRatioSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    stretchRatioSlider_.setRange (0.5, 2.0, 0.01);
    stretchRatioSlider_.setValue (1.0, juce::dontSendNotification);
    stretchRatioSlider_.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 46, 20);
    stretchRatioSlider_.setColour (juce::Slider::backgroundColourId,       kSurface);
    stretchRatioSlider_.setColour (juce::Slider::trackColourId,             kAccent.withAlpha (0.6f));
    stretchRatioSlider_.setColour (juce::Slider::thumbColourId,             kAccent);
    stretchRatioSlider_.setColour (juce::Slider::textBoxTextColourId,       kText);
    stretchRatioSlider_.setColour (juce::Slider::textBoxBackgroundColourId, kSurface);
    stretchRatioSlider_.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    stretchRatioSlider_.addListener (this);
    addAndMakeVisible (stretchRatioSlider_);

    updateRatioSliderState();
}

//==============================================================================
void TimeStretchPanel::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (juce::Colour (0xff0a0a1a));
    g.drawLine (static_cast<float> (getWidth() - 1), 0.0f,
                static_cast<float> (getWidth() - 1), static_cast<float> (getHeight()),
                1.0f);
}

void TimeStretchPanel::resized()
{
    const int pad   = 10;
    const int w     = getWidth() - pad * 2;
    const int itemH = 26;
    const int gap   = 6;

    int y = pad;

    titleLabel_.setBounds       (pad, y, w, 20); y += 24;
    y += 4;

    modeLabel_.setBounds        (pad, y, w, 14); y += 16;
    modeComboBox_.setBounds     (pad, y, w, itemH); y += itemH + gap;

    stretchRatioLabel_.setBounds(pad, y, w, 14); y += 16;
    stretchRatioSlider_.setBounds(pad, y, w, itemH);
}

//==============================================================================
void TimeStretchPanel::comboBoxChanged (juce::ComboBox* box)
{
    if (box == &modeComboBox_)
    {
        TimeStretchMode mode = TimeStretchMode::None;
        switch (modeComboBox_.getSelectedId())
        {
            case 1: mode = TimeStretchMode::None;     break;
            case 2: mode = TimeStretchMode::MatchBPM; break;
            case 3: mode = TimeStretchMode::PerPad;   break;
            case 4: mode = TimeStretchMode::Free;      break;
            default: break;
        }
        processor_.getTimeStretchEngine().setMode (mode);
        updateRatioSliderState();
    }
}

void TimeStretchPanel::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &stretchRatioSlider_)
    {
        // In Free mode the global ratio drives all pads uniformly via per-pad
        // settings. Apply it to all pads that don't already have an override.
        const float ratio = static_cast<float> (stretchRatioSlider_.getValue());
        const int numPads = static_cast<int> (processor_.getGridSize());
        for (int i = 0; i < numPads; ++i)
        {
            PadSettings settings = processor_.getPadSettings (i);
            settings.stretchRatio = ratio;
            processor_.setPadSettings (i, settings);
        }
    }
}

//==============================================================================
void TimeStretchPanel::refresh()
{
    switch (processor_.getTimeStretchEngine().getMode())
    {
        case TimeStretchMode::None:     modeComboBox_.setSelectedId (1, juce::dontSendNotification); break;
        case TimeStretchMode::MatchBPM: modeComboBox_.setSelectedId (2, juce::dontSendNotification); break;
        case TimeStretchMode::PerPad:   modeComboBox_.setSelectedId (3, juce::dontSendNotification); break;
        case TimeStretchMode::Free:     modeComboBox_.setSelectedId (4, juce::dontSendNotification); break;
        default: break;
    }
    updateRatioSliderState();
}

//==============================================================================
void TimeStretchPanel::updateRatioSliderState()
{
    const bool isFree = (modeComboBox_.getSelectedId() == 4);
    stretchRatioSlider_.setEnabled (isFree);
    stretchRatioLabel_.setColour (juce::Label::textColourId,
                                   isFree ? juce::Colour (0xff888888)
                                          : juce::Colour (0xff555566));
}
