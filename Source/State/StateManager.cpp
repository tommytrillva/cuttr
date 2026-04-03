#include "StateManager.h"

//==============================================================================
// Local identifier constants to keep string literals in one place
namespace IDs
{
    static const juce::Identifier CHOPPR          ("CHOPPR");
    static const juce::Identifier Pads            ("Pads");
    static const juce::Identifier Pad             ("Pad");
    static const juce::Identifier Slices          ("Slices");
    static const juce::Identifier Slice           ("Slice");

    // Root properties
    static const juce::Identifier bpm             ("bpm");
    static const juce::Identifier sliceMode       ("sliceMode");
    static const juce::Identifier gridSize        ("gridSize");
    static const juce::Identifier stretchMode     ("stretchMode");
    static const juce::Identifier pitchMode       ("pitchMode");
    static const juce::Identifier globalPitchOffset ("globalPitchOffset");
    static const juce::Identifier sampleFilePath  ("sampleFilePath");

    // Per-pad properties
    static const juce::Identifier sliceIndex      ("sliceIndex");
    static const juce::Identifier pitchOffsetSemitones ("pitchOffsetSemitones");
    static const juce::Identifier volume          ("volume");
    static const juce::Identifier pan             ("pan");
    static const juce::Identifier chokeGroup      ("chokeGroup");
    static const juce::Identifier reverse         ("reverse");
    static const juce::Identifier mute            ("mute");
    static const juce::Identifier solo            ("solo");
    static const juce::Identifier padStretchMode  ("padStretchMode");
    static const juce::Identifier stretchRatio    ("stretchRatio");

    // Per-slice properties
    static const juce::Identifier samplePosition  ("samplePosition");
    static const juce::Identifier isLocked        ("isLocked");
} // namespace IDs

//==============================================================================
juce::ValueTree StateManager::saveState (
    const std::vector<PadSettings>& pads,
    float                           bpm,
    SliceMode                       sliceMode,
    GridSize                        gridSize,
    TimeStretchMode                 stretchMode,
    PitchMode                       pitchMode,
    float                           globalPitchOffset,
    const juce::String&             sampleFilePath,
    const std::vector<SlicePoint>&  slices) const
{
    juce::ValueTree root (IDs::CHOPPR);

    // --- Top-level properties ---
    root.setProperty (IDs::bpm,              bpm,                             nullptr);
    root.setProperty (IDs::sliceMode,        static_cast<int> (sliceMode),    nullptr);
    root.setProperty (IDs::gridSize,         static_cast<int> (gridSize),     nullptr);
    root.setProperty (IDs::stretchMode,      static_cast<int> (stretchMode),  nullptr);
    root.setProperty (IDs::pitchMode,        static_cast<int> (pitchMode),    nullptr);
    root.setProperty (IDs::globalPitchOffset, globalPitchOffset,              nullptr);
    root.setProperty (IDs::sampleFilePath,   sampleFilePath,                  nullptr);

    // --- Pads ---
    juce::ValueTree padsVt (IDs::Pads);
    for (const auto& pad : pads)
    {
        juce::ValueTree padVt (IDs::Pad);
        padVt.setProperty (IDs::sliceIndex,           pad.sliceIndex,                         nullptr);
        padVt.setProperty (IDs::pitchOffsetSemitones, pad.pitchOffsetSemitones,               nullptr);
        padVt.setProperty (IDs::volume,               pad.volume,                             nullptr);
        padVt.setProperty (IDs::pan,                  pad.pan,                                nullptr);
        padVt.setProperty (IDs::chokeGroup,           pad.chokeGroup,                         nullptr);
        padVt.setProperty (IDs::reverse,              pad.reverse,                            nullptr);
        padVt.setProperty (IDs::mute,                 pad.mute,                               nullptr);
        padVt.setProperty (IDs::solo,                 pad.solo,                               nullptr);
        padVt.setProperty (IDs::padStretchMode,       static_cast<int> (pad.stretchMode),     nullptr);
        padVt.setProperty (IDs::stretchRatio,         pad.stretchRatio,                       nullptr);
        padsVt.addChild (padVt, -1, nullptr);
    }
    root.addChild (padsVt, -1, nullptr);

    // --- Slices ---
    juce::ValueTree slicesVt (IDs::Slices);
    for (const auto& slice : slices)
    {
        juce::ValueTree sliceVt (IDs::Slice);
        sliceVt.setProperty (IDs::samplePosition, slice.samplePosition, nullptr);
        sliceVt.setProperty (IDs::isLocked,       slice.isLocked,       nullptr);
        slicesVt.addChild (sliceVt, -1, nullptr);
    }
    root.addChild (slicesVt, -1, nullptr);

    return root;
}

//==============================================================================
bool StateManager::loadState (
    const juce::ValueTree&    state,
    std::vector<PadSettings>& pads,
    float&                    bpm,
    SliceMode&                sliceMode,
    GridSize&                 gridSize,
    TimeStretchMode&          stretchMode,
    PitchMode&                pitchMode,
    float&                    globalPitchOffset,
    juce::String&             sampleFilePath,
    std::vector<SlicePoint>&  slices) const
{
    if (! state.isValid() || state.getType() != IDs::CHOPPR)
        return false;

    // --- Top-level properties (with safe defaults) ---
    bpm               = static_cast<float> (state.getProperty (IDs::bpm,              120.0f));
    sliceMode         = static_cast<SliceMode>       ((int) state.getProperty (IDs::sliceMode,  0));
    gridSize          = static_cast<GridSize>         ((int) state.getProperty (IDs::gridSize,  16));
    stretchMode       = static_cast<TimeStretchMode>  ((int) state.getProperty (IDs::stretchMode, 0));
    pitchMode         = static_cast<PitchMode>        ((int) state.getProperty (IDs::pitchMode,  0));
    globalPitchOffset = static_cast<float> (state.getProperty (IDs::globalPitchOffset, 0.0f));
    sampleFilePath    = state.getProperty (IDs::sampleFilePath, juce::String()).toString();

    // --- Pads ---
    pads.clear();
    const auto padsVt = state.getChildWithName (IDs::Pads);
    if (padsVt.isValid())
    {
        for (int i = 0; i < padsVt.getNumChildren() && i < kMaxPads; ++i)
        {
            const auto padVt = padsVt.getChild (i);
            PadSettings pad;
            pad.sliceIndex           = static_cast<int>   (padVt.getProperty (IDs::sliceIndex,           i));
            pad.pitchOffsetSemitones = static_cast<float> (padVt.getProperty (IDs::pitchOffsetSemitones, 0.0f));
            pad.volume               = static_cast<float> (padVt.getProperty (IDs::volume,               1.0f));
            pad.pan                  = static_cast<float> (padVt.getProperty (IDs::pan,                  0.0f));
            pad.chokeGroup           = static_cast<int>   (padVt.getProperty (IDs::chokeGroup,            0));
            pad.reverse              = static_cast<bool>  (padVt.getProperty (IDs::reverse,              false));
            pad.mute                 = static_cast<bool>  (padVt.getProperty (IDs::mute,                 false));
            pad.solo                 = static_cast<bool>  (padVt.getProperty (IDs::solo,                 false));
            pad.stretchMode          = static_cast<TimeStretchMode> ((int) padVt.getProperty (IDs::padStretchMode, 0));
            pad.stretchRatio         = static_cast<float> (padVt.getProperty (IDs::stretchRatio,         1.0f));
            pads.push_back (pad);
        }
    }

    // Ensure we always have kMaxPads entries (fill remaining with defaults)
    while (static_cast<int> (pads.size()) < kMaxPads)
    {
        PadSettings pad;
        pad.sliceIndex = static_cast<int> (pads.size());
        pads.push_back (pad);
    }

    // --- Slices ---
    slices.clear();
    const auto slicesVt = state.getChildWithName (IDs::Slices);
    if (slicesVt.isValid())
    {
        for (int i = 0; i < slicesVt.getNumChildren(); ++i)
        {
            const auto sliceVt = slicesVt.getChild (i);
            SlicePoint slice;
            slice.samplePosition = static_cast<int>  (sliceVt.getProperty (IDs::samplePosition, 0));
            slice.isLocked       = static_cast<bool> (sliceVt.getProperty (IDs::isLocked,        false));
            slices.push_back (slice);
        }
    }

    return true;
}

//==============================================================================
void StateManager::getStateInformation (const juce::ValueTree& state,
                                        juce::MemoryBlock&     destData) const
{
    juce::MemoryOutputStream mos (destData, false);
    state.writeToStream (mos);
}

juce::ValueTree StateManager::setStateInformation (const void* data,
                                                    int         sizeInBytes) const
{
    juce::MemoryInputStream mis (data, static_cast<size_t> (sizeInBytes), false);
    return juce::ValueTree::readFromStream (mis);
}
