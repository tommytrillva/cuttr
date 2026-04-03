#include "PadGrid.h"
#include "../PluginProcessor.h"

//==============================================================================
// Static colour definitions
const juce::Colour PadGrid::kBackground  = juce::Colour (0xff1a1a2e);
const juce::Colour PadGrid::kPadSurface  = juce::Colour (0xff16213e);
const juce::Colour PadGrid::kPadAssigned = juce::Colour (0xff2a1a2e);
const juce::Colour PadGrid::kPadSelected = juce::Colour (0xffe94560);
const juce::Colour PadGrid::kPadActive   = juce::Colour (0xffe94560).withAlpha (0.55f);
const juce::Colour PadGrid::kPadText     = juce::Colour (0xffeeeeee);
const juce::Colour PadGrid::kPadMuted    = juce::Colour (0xff333355);
const juce::Colour PadGrid::kPadSolo     = juce::Colour (0xffffa020);

static constexpr int kPadGap = 4;

//==============================================================================
PadGrid::PadGrid (ChopprProcessor& processor, GridSize gridSize)
    : processor_ (processor),
      gridSize_  (gridSize)
{
    padActive_.fill (false);
    setOpaque (true);
}

//==============================================================================
void PadGrid::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    const int numPads = static_cast<int> (gridSize_);
    for (int i = 0; i < numPads; ++i)
        drawPad (g, i);
}

void PadGrid::resized()
{
    repaint();
}

//==============================================================================
void PadGrid::mouseDown (const juce::MouseEvent& event)
{
    const int pad = padAtPosition (event.x, event.y);
    if (pad < 0) return;

    pressedPad_ = pad;
    repaint();

    // Velocity: simulate from y position within the pad
    const auto bounds = padBounds (pad);
    const float relY  = static_cast<float> (event.y - bounds.getY())
                        / static_cast<float> (bounds.getHeight());
    const float vel   = 1.0f - juce::jlimit (0.0f, 1.0f, relY);

    listeners_.call ([pad, vel] (Listener& l) { l.padTriggered (pad, vel); });
}

void PadGrid::mouseUp (const juce::MouseEvent& event)
{
    const int pad = padAtPosition (event.x, event.y);
    if (pad >= 0 && pad == pressedPad_)
    {
        selectedPad_ = pad;
        listeners_.call ([pad] (Listener& l) { l.padSelected (pad); });
    }
    pressedPad_ = -1;
    repaint();
}

//==============================================================================
void PadGrid::setGridSize (GridSize newSize)
{
    gridSize_ = newSize;
    repaint();
}

void PadGrid::setSelectedPad (int padIndex)
{
    selectedPad_ = padIndex;
    repaint();
}

void PadGrid::setPadActive (int padIndex, bool active)
{
    if (padIndex < 0 || padIndex >= kMaxPads) return;
    padActive_[static_cast<std::size_t> (padIndex)] = active;
    repaint();
}

//==============================================================================
juce::Rectangle<int> PadGrid::padBounds (int index) const
{
    const int numPads = static_cast<int> (gridSize_);
    const int cols    = 4;
    const int rows    = numPads / cols;

    const int totalW = getWidth();
    const int totalH = getHeight();

    const int padW = (totalW - kPadGap * (cols + 1)) / cols;
    const int padH = (totalH - kPadGap * (rows + 1)) / rows;

    const int col = index % cols;
    const int row = index / cols;

    const int x = kPadGap + col * (padW + kPadGap);
    const int y = kPadGap + row * (padH + kPadGap);

    return { x, y, padW, padH };
}

int PadGrid::padAtPosition (int x, int y) const
{
    const int numPads = static_cast<int> (gridSize_);
    for (int i = 0; i < numPads; ++i)
    {
        if (padBounds (i).contains (x, y))
            return i;
    }
    return -1;
}

void PadGrid::drawPad (juce::Graphics& g, int padIndex)
{
    const auto bounds = padBounds (padIndex).toFloat();
    const float cornerRadius = 5.0f;

    const auto& settings = processor_.getPadSettings (padIndex);
    const bool  hasSlice = (settings.sliceIndex >= 0);
    const bool  isMuted  = settings.mute;
    const bool  isSolo   = settings.solo;
    const bool  isSelected = (padIndex == selectedPad_);
    const bool  isActive   = padActive_[static_cast<std::size_t> (padIndex)];
    const bool  isPressed  = (padIndex == pressedPad_);

    // Base fill colour
    juce::Colour baseColour = hasSlice ? kPadAssigned : kPadSurface;

    if (isMuted)
        baseColour = kPadMuted;
    else if (isSolo)
        baseColour = kPadSolo.withAlpha (0.35f);

    if (isActive || isPressed)
        baseColour = baseColour.brighter (0.4f).withAlpha (0.9f);

    // Shadow / rim
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 1.5f), cornerRadius);

    // Main fill
    g.setColour (baseColour);
    g.fillRoundedRectangle (bounds, cornerRadius);

    // Selected glow
    if (isSelected)
    {
        g.setColour (kPadSelected.withAlpha (0.6f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), cornerRadius, 2.0f);
    }
    else if (hasSlice)
    {
        g.setColour (kPadSelected.withAlpha (0.25f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), cornerRadius, 1.0f);
    }
    else
    {
        g.setColour (juce::Colour (0xff2a2a4a));
        g.drawRoundedRectangle (bounds.reduced (0.5f), cornerRadius, 1.0f);
    }

    // Pad number label
    g.setColour (isSelected ? juce::Colours::white : kPadText.withAlpha (hasSlice ? 0.9f : 0.5f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (juce::jmin (bounds.getHeight() * 0.35f, 14.0f))));
    g.drawText (juce::String (padIndex + 1),
                bounds.toNearestInt(),
                juce::Justification::centred,
                false);

    // "M" / "S" indicator for mute/solo
    if (isMuted || isSolo)
    {
        g.setFont (juce::Font (juce::FontOptions().withHeight (9.0f)));
        g.setColour (isSolo ? kPadSolo : juce::Colour (0xff888888));
        const juce::Rectangle<float> badgeBounds (bounds.getRight() - 14.0f,
                                                   bounds.getY() + 2.0f,
                                                   12.0f, 10.0f);
        g.drawText (isSolo ? "S" : "M", badgeBounds.toNearestInt(),
                    juce::Justification::centred, false);
    }

    // Slice index badge (bottom-right)
    if (hasSlice)
    {
        g.setFont (juce::Font (juce::FontOptions().withHeight (8.0f)));
        g.setColour (kPadSelected.withAlpha (0.75f));
        const juce::Rectangle<float> sliceBadge (bounds.getRight() - 16.0f,
                                                   bounds.getBottom() - 12.0f,
                                                   14.0f, 10.0f);
        g.drawText (juce::String (settings.sliceIndex),
                    sliceBadge.toNearestInt(),
                    juce::Justification::centredRight, false);
    }
}
