#pragma once

#include "../Common.h"
#include "PadMapper.h"
#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

//==============================================================================
/**
 * Processes the raw MidiBuffer provided by the host in processBlock and
 * translates MIDI note messages into PadEvents that the voice pool can consume.
 *
 * Channel filtering and MIDI-learn routing are both handled here.  The class
 * owns a PadMapper instance that is accessible for preset/learn configuration.
 */
class MidiInputManager
{
public:
    //==========================================================================
    struct PadEvent
    {
        int   padIndex  = -1;    ///< 0-based pad index
        float velocity  = 0.0f; ///< Normalised velocity [0..1]
        bool  isNoteOn  = true;
    };

    //==========================================================================
    MidiInputManager() = default;

    //==========================================================================
    /** Parse @p midiBuffer and append translated PadEvents to @p outEvents.
     *
     *  - Note-off messages (or note-on with velocity 0) produce a PadEvent
     *    with isNoteOn = false.
     *  - If MIDI learn is active, the first note-on message is consumed by the
     *    learn process (no PadEvent is produced for it).
     *  - Messages on channels other than the filter channel are ignored when
     *    midiChannel_ != 0.
     */
    void processMidiBuffer (const juce::MidiBuffer& midiBuffer,
                            std::vector<PadEvent>&  outEvents);

    //==========================================================================
    PadMapper&       getPadMapper()       { return padMapper_; }
    const PadMapper& getPadMapper() const { return padMapper_; }

    /** Set the MIDI channel to respond to.
     *  @param channel  1-16 for a specific channel, 0 for all channels. */
    void setMidiChannel (int channel);

    /** Currently active channel filter (0 = all channels). */
    int getMidiChannel() const;

private:
    //==========================================================================
    PadMapper padMapper_;
    int       midiChannel_ { 0 };  ///< 0 = respond to all channels
};
