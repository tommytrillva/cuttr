#include "AudioSetupPanel.h"

static const juce::Colour kBg     = juce::Colour (0xff1a1a2e);
static const juce::Colour kAccent = juce::Colour (0xffe94560);
static const juce::Colour kText   = juce::Colour (0xffeeeeee);

//==============================================================================
AudioSetupPanel::AudioSetupPanel (juce::AudioDeviceManager& deviceManager,
                                   int numInputChannels,
                                   int numOutputChannels)
    : deviceSelector_ (deviceManager,
                        numInputChannels,   // minAudioInputChannels
                        numInputChannels,   // maxAudioInputChannels
                        numOutputChannels,  // minAudioOutputChannels
                        numOutputChannels,  // maxAudioOutputChannels
                        true,   // showMidiInputOptions
                        false,  // showMidiOutputSelector
                        false,  // showChannelsAsStereoPairs
                        false)  // hideAdvancedOptionsWithButton
{
    // Title label
    titleLabel_.setText ("AUDIO SETUP", juce::dontSendNotification);
    titleLabel_.setFont (juce::Font (14.0f, juce::Font::bold));
    titleLabel_.setColour (juce::Label::textColourId, kText);
    titleLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel_);

    // Apply dark theme colours to the selector component
    deviceSelector_.setColour (juce::ListBox::backgroundColourId,
                                juce::Colour (0xff16213e));
    deviceSelector_.setColour (juce::ListBox::textColourId, kText);

    addAndMakeVisible (deviceSelector_);
}

//==============================================================================
void AudioSetupPanel::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Title bar accent strip
    g.setColour (juce::Colour (0xff16213e));
    g.fillRect (0, 0, getWidth(), 36);

    // Bottom border of title area
    g.setColour (juce::Colour (0xff0a0a1a));
    g.drawLine (0.0f, 36.0f, static_cast<float> (getWidth()), 36.0f, 1.5f);

    // Accent dot before title
    g.setColour (kAccent);
    g.fillRect (10, 14, 4, 14);
}

void AudioSetupPanel::resized()
{
    const int titleH = 36;
    const int pad    = 8;

    titleLabel_.setBounds (20, 0, getWidth() - 20, titleH);

    deviceSelector_.setBounds (pad,
                                titleH + pad,
                                getWidth()  - pad * 2,
                                getHeight() - titleH - pad * 2);
}
