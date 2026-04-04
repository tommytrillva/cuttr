#include "AboutPanel.h"

AboutPanel::AboutPanel()
{
    setSize (340, 220);

    closeBtn_.onClick = [this] {
        if (auto* parent = getParentComponent())
            parent->removeChildComponent (this);
        delete this;
    };
    addAndMakeVisible (closeBtn_);
}

void AboutPanel::paint (juce::Graphics& g)
{
    // Translucent backdrop
    g.fillAll (juce::Colours::black.withAlpha (0.6f));

    // Card
    const auto card = getLocalBounds().withSizeKeepingCentre (300, 180);
    g.setColour (juce::Colour (0xff1e1e1e));
    g.fillRoundedRectangle (card.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff00aaff));
    g.drawRoundedRectangle (card.toFloat(), 8.0f, 1.5f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.drawText ("CHOPPR", card.withHeight (50), juce::Justification::centred);

    g.setFont (juce::Font (13.0f));
    g.setColour (juce::Colours::lightgrey);
    g.drawText ("Version 1.0.0  \u2014  Music Sampler VST3",
                card.translated (0, 44).withHeight (22),
                juce::Justification::centred);

    g.setFont (juce::Font (11.0f));
    g.setColour (juce::Colours::grey);
    g.drawText ("Built with JUCE 7   |   \u00a9 2026 CHOPPR Audio",
                card.translated (0, 68).withHeight (18),
                juce::Justification::centred);

    g.drawText ("Drag & drop audio to load  |  Space=Play  R=Rec  M=Slice",
                card.translated (0, 90).withHeight (18),
                juce::Justification::centred);
}

void AboutPanel::resized()
{
    const auto card = getLocalBounds().withSizeKeepingCentre (300, 180);
    closeBtn_.setBounds (card.getRight() - 74, card.getBottom() - 32, 66, 24);
}

void AboutPanel::show (juce::Component* parent)
{
    if (parent == nullptr) return;
    auto* panel = new AboutPanel();
    panel->setBounds (parent->getLocalBounds());
    parent->addAndMakeVisible (panel);
    panel->toFront (false);
}
