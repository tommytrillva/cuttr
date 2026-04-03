#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common.h"

class ChopprProcessor;

//==============================================================================
/**
 * WaveformDisplay
 *
 * Full-width waveform viewer with:
 *   - Waveform rendering (stereo or mono, dark theme)
 *   - Animated playhead (30 Hz timer)
 *   - Vertical slice markers (red lines)
 *   - Loop region shading (semi-transparent blue)
 *   - Time ruler across the top (bar numbers)
 *   - Mouse-click to add/select slice, double-click to remove slice
 */
class WaveformDisplay : public juce::Component,
                        public juce::ChangeListener,
                        public juce::Timer
{
public:
    //==========================================================================
    explicit WaveformDisplay (ChopprProcessor& processor);
    ~WaveformDisplay() override;

    //==========================================================================
    // juce::Component
    void paint (juce::Graphics& g) override;
    void resized() override;

    // Mouse
    void mouseDown        (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

    //==========================================================================
    // juce::ChangeListener
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    //==========================================================================
    // juce::Timer
    void timerCallback() override;

    //==========================================================================
    /** Set the normalised (0–1) loop region boundaries. */
    void setLoopPoints (double start, double end);

    /** Retrieve the currently selected slice index (-1 = none). */
    int  getSelectedSlice() const { return selectedSlice_; }

private:
    //==========================================================================
    /** Convert a normalised (0–1) x position to a pixel x coordinate. */
    float normToX (double norm) const;

    /** Convert a pixel x coordinate to a normalised (0–1) position. */
    double xToNorm (float x) const;

    /** Build a cached waveform path for fast repaints. */
    void buildWaveformPath();

    /** Draw the time ruler at the top of the component. */
    void drawTimeRuler (juce::Graphics& g);

    /** Draw all slice markers. */
    void drawSliceMarkers (juce::Graphics& g);

    /** Draw the animated playhead. */
    void drawPlayhead (juce::Graphics& g);

    /** Draw the loop region overlay. */
    void drawLoopRegion (juce::Graphics& g);

    //==========================================================================
    // Dark theme colours
    static const juce::Colour kBackground;
    static const juce::Colour kWaveformFill;
    static const juce::Colour kWaveformOutline;
    static const juce::Colour kSliceMarker;
    static const juce::Colour kSliceSelected;
    static const juce::Colour kPlayhead;
    static const juce::Colour kLoopRegion;
    static const juce::Colour kRulerBg;
    static const juce::Colour kRulerText;

    //==========================================================================
    ChopprProcessor& processor_;

    double loopStart_    { 0.0 };
    double loopEnd_      { 1.0 };
    double playheadPos_  { 0.0 };
    int    selectedSlice_ { -1 };

    /** Cached waveform path; rebuilt whenever the sample changes. */
    juce::Path waveformPath_;
    bool  waveformPathDirty_ { true };
    int   cachedWidth_  { 0 };
    int   cachedHeight_ { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
