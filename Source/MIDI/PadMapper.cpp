#include "PadMapper.h"

#include <algorithm>

//==============================================================================
PadMapper::PadMapper()
{
    noteTopadMap_.fill (-1);
    setupPreset (ControllerPreset::KompleteKontrol25);
}

//==============================================================================
void PadMapper::setPreset (ControllerPreset preset)
{
    preset_ = preset;
    setupPreset (preset);
}

void PadMapper::setupPreset (ControllerPreset preset)
{
    if (preset == ControllerPreset::Custom)
        return;  // Leave map as-is; caller manages custom entries

    noteTopadMap_.fill (-1);

    switch (preset)
    {
        case ControllerPreset::KompleteKontrol25:
        {
            // Komplete Kontrol 25: pads on notes 48-63 (C3-D#4), 16 pads
            for (int i = 0; i < 16; ++i)
                noteTopadMap_[48 + i] = i;
            break;
        }

        case ControllerPreset::MachineMK3:
        {
            // Maschine MK3: pads on notes 36-51 (C2-D#3), 16 pads
            for (int i = 0; i < 16; ++i)
                noteTopadMap_[36 + i] = i;
            break;
        }

        case ControllerPreset::Push2:
        {
            // Push 2 has an 8x8 grid; the bottom 4 rows (notes 36-67) map to
            // pads 0-31 in row-major order (bottom-left to top-right).
            // Rows 36-43 -> pads 0-7
            // Rows 44-51 -> pads 8-15
            // Rows 52-59 -> pads 16-23
            // Rows 60-67 -> pads 24-31
            for (int i = 0; i < 32; ++i)
                noteTopadMap_[36 + i] = i;

            // Upper 4 rows (68-99) also map to the same 32 pads (duplicate
            // physical buttons on the full 8x8 grid if all 64 are used).
            for (int i = 0; i < 32 && (68 + i) < 128; ++i)
                noteTopadMap_[68 + i] = i;
            break;
        }

        case ControllerPreset::Custom:
            // Handled above – not reached
            break;
    }
}

//==============================================================================
void PadMapper::setCustomMapping (int midiNote, int padIndex)
{
    jassert (midiNote >= 0 && midiNote < 128);
    jassert (padIndex >= 0 && padIndex < kMaxPads);

    noteTopadMap_[midiNote] = padIndex;

    // Update or add a custom mapping entry for serialisation
    for (auto& entry : customMappings_)
    {
        if (entry.midiNote == midiNote)
        {
            entry.padIndex = padIndex;
            return;
        }
    }

    MidiLearnEntry entry;
    entry.padIndex   = padIndex;
    entry.midiNote   = midiNote;
    entry.midiChannel = 0;  // any channel
    customMappings_.push_back (entry);
}

void PadMapper::clearCustomMappings()
{
    customMappings_.clear();
    // Re-apply the current preset to restore the base map
    setupPreset (preset_);
}

//==============================================================================
int PadMapper::getPadIndex (int midiNote, int midiChannel) const
{
    juce::ignoreUnused (midiChannel);  // channel filtering is done at MidiInputManager level

    if (midiNote < 0 || midiNote >= 128)
        return -1;

    return noteTopadMap_[midiNote];
}

//==============================================================================
bool PadMapper::isLearning() const
{
    return learningPadIndex_ >= 0;
}

void PadMapper::startMidiLearn (int padIndex)
{
    jassert (padIndex >= 0 && padIndex < kMaxPads);
    learningPadIndex_ = padIndex;
}

void PadMapper::cancelMidiLearn()
{
    learningPadIndex_ = -1;
}

void PadMapper::receiveMidiLearn (int midiNote, int midiChannel)
{
    if (learningPadIndex_ < 0)
        return;

    jassert (midiNote >= 0 && midiNote < 128);

    noteTopadMap_[midiNote] = learningPadIndex_;

    // Record in custom mappings list
    bool found = false;
    for (auto& entry : customMappings_)
    {
        if (entry.midiNote == midiNote)
        {
            entry.padIndex    = learningPadIndex_;
            entry.midiChannel = midiChannel;
            found = true;
            break;
        }
    }

    if (! found)
    {
        MidiLearnEntry entry;
        entry.padIndex    = learningPadIndex_;
        entry.midiNote    = midiNote;
        entry.midiChannel = midiChannel;
        customMappings_.push_back (entry);
    }

    learningPadIndex_ = -1;
}

//==============================================================================
PadMapper::ControllerPreset PadMapper::getCurrentPreset() const
{
    return preset_;
}

const std::vector<PadMapper::MidiLearnEntry>& PadMapper::getCustomMappings() const
{
    return customMappings_;
}

//==============================================================================
juce::ValueTree PadMapper::toValueTree() const
{
    juce::ValueTree vt ("PadMapper");
    vt.setProperty ("preset", static_cast<int> (preset_), nullptr);

    juce::ValueTree mappingsVt ("CustomMappings");
    for (const auto& entry : customMappings_)
    {
        juce::ValueTree entryVt ("Mapping");
        entryVt.setProperty ("padIndex",    entry.padIndex,    nullptr);
        entryVt.setProperty ("midiNote",    entry.midiNote,    nullptr);
        entryVt.setProperty ("midiChannel", entry.midiChannel, nullptr);
        mappingsVt.addChild (entryVt, -1, nullptr);
    }
    vt.addChild (mappingsVt, -1, nullptr);

    return vt;
}

void PadMapper::fromValueTree (const juce::ValueTree& vt)
{
    if (! vt.isValid() || vt.getType() != juce::Identifier ("PadMapper"))
        return;

    const int presetInt = vt.getProperty ("preset", static_cast<int> (ControllerPreset::KompleteKontrol25));
    preset_ = static_cast<ControllerPreset> (presetInt);
    setupPreset (preset_);

    customMappings_.clear();

    const auto mappingsVt = vt.getChildWithName ("CustomMappings");
    if (! mappingsVt.isValid())
        return;

    for (int i = 0; i < mappingsVt.getNumChildren(); ++i)
    {
        const auto childVt = mappingsVt.getChild (i);
        MidiLearnEntry entry;
        entry.padIndex    = childVt.getProperty ("padIndex",    -1);
        entry.midiNote    = childVt.getProperty ("midiNote",    -1);
        entry.midiChannel = childVt.getProperty ("midiChannel",  0);

        if (entry.midiNote >= 0 && entry.midiNote < 128 &&
            entry.padIndex >= 0 && entry.padIndex < kMaxPads)
        {
            noteTopadMap_[entry.midiNote] = entry.padIndex;
            customMappings_.push_back (entry);
        }
    }
}
