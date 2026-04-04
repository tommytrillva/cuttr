#include "MidiMonitorPanel.h"
#include "../PluginProcessor.h"

MidiMonitorPanel::MidiMonitorPanel (ChopprProcessor& processor)
    : processor_ (processor)
{
    clearBtn_.onClick = [this] { entries_.clear(); scrollOffset_ = 0; repaint(); };
    addAndMakeVisible (clearBtn_);

    pauseBtn_.setClickingTogglesState (true);
    pauseBtn_.onStateChange = [this] { paused_ = pauseBtn_.getToggleState(); };
    addAndMakeVisible (pauseBtn_);

    startTimerHz (30);  // refresh at 30fps
}

void MidiMonitorPanel::logMessage (const juce::MidiMessage& msg)
{
    if (paused_) return;

    MidiLogEntry entry;
    entry.display = formatMessage (msg);

    // Write to ring buffer from any thread
    int start1, size1, start2, size2;
    fifo_.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0) ringBuffer_[start1] = entry;
    fifo_.finishedWrite (size1 + size2);
}

void MidiMonitorPanel::timerCallback()
{
    // Drain ring buffer on message thread
    int start1, size1, start2, size2;
    fifo_.prepareToRead (fifo_.getNumReady(), start1, size1, start2, size2);

    for (int i = start1; i < start1 + size1; ++i)
        entries_.push_back (ringBuffer_[i]);
    for (int i = start2; i < start2 + size2; ++i)
        entries_.push_back (ringBuffer_[i]);

    fifo_.finishedRead (size1 + size2);

    while ((int)entries_.size() > kMaxEntries)
        entries_.pop_front();

    if (size1 + size2 > 0)
        repaint();
}

void MidiMonitorPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));

    // Header
    g.setColour (juce::Colour (0xff2a2a2a));
    g.fillRect (0, 0, getWidth(), 24);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (13.0f, juce::Font::bold));
    g.drawText ("MIDI Monitor", 8, 0, 200, 24, juce::Justification::centredLeft);

    // Border
    g.setColour (juce::Colour (0xff444444));
    g.drawRect (getLocalBounds());

    const int listTop    = 28;
    const int listBottom = getHeight() - 4;
    const int listHeight = listBottom - listTop;
    const int maxVisible = listHeight / rowHeight_;

    // Draw rows from bottom (newest at bottom)
    const int numEntries = (int)entries_.size();
    const int startIdx   = juce::jmax (0, numEntries - maxVisible - scrollOffset_);
    const int endIdx     = juce::jmin (numEntries, startIdx + maxVisible);

    g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));

    for (int i = startIdx; i < endIdx; ++i)
    {
        const int row = i - startIdx;
        const int y   = listTop + row * rowHeight_;

        // Alternate row background
        if (row % 2 == 0)
        {
            g.setColour (juce::Colour (0xff222222));
            g.fillRect (0, y, getWidth(), rowHeight_);
        }

        g.setColour (juce::Colours::lightgreen);
        g.drawText (entries_[i].display, 4, y, getWidth() - 8, rowHeight_,
                    juce::Justification::centredLeft, true);
    }

    // Scroll hint
    if (scrollOffset_ > 0)
    {
        g.setColour (juce::Colours::yellow.withAlpha (0.6f));
        g.drawText (juce::String ("^ ") + juce::String (scrollOffset_) + " more",
                    getWidth() - 80, listBottom - 16, 76, 14,
                    juce::Justification::centredRight);
    }
}

void MidiMonitorPanel::resized()
{
    clearBtn_.setBounds  (getWidth() - 100, 1, 46, 22);
    pauseBtn_.setBounds  (getWidth() -  52, 1, 48, 22);
}

void MidiMonitorPanel::clear()
{
    entries_.clear();
    scrollOffset_ = 0;
    repaint();
}

juce::String MidiMonitorPanel::formatMessage (const juce::MidiMessage& msg) const
{
    juce::String s;
    const int ch = msg.getChannel();

    if      (msg.isNoteOn())
        s = juce::String::formatted ("Ch%d  Note On   n=%3d  v=%3d", ch, msg.getNoteNumber(), msg.getVelocity());
    else if (msg.isNoteOff())
        s = juce::String::formatted ("Ch%d  Note Off  n=%3d  v=%3d", ch, msg.getNoteNumber(), msg.getVelocity());
    else if (msg.isController())
        s = juce::String::formatted ("Ch%d  CC  %3d = %3d", ch, msg.getControllerNumber(), msg.getControllerValue());
    else if (msg.isPitchWheel())
        s = juce::String::formatted ("Ch%d  PitchBend %5d", ch, msg.getPitchWheelValue());
    else if (msg.isProgramChange())
        s = juce::String::formatted ("Ch%d  ProgChange %3d", ch, msg.getProgramChangeNumber());
    else if (msg.isAftertouch())
        s = juce::String::formatted ("Ch%d  Aftertouch n=%3d  v=%3d", ch, msg.getNoteNumber(), msg.getAfterTouchValue());
    else if (msg.isChannelPressure())
        s = juce::String::formatted ("Ch%d  ChanPressure %3d", ch, msg.getChannelPressureValue());
    else
        s = juce::String::formatted ("Ch%d  [0x%02x]", ch, (int)msg.getRawData()[0]);

    return s;
}
