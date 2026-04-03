#include "PitchPanel.h"
#include "../PluginProcessor.h"

static const juce::Colour kBg      = juce::Colour (0xff1a1a2e);
static const juce::Colour kSurface = juce::Colour (0xff16213e);
static const juce::Colour kAccent  = juce::Colour (0xffe94560);
static const juce::Colour kText    = juce::Colour (0xffeeeeee);
static const juce::Colour kDim     = juce::Colour (0xff888888);

//==============================================================================
PitchPanel::PitchPanel (ChopprProcessor& processor)
    : processor_ (processor)
{
    // Title
    titleLabel_.setText ("PITCH", juce::dontSendNotification);
    titleLabel_.setFont (juce::Font (juce::FontOptions().withHeight (13.0f).withStyle ("Bold")));
    titleLabel_.setColour (juce::Label::textColourId, kText);
    titleLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel_);

    // Global pitch label
    globalPitchLabel_.setText ("Global Pitch", juce::dontSendNotification);
    globalPitchLabel_.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    globalPitchLabel_.setColour (juce::Label::textColourId, kDim);
    globalPitchLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (globalPitchLabel_);

    // Global pitch slider
    globalPitchSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    globalPitchSlider_.setRange (-24.0, 24.0, 0.5);
    globalPitchSlider_.setValue (static_cast<double> (processor_.getGlobalPitchOffset()),
                                  juce::dontSendNotification);
    globalPitchSlider_.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 46, 20);
    globalPitchSlider_.setTextValueSuffix (" st");
    globalPitchSlider_.setColour (juce::Slider::backgroundColourId,       kSurface);
    globalPitchSlider_.setColour (juce::Slider::trackColourId,             kAccent.withAlpha (0.6f));
    globalPitchSlider_.setColour (juce::Slider::thumbColourId,             kAccent);
    globalPitchSlider_.setColour (juce::Slider::textBoxTextColourId,       kText);
    globalPitchSlider_.setColour (juce::Slider::textBoxBackgroundColourId, kSurface);
    globalPitchSlider_.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    globalPitchSlider_.addListener (this);
    addAndMakeVisible (globalPitchSlider_);

    // Mode label
    pitchModeLabel_.setText ("Mode", juce::dontSendNotification);
    pitchModeLabel_.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    pitchModeLabel_.setColour (juce::Label::textColourId, kDim);
    pitchModeLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (pitchModeLabel_);

    // Mode combo
    pitchModeComboBox_.addItem ("Chromatic",  1);
    pitchModeComboBox_.addItem ("Tonal",      2);
    pitchModeComboBox_.addItem ("Percussive", 3);
    pitchModeComboBox_.setColour (juce::ComboBox::backgroundColourId,     kBg);
    pitchModeComboBox_.setColour (juce::ComboBox::textColourId,             kText);
    pitchModeComboBox_.setColour (juce::ComboBox::outlineColourId,          juce::Colour (0xff333355));
    pitchModeComboBox_.setColour (juce::ComboBox::arrowColourId,            kAccent);
    pitchModeComboBox_.setColour (juce::ComboBox::focusedOutlineColourId,   kAccent);

    // Sync combo to current engine mode
    switch (processor_.getPitchShiftEngine().getMode())
    {
        case PitchMode::Chromatic:   pitchModeComboBox_.setSelectedId (1, juce::dontSendNotification); break;
        case PitchMode::Tonal:       pitchModeComboBox_.setSelectedId (2, juce::dontSendNotification); break;
        case PitchMode::Percussive:  pitchModeComboBox_.setSelectedId (3, juce::dontSendNotification); break;
        default:                     pitchModeComboBox_.setSelectedId (1, juce::dontSendNotification); break;
    }
    pitchModeComboBox_.addListener (this);
    addAndMakeVisible (pitchModeComboBox_);

    // Reset button
    resetButton_.setColour (juce::TextButton::buttonColourId,  kSurface);
    resetButton_.setColour (juce::TextButton::buttonOnColourId, kAccent);
    resetButton_.setColour (juce::TextButton::textColourOffId,  kText);
    resetButton_.setColour (juce::TextButton::textColourOnId,   kText);
    resetButton_.addListener (this);
    addAndMakeVisible (resetButton_);
}

//==============================================================================
void PitchPanel::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (juce::Colour (0xff0a0a1a));
    g.drawLine (static_cast<float> (getWidth() - 1), 0.0f,
                static_cast<float> (getWidth() - 1), static_cast<float> (getHeight()),
                1.0f);
}

void PitchPanel::resized()
{
    const int pad   = 10;
    const int w     = getWidth() - pad * 2;
    const int itemH = 26;
    const int gap   = 6;

    int y = pad;

    titleLabel_.setBounds       (pad, y, w, 20); y += 24;
    y += 4;

    globalPitchLabel_.setBounds (pad, y, w, 14); y += 16;
    globalPitchSlider_.setBounds(pad, y, w, itemH); y += itemH + gap;

    pitchModeLabel_.setBounds   (pad, y, w, 14); y += 16;
    pitchModeComboBox_.setBounds(pad, y, w, itemH); y += itemH + gap * 2;

    resetButton_.setBounds      (pad, y, w, itemH);
}

//==============================================================================
void PitchPanel::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &globalPitchSlider_)
        processor_.setGlobalPitchOffset (static_cast<float> (globalPitchSlider_.getValue()));
}

void PitchPanel::comboBoxChanged (juce::ComboBox* box)
{
    if (box == &pitchModeComboBox_)
    {
        PitchMode mode = PitchMode::Chromatic;
        switch (pitchModeComboBox_.getSelectedId())
        {
            case 1: mode = PitchMode::Chromatic;  break;
            case 2: mode = PitchMode::Tonal;      break;
            case 3: mode = PitchMode::Percussive; break;
            default: break;
        }
        processor_.getPitchShiftEngine().setMode (mode);
    }
}

void PitchPanel::buttonClicked (juce::Button* button)
{
    if (button == &resetButton_)
    {
        processor_.setGlobalPitchOffset (0.0f);
        globalPitchSlider_.setValue (0.0, juce::dontSendNotification);
        processor_.getPitchShiftEngine().setMode (PitchMode::Chromatic);
        pitchModeComboBox_.setSelectedId (1, juce::dontSendNotification);
    }
}

//==============================================================================
void PitchPanel::refresh()
{
    globalPitchSlider_.setValue (static_cast<double> (processor_.getGlobalPitchOffset()),
                                  juce::dontSendNotification);
    switch (processor_.getPitchShiftEngine().getMode())
    {
        case PitchMode::Chromatic:   pitchModeComboBox_.setSelectedId (1, juce::dontSendNotification); break;
        case PitchMode::Tonal:       pitchModeComboBox_.setSelectedId (2, juce::dontSendNotification); break;
        case PitchMode::Percussive:  pitchModeComboBox_.setSelectedId (3, juce::dontSendNotification); break;
        default: break;
    }
}
