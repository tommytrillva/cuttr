#include "PadFxPanel.h"
#include "../PluginProcessor.h"

PadFxPanel::PadFxPanel (ChopprProcessor& processor)
    : processor_ (processor)
{
    // ── Filter type combo ──────────────────────────────────────────────
    filterTypeCombo_.addItem ("Low Pass",  1);
    filterTypeCombo_.addItem ("High Pass", 2);
    filterTypeCombo_.addItem ("Band Pass", 3);
    filterTypeCombo_.addItem ("Notch",     4);
    filterTypeCombo_.setSelectedId (1, juce::dontSendNotification);

    // ── Setup all sliders ─────────────────────────────────────────────
    filterCutoff_.setRange (20.0, 20000.0, 1.0);
    filterCutoff_.setSkewFactorFromMidPoint (2000.0);
    filterCutoff_.setValue (8000.0, juce::dontSendNotification);

    filterRes_.setRange (0.1, 10.0, 0.01);
    filterRes_.setValue (0.707, juce::dontSendNotification);

    distDrive_.setRange (1.0, 20.0, 0.1);
    distDrive_.setValue (2.0, juce::dontSendNotification);

    distMix_.setRange (0.0, 1.0, 0.01);
    distMix_.setValue (1.0, juce::dontSendNotification);

    revRoom_.setRange  (0.0, 1.0, 0.01);  revRoom_.setValue  (0.5, juce::dontSendNotification);
    revDamp_.setRange  (0.0, 1.0, 0.01);  revDamp_.setValue  (0.5, juce::dontSendNotification);
    revWidth_.setRange (0.0, 1.0, 0.01);  revWidth_.setValue (1.0, juce::dontSendNotification);
    revMix_.setRange   (0.0, 1.0, 0.01);  revMix_.setValue   (0.3, juce::dontSendNotification);

    delTime_.setRange    (1.0, 2000.0, 1.0);  delTime_.setValue    (250.0, juce::dontSendNotification);
    delFeedback_.setRange(0.0, 0.95,  0.01);  delFeedback_.setValue(0.4,   juce::dontSendNotification);
    delMix_.setRange     (0.0, 1.0,   0.01);  delMix_.setValue     (0.3,   juce::dontSendNotification);

    // Apply rotary style to all sliders
    for (auto* s : { &filterCutoff_, &filterRes_,
                     &distDrive_,    &distMix_,
                     &revRoom_,      &revDamp_,     &revWidth_,  &revMix_,
                     &delTime_,      &delFeedback_,  &delMix_ })
    {
        s->setSliderStyle  (juce::Slider::Rotary);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 14);
    }

    // Style headers
    for (auto* lbl : { &filterHeader_, &distHeader_, &reverbHeader_, &delayHeader_ })
    {
        lbl->setFont (juce::Font (11.0f, juce::Font::bold));
        lbl->setColour (juce::Label::textColourId, juce::Colour (0xff00aaff));
        lbl->setJustificationType (juce::Justification::centredLeft);
    }

    // Wire all onChange/onClick → saveToPad
    auto save = [this] { saveToPad(); };

    filterEnable_.onClick          = save;
    filterTypeCombo_.onChange      = save;
    filterCutoff_.onValueChange    = save;
    filterRes_.onValueChange       = save;

    distEnable_.onClick            = save;
    distDrive_.onValueChange       = save;
    distMix_.onValueChange         = save;

    revEnable_.onClick             = save;
    revRoom_.onValueChange         = save;
    revDamp_.onValueChange         = save;
    revWidth_.onValueChange        = save;
    revMix_.onValueChange          = save;

    delEnable_.onClick             = save;
    delTime_.onValueChange         = save;
    delFeedback_.onValueChange     = save;
    delMix_.onValueChange          = save;

    // Add all children
    for (auto* c : { (juce::Component*)&filterHeader_,
                     &filterEnable_, &filterTypeCombo_,
                     &filterCutoff_, &filterCutoffLabel_,
                     &filterRes_,    &filterResLabel_,
                     &distHeader_,
                     &distEnable_,
                     &distDrive_,    &distDriveLabel_,
                     &distMix_,      &distMixLabel_,
                     &reverbHeader_,
                     &revEnable_,
                     &revRoom_,      &revRoomLabel_,
                     &revDamp_,      &revDampLabel_,
                     &revWidth_,     &revWidthLabel_,
                     &revMix_,       &revMixLabel_,
                     &delayHeader_,
                     &delEnable_,
                     &delTime_,      &delTimeLabel_,
                     &delFeedback_,  &delFeedLabel_,
                     &delMix_,       &delMixLabel_ })
        addAndMakeVisible (c);
}

void PadFxPanel::padSelected (int padIndex)
{
    selectedPad_ = padIndex;
    setEnabled (padIndex >= 0);
    loadFromPad();
}

void PadFxPanel::loadFromPad()
{
    if (selectedPad_ < 0) return;
    const auto& fx = processor_.getPadSettings (selectedPad_).fx;

    filterEnable_.setToggleState (fx.filterEnabled, juce::dontSendNotification);
    filterTypeCombo_.setSelectedId (fx.filterType + 1, juce::dontSendNotification);
    filterCutoff_.setValue (fx.filterCutoff,    juce::dontSendNotification);
    filterRes_.setValue    (fx.filterResonance, juce::dontSendNotification);

    distEnable_.setToggleState (fx.distortionEnabled, juce::dontSendNotification);
    distDrive_.setValue (fx.distortionDrive, juce::dontSendNotification);
    distMix_.setValue   (fx.distortionMix,   juce::dontSendNotification);

    revEnable_.setToggleState (fx.reverbEnabled, juce::dontSendNotification);
    revRoom_.setValue  (fx.reverbRoomSize, juce::dontSendNotification);
    revDamp_.setValue  (fx.reverbDamping,  juce::dontSendNotification);
    revWidth_.setValue (fx.reverbWidth,    juce::dontSendNotification);
    revMix_.setValue   (fx.reverbMix,      juce::dontSendNotification);

    delEnable_.setToggleState (fx.delayEnabled, juce::dontSendNotification);
    delTime_.setValue     (fx.delayTimeMs,   juce::dontSendNotification);
    delFeedback_.setValue (fx.delayFeedback, juce::dontSendNotification);
    delMix_.setValue      (fx.delayMix,      juce::dontSendNotification);
}

void PadFxPanel::saveToPad()
{
    if (selectedPad_ < 0) return;
    auto s = processor_.getPadSettings (selectedPad_);

    s.fx.filterEnabled    = filterEnable_.getToggleState();
    s.fx.filterType       = filterTypeCombo_.getSelectedId() - 1;
    s.fx.filterCutoff     = (float) filterCutoff_.getValue();
    s.fx.filterResonance  = (float) filterRes_.getValue();

    s.fx.distortionEnabled = distEnable_.getToggleState();
    s.fx.distortionDrive   = (float) distDrive_.getValue();
    s.fx.distortionMix     = (float) distMix_.getValue();

    s.fx.reverbEnabled  = revEnable_.getToggleState();
    s.fx.reverbRoomSize = (float) revRoom_.getValue();
    s.fx.reverbDamping  = (float) revDamp_.getValue();
    s.fx.reverbWidth    = (float) revWidth_.getValue();
    s.fx.reverbMix      = (float) revMix_.getValue();

    s.fx.delayEnabled  = delEnable_.getToggleState();
    s.fx.delayTimeMs   = (float) delTime_.getValue();
    s.fx.delayFeedback = (float) delFeedback_.getValue();
    s.fx.delayMix      = (float) delMix_.getValue();

    processor_.setPadSettings (selectedPad_, s);
}

void PadFxPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));

    // Draw thin separator lines between sections
    g.setColour (juce::Colour (0xff333333));
    const int w = getWidth();
    for (int y : { 80, 160, 240 })
        g.drawHorizontalLine (y, 4.0f, (float)(w - 4));
}

void PadFxPanel::resized()
{
    const int w   = getWidth();
    const int pad = 6;
    const int kW  = 56;   // knob width
    const int kH  = 62;   // knob height
    const int row = 78;   // section height
    int y = 2;

    // ── Filter ────────────────────────────────────────────────────────
    filterHeader_.setBounds  (pad, y,      80,  16);
    filterEnable_.setBounds  (w - 36, y,  32,  16);
    filterTypeCombo_.setBounds(pad, y+18, w-2*pad, 22);
    filterCutoff_.setBounds  (pad,         y+42, kW, kH);
    filterCutoffLabel_.setBounds(pad,      y+42+kH-2, kW, 12);
    filterRes_.setBounds     (pad + kW+4,  y+42, kW, kH);
    filterResLabel_.setBounds(pad + kW+4,  y+42+kH-2, kW, 12);
    y += row + 2;

    // ── Distortion ────────────────────────────────────────────────────
    distHeader_.setBounds    (pad, y,      80,  16);
    distEnable_.setBounds    (w - 36, y,  32,  16);
    distDrive_.setBounds     (pad,         y+20, kW, kH);
    distDriveLabel_.setBounds(pad,         y+20+kH-2, kW, 12);
    distMix_.setBounds       (pad + kW+4,  y+20, kW, kH);
    distMixLabel_.setBounds  (pad + kW+4,  y+20+kH-2, kW, 12);
    y += row + 2;

    // ── Reverb ────────────────────────────────────────────────────────
    reverbHeader_.setBounds  (pad, y,      80,  16);
    revEnable_.setBounds     (w - 36, y,  32,  16);
    revRoom_.setBounds       (pad,            y+20, kW, kH);
    revRoomLabel_.setBounds  (pad,            y+20+kH-2, kW, 12);
    revDamp_.setBounds       (pad + kW+4,     y+20, kW, kH);
    revDampLabel_.setBounds  (pad + kW+4,     y+20+kH-2, kW, 12);
    revWidth_.setBounds      (pad + (kW+4)*2, y+20, kW, kH);
    revWidthLabel_.setBounds (pad + (kW+4)*2, y+20+kH-2, kW, 12);
    revMix_.setBounds        (pad + (kW+4)*3, y+20, kW, kH);
    revMixLabel_.setBounds   (pad + (kW+4)*3, y+20+kH-2, kW, 12);
    y += row + 2;

    // ── Delay ─────────────────────────────────────────────────────────
    delayHeader_.setBounds   (pad, y,      80,  16);
    delEnable_.setBounds     (w - 36, y,  32,  16);
    delTime_.setBounds       (pad,            y+20, kW, kH);
    delTimeLabel_.setBounds  (pad,            y+20+kH-2, kW, 12);
    delFeedback_.setBounds   (pad + kW+4,     y+20, kW, kH);
    delFeedLabel_.setBounds  (pad + kW+4,     y+20+kH-2, kW, 12);
    delMix_.setBounds        (pad + (kW+4)*2, y+20, kW, kH);
    delMixLabel_.setBounds   (pad + (kW+4)*2, y+20+kH-2, kW, 12);
}
