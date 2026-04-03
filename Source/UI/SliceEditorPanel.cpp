#include "SliceEditorPanel.h"
#include "../PluginProcessor.h"

static const juce::Colour kBg      = juce::Colour (0xff1a1a2e);
static const juce::Colour kSurface = juce::Colour (0xff16213e);
static const juce::Colour kAccent  = juce::Colour (0xffe94560);
static const juce::Colour kText    = juce::Colour (0xffeeeeee);
static const juce::Colour kDim     = juce::Colour (0xff888888);

static void styleCombo (juce::ComboBox& cb)
{
    cb.setColour (juce::ComboBox::backgroundColourId,  juce::Colour (0xff1a1a2e));
    cb.setColour (juce::ComboBox::textColourId,         kText);
    cb.setColour (juce::ComboBox::outlineColourId,      juce::Colour (0xff333355));
    cb.setColour (juce::ComboBox::arrowColourId,        kAccent);
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
                        juce::Colour colour = kDim)
{
    lbl.setText (text, juce::dontSendNotification);
    lbl.setFont (juce::Font (juce::FontOptions().withHeight (fontSize)));
    lbl.setColour (juce::Label::textColourId, colour);
    lbl.setJustificationType (juce::Justification::centredLeft);
}

//==============================================================================
SliceEditorPanel::SliceEditorPanel (ChopprProcessor& processor)
    : processor_ (processor)
{
    // Title
    styleLabel (titleLabel_, "SLICER", 13.0f, kText);
    titleLabel_.setFont (juce::Font (juce::FontOptions().withHeight (13.0f).withStyle ("Bold")));
    addAndMakeVisible (titleLabel_);

    // Mode label
    styleLabel (sliceModeLabel_, "Mode");
    addAndMakeVisible (sliceModeLabel_);

    // Mode combo
    sliceModeComboBox_.addItem ("Transient", 1);
    sliceModeComboBox_.addItem ("Equal",     2);
    sliceModeComboBox_.addItem ("Beat",      3);
    sliceModeComboBox_.addItem ("Bar",       4);
    sliceModeComboBox_.setSelectedId (1, juce::dontSendNotification);
    styleCombo (sliceModeComboBox_);
    sliceModeComboBox_.addListener (this);
    addAndMakeVisible (sliceModeComboBox_);

    // Buttons
    styleButton (autoSliceButton_);
    autoSliceButton_.addListener (this);
    addAndMakeVisible (autoSliceButton_);

    styleButton (clearSlicesButton_);
    clearSlicesButton_.setColour (juce::TextButton::buttonColourId,
                                   juce::Colour (0xff2a1a1a));
    clearSlicesButton_.addListener (this);
    addAndMakeVisible (clearSlicesButton_);

    // Slice count
    styleLabel (sliceCountLabel_, "Slices: 0", 11.0f, kDim);
    sliceCountLabel_.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (sliceCountLabel_);

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
}

void SliceEditorPanel::resized()
{
    const int pad  = 10;
    const int w    = getWidth() - pad * 2;
    const int itemH = 26;
    const int gap  = 6;

    int y = pad;

    titleLabel_.setBounds       (pad, y, w, 20); y += 24;

    // Divider space
    y += 4;

    sliceModeLabel_.setBounds   (pad, y, w, 14); y += 16;
    sliceModeComboBox_.setBounds(pad, y, w, itemH); y += itemH + gap;

    autoSliceButton_.setBounds  (pad, y, w, itemH); y += itemH + gap;
    clearSlicesButton_.setBounds(pad, y, w, itemH); y += itemH + gap * 2;

    sliceCountLabel_.setBounds  (pad, y, w, 18);
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
        processor_.autoSlice (currentMode());
        refreshSliceCount();
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
