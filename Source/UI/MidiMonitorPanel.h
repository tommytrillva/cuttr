#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <deque>

class ChopprProcessor;

struct MidiLogEntry
{
    juce::String timestamp;
    juce::String type;
    int          channel  { 1 };
    int          data1    { 0 };
    int          data2    { 0 };
    juce::String display;   // pre-formatted string
};

class MidiMonitorPanel : public juce::Component,
                         public juce::Timer
{
public:
    explicit MidiMonitorPanel (ChopprProcessor& processor);
    ~MidiMonitorPanel() override = default;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    /** Called from the message thread to log an incoming MIDI message. Thread-safe. */
    void logMessage (const juce::MidiMessage& msg);

    /** Clear the log. */
    void clear();

private:
    ChopprProcessor& processor_;

    static constexpr int kMaxEntries = 200;

    juce::AbstractFifo fifo_  { kMaxEntries };
    MidiLogEntry       ringBuffer_[kMaxEntries];

    // Display list (message thread only)
    std::deque<MidiLogEntry> entries_;

    juce::TextButton   clearBtn_  { "Clear" };
    juce::ToggleButton pauseBtn_  { "Pause" };
    bool paused_ { false };

    // Scroll
    int scrollOffset_ { 0 };
    int rowHeight_    { 18 };

    juce::String formatMessage (const juce::MidiMessage& msg) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMonitorPanel)
};
