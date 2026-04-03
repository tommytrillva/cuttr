#pragma once

#include "../Common.h"
#include <juce_core/juce_core.h>

#include <vector>

//==============================================================================
/**
 * Manages the set of slice points for the loaded sample.
 *
 * Slice points are maintained in ascending order of samplePosition.
 * Auto-detection delegates to TransientDetector for transient-based modes
 * and uses BPM-derived arithmetic for Beat / Bar modes.
 */
class SliceEngine
{
public:
    SliceEngine()  = default;
    ~SliceEngine() = default;

    //==========================================================================
    /** Replace the full slice list. Incoming points are sorted and deduplicated. */
    void setSlices (std::vector<SlicePoint> slices);

    /** Insert a single slice at the given sample position. */
    void addSlice (int samplePosition);

    /** Remove the slice at the given index. Locked slices are skipped. */
    void removeSlice (int index);

    /** Move the slice at @p index to @p newPosition.
     *  Does nothing if the slice is locked. */
    void moveSlice (int index, int newPosition);

    /** Remove all non-locked slices (locked slices are preserved). */
    void clearSlices();

    /** Remove the most recently manually-added slice (Ctrl+Z / Z key).
     *  Does nothing if no manual slices remain. */
    void undoLastSlice();

    //==========================================================================
    /** Automatically detect slices.
     *  @param buffer      Audio to analyse (not modified).
     *  @param sampleRate  Sample rate of @p buffer.
     *  @param mode        Detection algorithm.
     *  @param bpm         Project BPM – used for Beat and Bar modes.
     */
    void autoDetectSlices (const juce::AudioBuffer<float>& buffer,
                           double   sampleRate,
                           SliceMode mode,
                           float    bpm = 120.0f);

    //==========================================================================
    const std::vector<SlicePoint>& getSlices()    const;
    int getNumSlices()                             const;

    /** Returns the sample position of slice @p index, or 0 if out of range. */
    int getSliceStart (int index) const;

    /** Returns the start of the next slice, or @p totalSamples if this is last. */
    int getSliceEnd (int index, int totalSamples) const;

    /** Returns the slice index that contains @p samplePos.
     *  Returns -1 if there are no slices. */
    int findSliceAtPosition (int samplePos) const;

private:
    //==========================================================================
    std::vector<SlicePoint> slices_;

    /** Stack of sample positions added via addSlice() – used for undo. */
    std::vector<int> manualSlicePositions_;

    /** Sort by samplePosition, remove entries within 100 samples of each other,
     *  and ensure the very first slice is at position 0. */
    void sortAndDeduplicate();
};
