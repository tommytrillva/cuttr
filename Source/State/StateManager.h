#pragma once

#include "../Common.h"
#include <juce_data_structures/juce_data_structures.h>
#include <juce_core/juce_core.h>

#include <vector>

//==============================================================================
/**
 * Serialises and deserialises all plugin state to/from a JUCE ValueTree.
 *
 * StateManager is stateless itself – it acts purely as a codec.  All mutable
 * state lives in ChopprProcessor and is passed in and out by value/reference.
 *
 * ValueTree schema (root identifier: "CHOPPR"):
 *   Properties: bpm, sliceMode, gridSize, stretchMode, pitchMode,
 *               globalPitchOffset, sampleFilePath
 *   Children:
 *     "Pads"   -> N children "Pad" (one per pad)
 *     "Slices" -> M children "Slice" (one per slice point)
 */
class StateManager
{
public:
    StateManager()  = default;
    ~StateManager() = default;

    //==========================================================================
    /** Snapshot all plugin state into a freshly-created ValueTree.
     *
     *  @param pads              Array of per-pad settings (length must be <= kMaxPads).
     *  @param bpm               Project tempo in BPM.
     *  @param sliceMode         Current slice-detection algorithm.
     *  @param gridSize          Pad-grid layout.
     *  @param stretchMode       Time-stretch algorithm.
     *  @param pitchMode         Pitch-shift algorithm.
     *  @param globalPitchOffset Global pitch offset in semitones.
     *  @param sampleFilePath    Absolute path to the loaded audio file.
     *  @param slices            Current slice-point list.
     *  @return                  A fully-populated "CHOPPR" ValueTree.
     */
    juce::ValueTree saveState (const std::vector<PadSettings>& pads,
                               float                           bpm,
                               SliceMode                       sliceMode,
                               GridSize                        gridSize,
                               TimeStretchMode                 stretchMode,
                               PitchMode                       pitchMode,
                               float                           globalPitchOffset,
                               const juce::String&             sampleFilePath,
                               const std::vector<SlicePoint>&  slices) const;

    //==========================================================================
    /** Restore plugin state from a ValueTree produced by saveState().
     *
     *  Missing properties are replaced with safe defaults.
     *  @return  true if the tree had the expected root identifier ("CHOPPR").
     */
    bool loadState (const juce::ValueTree&    state,
                    std::vector<PadSettings>& pads,
                    float&                    bpm,
                    SliceMode&                sliceMode,
                    GridSize&                 gridSize,
                    TimeStretchMode&          stretchMode,
                    PitchMode&                pitchMode,
                    float&                    globalPitchOffset,
                    juce::String&             sampleFilePath,
                    std::vector<SlicePoint>&  slices) const;

    //==========================================================================
    /** Serialise @p state to raw binary (for AudioProcessor::getStateInformation). */
    void getStateInformation (const juce::ValueTree& state,
                              juce::MemoryBlock&     destData) const;

    /** Deserialise from raw binary (for AudioProcessor::setStateInformation).
     *  @return  The decoded ValueTree, or an invalid tree on parse failure.
     */
    juce::ValueTree setStateInformation (const void* data,
                                         int         sizeInBytes) const;
};
