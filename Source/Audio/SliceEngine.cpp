#include "SliceEngine.h"
#include "../Util/TransientDetector.h"

#include <algorithm>
#include <cmath>

//==============================================================================
void SliceEngine::setSlices (std::vector<SlicePoint> slices)
{
    slices_ = std::move (slices);
    manualSlicePositions_.clear();
    sortAndDeduplicate();
}

void SliceEngine::addSlice (int samplePosition)
{
    SlicePoint sp;
    sp.samplePosition = samplePosition;
    slices_.push_back (sp);
    sortAndDeduplicate();
    manualSlicePositions_.push_back (samplePosition); // track for undo
}

void SliceEngine::removeSlice (int index)
{
    if (index < 0 || index >= static_cast<int> (slices_.size()))
        return;

    if (slices_[static_cast<size_t> (index)].isLocked)
        return;

    slices_.erase (slices_.begin() + index);
    // No need to re-sort; removal preserves order
}

void SliceEngine::moveSlice (int index, int newPosition)
{
    if (index < 0 || index >= static_cast<int> (slices_.size()))
        return;

    if (slices_[static_cast<size_t> (index)].isLocked)
        return;

    slices_[static_cast<size_t> (index)].samplePosition = newPosition;
    sortAndDeduplicate();
}

void SliceEngine::clearSlices()
{
    // Preserve locked slices
    slices_.erase (
        std::remove_if (slices_.begin(), slices_.end(),
                        [] (const SlicePoint& sp) { return ! sp.isLocked; }),
        slices_.end());

    manualSlicePositions_.clear();
    sortAndDeduplicate();
}

void SliceEngine::undoLastSlice()
{
    while (! manualSlicePositions_.empty())
    {
        const int target = manualSlicePositions_.back();
        manualSlicePositions_.pop_back();

        // Find and remove the unlocked slice nearest to that position
        for (int i = 0; i < static_cast<int> (slices_.size()); ++i)
        {
            if (slices_[static_cast<size_t> (i)].samplePosition == target
                && ! slices_[static_cast<size_t> (i)].isLocked)
            {
                slices_.erase (slices_.begin() + i);
                return;
            }
        }
        // Slice may have been moved by deduplication – keep popping
    }
}

//==============================================================================
void SliceEngine::autoDetectSlices (const juce::AudioBuffer<float>& buffer,
                                    double   sampleRate,
                                    SliceMode mode,
                                    float    bpm)
{
    // Preserve user-locked slices across auto-detection
    std::vector<SlicePoint> lockedSlices;
    for (auto& sp : slices_)
        if (sp.isLocked)
            lockedSlices.push_back (sp);

    slices_ = lockedSlices;

    const int totalSamples = buffer.getNumSamples();
    if (totalSamples <= 0)
        return;

    switch (mode)
    {
        case SliceMode::Transient:
        {
            TransientDetector detector;
            TransientDetector::Settings settings;
            // Use defaults; first position (0) is always included by the detector
            auto positions = detector.detectTransients (buffer, sampleRate, settings);
            for (int pos : positions)
            {
                SlicePoint sp;
                sp.samplePosition = pos;
                sp.isLocked       = false;
                slices_.push_back (sp);
            }
            break;
        }

        case SliceMode::Equal:
        {
            // Divide into 16 equal slices
            const int numDivisions = 16;
            for (int i = 0; i < numDivisions; ++i)
            {
                SlicePoint sp;
                sp.samplePosition = static_cast<int> (
                    static_cast<double> (i) / static_cast<double> (numDivisions)
                    * static_cast<double> (totalSamples));
                sp.isLocked = false;
                slices_.push_back (sp);
            }
            break;
        }

        case SliceMode::Beat:
        {
            // One slice per beat
            if (bpm <= 0.0f) bpm = 120.0f;
            const double samplesPerBeat = sampleRate * 60.0 / static_cast<double> (bpm);
            int pos = 0;
            while (pos < totalSamples)
            {
                SlicePoint sp;
                sp.samplePosition = pos;
                sp.isLocked       = false;
                slices_.push_back (sp);
                pos += static_cast<int> (std::round (samplesPerBeat));
            }
            break;
        }

        case SliceMode::Bar:
        {
            // One slice per bar (4 beats)
            if (bpm <= 0.0f) bpm = 120.0f;
            const double samplesPerBar = sampleRate * 60.0 / static_cast<double> (bpm) * 4.0;
            int pos = 0;
            while (pos < totalSamples)
            {
                SlicePoint sp;
                sp.samplePosition = pos;
                sp.isLocked       = false;
                slices_.push_back (sp);
                pos += static_cast<int> (std::round (samplesPerBar));
            }
            break;
        }

        default:
            break;
    }

    sortAndDeduplicate();
}

//==============================================================================
void SliceEngine::warpToGrid (double samplesPerBeat, int subdivisions)
{
    if (samplesPerBeat <= 0.0 || slices_.empty()) return;

    const double gridSize = samplesPerBeat / static_cast<double> (subdivisions);

    for (auto& slice : slices_)
    {
        if (slice.isLocked) continue;  // don't move locked slices

        // Snap to nearest grid line
        const double snapped = std::round (slice.samplePosition / gridSize) * gridSize;
        slice.samplePosition = juce::roundToInt (snapped);
    }

    // Remove duplicates that may have snapped to the same position
    std::sort (slices_.begin(), slices_.end(),
               [] (const SlicePoint& a, const SlicePoint& b) {
                   return a.samplePosition < b.samplePosition;
               });
    slices_.erase (std::unique (slices_.begin(), slices_.end(),
                                [] (const SlicePoint& a, const SlicePoint& b) {
                                    return a.samplePosition == b.samplePosition;
                                }),
                   slices_.end());
}

//==============================================================================
const std::vector<SlicePoint>& SliceEngine::getSlices() const
{
    return slices_;
}

int SliceEngine::getNumSlices() const
{
    return static_cast<int> (slices_.size());
}

int SliceEngine::getSliceStart (int index) const
{
    if (index < 0 || index >= static_cast<int> (slices_.size()))
        return 0;
    return slices_[static_cast<size_t> (index)].samplePosition;
}

int SliceEngine::getSliceEnd (int index, int totalSamples) const
{
    if (index < 0 || index >= static_cast<int> (slices_.size()))
        return totalSamples;

    const int nextIndex = index + 1;
    if (nextIndex >= static_cast<int> (slices_.size()))
        return totalSamples;

    return slices_[static_cast<size_t> (nextIndex)].samplePosition;
}

int SliceEngine::findSliceAtPosition (int samplePos) const
{
    if (slices_.empty())
        return -1;

    // Find the last slice whose samplePosition <= samplePos
    int result = 0;
    for (int i = 0; i < static_cast<int> (slices_.size()); ++i)
    {
        if (slices_[static_cast<size_t> (i)].samplePosition <= samplePos)
            result = i;
        else
            break;
    }
    return result;
}

//==============================================================================
void SliceEngine::sortAndDeduplicate()
{
    // Sort ascending
    std::sort (slices_.begin(), slices_.end(),
               [] (const SlicePoint& a, const SlicePoint& b)
               { return a.samplePosition < b.samplePosition; });

    // Ensure first slice is at position 0 (add if absent)
    if (slices_.empty() || slices_.front().samplePosition != 0)
    {
        SlicePoint start;
        start.samplePosition = 0;
        start.isLocked       = false;
        slices_.insert (slices_.begin(), start);
    }

    // Remove duplicates: keep the earlier entry if two are within 100 samples
    // (prefer locked slices when deciding which to keep)
    std::vector<SlicePoint> deduped;
    deduped.reserve (slices_.size());

    for (auto& sp : slices_)
    {
        if (deduped.empty())
        {
            deduped.push_back (sp);
            continue;
        }

        const int gap = sp.samplePosition - deduped.back().samplePosition;
        if (gap < 100)
        {
            // Keep whichever is locked; if neither (or both), keep existing
            if (sp.isLocked && ! deduped.back().isLocked)
                deduped.back() = sp;
            // else: discard the new one
        }
        else
        {
            deduped.push_back (sp);
        }
    }

    slices_ = std::move (deduped);

    // Clamp to kMaxSlices
    if (static_cast<int> (slices_.size()) > kMaxSlices)
        slices_.resize (static_cast<size_t> (kMaxSlices));
}
