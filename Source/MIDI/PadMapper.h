#pragma once

#include "../Common.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>

#include <array>
#include <vector>

//==============================================================================
/**
 * Maps MIDI note numbers to pad indices (0-based).
 *
 * Preset mappings cover popular controllers (Komplete Kontrol, Maschine MK3,
 * Push 2).  Custom per-note mappings can be layered on top, and MIDI-learn
 * lets the user assign an incoming note to any pad at runtime.
 */
class PadMapper
{
public:
    //==========================================================================
    enum class ControllerPreset
    {
        KompleteKontrol25,  ///< Notes 48-63  -> pads 0-15
        MachineMK3,         ///< Notes 36-51  -> pads 0-15
        Push2,              ///< Notes 36-99  -> pads 0-31 (64-pad grid, lower 32 used)
        Custom              ///< No preset; only custom mappings apply
    };

    struct MidiLearnEntry
    {
        int padIndex   = -1;
        int midiNote   = -1;
        int midiChannel = 0;  ///< 1-16; 0 = any
    };

    //==========================================================================
    PadMapper();

    //==========================================================================
    /** Apply a built-in controller preset (resets the current note map). */
    void setPreset (ControllerPreset preset);

    /** Add or override a single note->pad mapping in the custom layer. */
    void setCustomMapping (int midiNote, int padIndex);

    /** Remove all custom mappings (preset bindings are not affected). */
    void clearCustomMappings();

    //==========================================================================
    /** Return the 0-based pad index for the given MIDI note and channel.
     *  Returns -1 if the note is not mapped.
     *  @param midiNote     MIDI note number (0-127).
     *  @param midiChannel  MIDI channel (1-16).
     */
    int getPadIndex (int midiNote, int midiChannel) const;

    //==========================================================================
    bool isLearning() const;
    void startMidiLearn (int padIndex);
    void cancelMidiLearn();

    /** Call when an incoming MIDI note should be learnt to the current pad.
     *  Does nothing if MIDI learn is not active.
     */
    void receiveMidiLearn (int midiNote, int midiChannel);

    //==========================================================================
    ControllerPreset                     getCurrentPreset()    const;
    const std::vector<MidiLearnEntry>&   getCustomMappings()   const;

    //==========================================================================
    juce::ValueTree toValueTree()                        const;
    void            fromValueTree (const juce::ValueTree& vt);

private:
    //==========================================================================
    /** Fill noteTopadMap_ according to @p preset.
     *  For Custom, the map is NOT changed (caller is expected to manage it). */
    void setupPreset (ControllerPreset preset);

    //==========================================================================
    /** Direct lookup table: index = MIDI note (0-127), value = pad index or -1. */
    std::array<int, 128>        noteTopadMap_;

    ControllerPreset            preset_          { ControllerPreset::KompleteKontrol25 };
    std::vector<MidiLearnEntry> customMappings_;

    /** Pad index currently being learnt, or -1 when not in learn mode. */
    int                         learningPadIndex_ { -1 };
};
