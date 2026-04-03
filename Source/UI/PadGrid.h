#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common.h"

class ChopprProcessor;

//==============================================================================
/**
 * PadGrid
 *
 * Renders a 4x4 or 4x8 grid of velocity-sensitive pad buttons.
 * Each pad displays its number, highlights when active/playing, and
 * reflects whether it has an assigned slice via color tinting.
 *
 * Observers register as Listener to receive pad trigger / selection events.
 */
class PadGrid : public juce::Component
{
public:
    //==========================================================================
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void padTriggered (int padIndex, float velocity) = 0;
        virtual void padSelected  (int padIndex)                 = 0;
    };

    //==========================================================================
    PadGrid (ChopprProcessor& processor, GridSize gridSize);
    ~PadGrid() override = default;

    //==========================================================================
    // juce::Component
    void paint   (juce::Graphics& g) override;
    void resized () override;

    void mouseDown  (const juce::MouseEvent& event) override;
    void mouseUp    (const juce::MouseEvent& event) override;

    //==========================================================================
    /** Switch between 4x4 and 4x8 layouts; calls resized(). */
    void setGridSize (GridSize newSize);

    /** Highlight the pad at @p padIndex as the current editor selection. */
    void setSelectedPad (int padIndex);

    /** Flash a pad to show that it is currently playing. */
    void setPadActive (int padIndex, bool active);

    //==========================================================================
    void addListener    (Listener* l) { listeners_.add (l); }
    void removeListener (Listener* l) { listeners_.remove (l); }

private:
    //==========================================================================
    /** Return the bounding rect of pad @p index, based on current layout. */
    juce::Rectangle<int> padBounds (int index) const;

    /** Return the pad index at pixel position (x, y), or -1 if none. */
    int  padAtPosition (int x, int y) const;

    /** Draw a single pad. */
    void drawPad (juce::Graphics& g, int padIndex);

    //==========================================================================
    // Theme colours
    static const juce::Colour kBackground;
    static const juce::Colour kPadSurface;
    static const juce::Colour kPadAssigned;
    static const juce::Colour kPadSelected;
    static const juce::Colour kPadActive;
    static const juce::Colour kPadText;
    static const juce::Colour kPadMuted;
    static const juce::Colour kPadSolo;

    //==========================================================================
    ChopprProcessor& processor_;

    GridSize gridSize_;
    int      selectedPad_  { -1 };

    /** Tracks which pads are currently "lit" (playing). */
    std::array<bool, kMaxPads> padActive_ {};

    /** Tracks which pad is being pressed for visual feedback. */
    int pressedPad_ { -1 };

    juce::ListenerList<Listener> listeners_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadGrid)
};
