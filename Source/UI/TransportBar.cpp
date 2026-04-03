#include "TransportBar.h"
#include "../PluginProcessor.h"

static const juce::Colour kBg       = juce::Colour (0xff16213e);
static const juce::Colour kAccent   = juce::Colour (0xffe94560);
static const juce::Colour kDimText  = juce::Colour (0xff888888);
static const juce::Colour kText     = juce::Colour (0xffeeeeee);
static const juce::Colour kSurface  = juce::Colour (0xff1a1a2e);
static const juce::Colour kRecOn    = juce::Colour (0xffcc2222);

//==============================================================================
TransportBar::TransportBar (ChopprProcessor& processor)
    : processor_ (processor)
{
    // ---- Play ----
    applyButtonStyle (playButton_, kSurface, kAccent.darker (0.2f), kText);
    playButton_.addListener (this);
    addAndMakeVisible (playButton_);

    // ---- Stop ----
    applyButtonStyle (stopButton_, kSurface, juce::Colour (0xff2a2a4a), kText);
    stopButton_.addListener (this);
    addAndMakeVisible (stopButton_);

    // ---- Rec ----
    applyButtonStyle (recordButton_, kSurface, kRecOn, kText);
    recordButton_.setClickingTogglesState (true);
    recordButton_.addListener (this);
    addAndMakeVisible (recordButton_);

    // ---- BPM Label ----
    bpmLabel_.setText ("BPM", juce::dontSendNotification);
    bpmLabel_.setFont (juce::Font (11.0f));
    bpmLabel_.setColour (juce::Label::textColourId, kDimText);
    bpmLabel_.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bpmLabel_);

    // ---- BPM Slider ----
    bpmSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider_.setRange (50.0, 250.0, 0.5);
    bpmSlider_.setValue (static_cast<double> (processor_.getBPM()),
                          juce::dontSendNotification);
    bpmSlider_.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 20);
    bpmSlider_.setColour (juce::Slider::backgroundColourId,  kSurface);
    bpmSlider_.setColour (juce::Slider::trackColourId,        kAccent.withAlpha (0.6f));
    bpmSlider_.setColour (juce::Slider::thumbColourId,        kAccent);
    bpmSlider_.setColour (juce::Slider::textBoxTextColourId,  kText);
    bpmSlider_.setColour (juce::Slider::textBoxBackgroundColourId, kSurface);
    bpmSlider_.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    bpmSlider_.addListener (this);
    addAndMakeVisible (bpmSlider_);

    // ---- Tap Tempo ----
    applyButtonStyle (tapTempoButton_, kSurface, juce::Colour (0xff2a2a50), kText);
    tapTempoButton_.addListener (this);
    addAndMakeVisible (tapTempoButton_);

    // ---- Time Sig Label ----
    timeSigLabel_.setText ("4/4", juce::dontSendNotification);
    timeSigLabel_.setFont (juce::Font (14.0f));
    timeSigLabel_.setColour (juce::Label::textColourId, kDimText);
    timeSigLabel_.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (timeSigLabel_);

    // ---- Grid Size ----
    applyButtonStyle (gridSizeButton_, kSurface, juce::Colour (0xff2a1a3a), kText);
    gridSizeButton_.addListener (this);
    addAndMakeVisible (gridSizeButton_);

    updateGridSizeLabel();
}

//==============================================================================
void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Bottom separator line
    g.setColour (juce::Colour (0xff0a0a1a));
    g.drawLine (0.0f,
                static_cast<float> (getHeight() - 1),
                static_cast<float> (getWidth()),
                static_cast<float> (getHeight() - 1),
                1.5f);
}

void TransportBar::resized()
{
    // Manual FlexBox-style horizontal layout
    const int h      = getHeight();
    const int pad    = 6;
    const int btnH   = h - pad * 2;
    const int btnW   = 44;
    const int sliderW = 130;
    const int tapW   = 38;
    const int tsW    = 36;
    const int gsW    = 48;

    int x = pad;

    playButton_.setBounds   (x, pad, btnW, btnH); x += btnW + 4;
    stopButton_.setBounds   (x, pad, btnW, btnH); x += btnW + 4;
    recordButton_.setBounds (x, pad, btnW, btnH); x += btnW + 12;

    bpmLabel_.setBounds (x, pad, 32, btnH); x += 34;
    bpmSlider_.setBounds (x, pad, sliderW, btnH); x += sliderW + 4;
    tapTempoButton_.setBounds (x, pad, tapW, btnH); x += tapW + 12;

    timeSigLabel_.setBounds (x, pad, tsW, btnH); x += tsW + 8;
    gridSizeButton_.setBounds (x, pad, gsW, btnH);
}

//==============================================================================
void TransportBar::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &bpmSlider_)
        processor_.setBPM (static_cast<float> (bpmSlider_.getValue()));
}

void TransportBar::buttonClicked (juce::Button* button)
{
    if (button == &playButton_)
    {
        processor_.setPlaying (true);
    }
    else if (button == &stopButton_)
    {
        processor_.setPlaying (false);
    }
    else if (button == &recordButton_)
    {
        if (recordButton_.getToggleState())
            processor_.startRecording();
        else
            processor_.stopRecording();
    }
    else if (button == &tapTempoButton_)
    {
        processor_.tapTempo();
        // Refresh BPM slider after tap
        bpmSlider_.setValue (static_cast<double> (processor_.getBPM()),
                              juce::dontSendNotification);
    }
    else if (button == &gridSizeButton_)
    {
        isGridSize4x8_ = !isGridSize4x8_;
        processor_.setGridSize (isGridSize4x8_ ? GridSize::Grid4x8 : GridSize::Grid4x4);
        updateGridSizeLabel();
    }
}

//==============================================================================
void TransportBar::refresh()
{
    bpmSlider_.setValue (static_cast<double> (processor_.getBPM()),
                          juce::dontSendNotification);
    isGridSize4x8_ = (processor_.getGridSize() == GridSize::Grid4x8);
    updateGridSizeLabel();
}

//==============================================================================
void TransportBar::applyButtonStyle (juce::TextButton& btn,
                                     juce::Colour normalBg,
                                     juce::Colour overBg,
                                     juce::Colour textColour)
{
    btn.setColour (juce::TextButton::buttonColourId,   normalBg);
    btn.setColour (juce::TextButton::buttonOnColourId,  overBg);
    btn.setColour (juce::TextButton::textColourOffId,   textColour);
    btn.setColour (juce::TextButton::textColourOnId,    textColour);
}

void TransportBar::updateGridSizeLabel()
{
    gridSizeButton_.setButtonText (isGridSize4x8_ ? "4x8" : "4x4");
}
