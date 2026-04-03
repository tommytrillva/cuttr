#include "MidiInputManager.h"

//==============================================================================
void MidiInputManager::processMidiBuffer (const juce::MidiBuffer& midiBuffer,
                                          std::vector<PadEvent>&  outEvents)
{
    for (const auto metadata : midiBuffer)
    {
        const auto msg = metadata.getMessage();

        // Only process note-on and note-off messages
        if (! msg.isNoteOn() && ! msg.isNoteOff())
            continue;

        // Channel filter: midiChannel_ == 0 means accept all
        if (midiChannel_ != 0 && msg.getChannel() != midiChannel_)
            continue;

        const int midiNote   = msg.getNoteNumber();
        const int midiChannel = msg.getChannel();

        // MIDI learn mode: consume the note-on and assign it to the learning pad
        if (padMapper_.isLearning() && msg.isNoteOn() && msg.getVelocity() > 0)
        {
            padMapper_.receiveMidiLearn (midiNote, midiChannel);
            continue;  // Do not generate a pad trigger for learnt notes
        }

        // Normal routing
        const int padIndex = padMapper_.getPadIndex (midiNote, midiChannel);
        if (padIndex < 0)
            continue;

        PadEvent event;
        event.padIndex = padIndex;
        event.isNoteOn = msg.isNoteOn() && msg.getVelocity() > 0;
        event.velocity = event.isNoteOn
                             ? static_cast<float> (msg.getVelocity()) / 127.0f
                             : 0.0f;

        outEvents.push_back (event);
    }
}

//==============================================================================
void MidiInputManager::setMidiChannel (int channel)
{
    jassert (channel >= 0 && channel <= 16);
    midiChannel_ = channel;
}

int MidiInputManager::getMidiChannel() const
{
    return midiChannel_;
}
