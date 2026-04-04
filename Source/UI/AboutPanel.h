#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class AboutPanel : public juce::Component
{
public:
    AboutPanel();
    ~AboutPanel() override = default;

    void paint  (juce::Graphics&) override;
    void resized() override;

    /** Shows the About panel as a non-modal overlay on the given parent. */
    static void show (juce::Component* parent);

private:
    juce::TextButton closeBtn_ { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutPanel)
};
