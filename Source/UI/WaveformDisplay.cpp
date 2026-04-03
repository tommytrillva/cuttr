#include "WaveformDisplay.h"
#include "../PluginProcessor.h"

//==============================================================================
// Static colour definitions
const juce::Colour WaveformDisplay::kBackground     = juce::Colour (0xff1a1a2e);
const juce::Colour WaveformDisplay::kWaveformFill   = juce::Colour (0xff16213e);
const juce::Colour WaveformDisplay::kWaveformOutline = juce::Colour (0xffe94560).withAlpha (0.85f);
const juce::Colour WaveformDisplay::kSliceMarker    = juce::Colour (0xffe94560);
const juce::Colour WaveformDisplay::kSliceSelected  = juce::Colours::white;
const juce::Colour WaveformDisplay::kPlayhead       = juce::Colours::white;
const juce::Colour WaveformDisplay::kLoopRegion     = juce::Colour (0xff3388cc).withAlpha (0.22f);
const juce::Colour WaveformDisplay::kRulerBg        = juce::Colour (0xff0f0f1e);
const juce::Colour WaveformDisplay::kRulerText      = juce::Colour (0xff888888);

static constexpr int kRulerHeight = 18;

//==============================================================================
WaveformDisplay::WaveformDisplay (ChopprProcessor& processor)
    : processor_ (processor)
{
    processor_.getChangeListeners().add (this);
    startTimerHz (30);
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
    processor_.getChangeListeners().remove (this);
}

//==============================================================================
void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();

    // Background
    g.fillAll (kBackground);

    const int rulerH   = kRulerHeight;
    const auto wfBounds = bounds.withTrimmedTop (rulerH);

    // Loop region (below ruler)
    drawLoopRegion (g);

    // Waveform
    const auto& sampleBuf = processor_.getSampleBuffer();
    if (sampleBuf.hasAudio())
    {
        if (waveformPathDirty_ || cachedWidth_ != wfBounds.getWidth() || cachedHeight_ != wfBounds.getHeight())
            buildWaveformPath();

        g.setColour (kWaveformFill);
        g.fillPath (waveformPath_);

        g.setColour (kWaveformOutline);
        g.strokePath (waveformPath_, juce::PathStrokeType (1.2f));
    }
    else
    {
        // Empty state label
        g.setColour (juce::Colour (0xff444466));
        g.setFont (juce::Font (14.0f));
        g.drawText ("Drop a sample or use File > Load",
                    wfBounds, juce::Justification::centred, false);
    }

    // Time ruler
    drawTimeRuler (g);

    // Slice markers
    drawSliceMarkers (g);

    // Playhead
    drawPlayhead (g);
}

void WaveformDisplay::resized()
{
    waveformPathDirty_ = true;
}

//==============================================================================
void WaveformDisplay::mouseDown (const juce::MouseEvent& event)
{
    const auto& sampleBuf = processor_.getSampleBuffer();
    if (!sampleBuf.hasAudio())
        return;

    // Ignore clicks inside the ruler
    if (event.y < kRulerHeight)
        return;

    const double norm = xToNorm (static_cast<float> (event.x));
    const int totalSamples = sampleBuf.getNumSamples();
    const int clickSample  = juce::roundToInt (norm * static_cast<double> (totalSamples));

    // Check if an existing slice is near the click
    const auto& slices = processor_.getSliceEngine().getSlices();
    const float snapPixels = 8.0f;

    for (int i = 0; i < static_cast<int> (slices.size()); ++i)
    {
        const float sliceX = normToX (static_cast<double> (slices[i].samplePosition)
                                      / static_cast<double> (totalSamples));
        if (std::abs (sliceX - static_cast<float> (event.x)) < snapPixels)
        {
            selectedSlice_ = i;
            repaint();
            return;
        }
    }

    // No existing slice nearby – add a new one
    processor_.getSliceEngine().addSlice (clickSample);
    processor_.sendChangeMessage();
    repaint();
}

void WaveformDisplay::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto& sampleBuf = processor_.getSampleBuffer();
    if (!sampleBuf.hasAudio())
        return;

    if (event.y < kRulerHeight)
        return;

    const auto& slices      = processor_.getSliceEngine().getSlices();
    const int   totalSamples = sampleBuf.getNumSamples();
    const float snapPixels   = 8.0f;

    for (int i = 0; i < static_cast<int> (slices.size()); ++i)
    {
        const float sliceX = normToX (static_cast<double> (slices[i].samplePosition)
                                      / static_cast<double> (totalSamples));
        if (std::abs (sliceX - static_cast<float> (event.x)) < snapPixels)
        {
            processor_.getSliceEngine().removeSlice (i);
            if (selectedSlice_ == i)
                selectedSlice_ = -1;
            else if (selectedSlice_ > i)
                --selectedSlice_;
            processor_.sendChangeMessage();
            repaint();
            return;
        }
    }
}

//==============================================================================
void WaveformDisplay::changeListenerCallback (juce::ChangeBroadcaster*)
{
    waveformPathDirty_ = true;
    repaint();
}

//==============================================================================
void WaveformDisplay::timerCallback()
{
    // Update playhead from processor if playing
    if (processor_.isPlaying())
    {
        const auto& sampleBuf = processor_.getSampleBuffer();
        if (sampleBuf.hasAudio())
        {
            // Read the current playback position from the metronome / sample position.
            // We expose what we can from the public API: use processor_.getMetronome()
            // to infer beat position and map it to a normalised waveform position.
            // As a fallback we keep the position static when we can't determine it.
            // (A fuller implementation would expose a getCurrentSamplePosition() method.)
        }
    }

    repaint();
}

//==============================================================================
void WaveformDisplay::setLoopPoints (double start, double end)
{
    loopStart_ = juce::jlimit (0.0, 1.0, start);
    loopEnd_   = juce::jlimit (0.0, 1.0, end);
    repaint();
}

//==============================================================================
// Private helpers
//==============================================================================
float WaveformDisplay::normToX (double norm) const
{
    return static_cast<float> (norm * static_cast<double> (getWidth()));
}

double WaveformDisplay::xToNorm (float x) const
{
    const int w = getWidth();
    if (w <= 0) return 0.0;
    return juce::jlimit (0.0, 1.0, static_cast<double> (x) / static_cast<double> (w));
}

void WaveformDisplay::buildWaveformPath()
{
    waveformPath_.clear();

    const auto& sampleBuf = processor_.getSampleBuffer();
    if (!sampleBuf.hasAudio())
        return;

    const int rulerH = kRulerHeight;
    const int w      = getWidth();
    const int h      = getHeight() - rulerH;
    if (w <= 0 || h <= 0)
        return;

    const auto& buffer       = sampleBuf.getBuffer();
    const int   numSamples   = buffer.getNumSamples();
    const int   numChannels  = buffer.getNumChannels();
    const float midY         = static_cast<float> (rulerH + h / 2);
    const float halfH        = static_cast<float> (h / 2) - 2.0f;

    // Downsample: one pixel column = a block of samples
    const double samplesPerPixel = static_cast<double> (numSamples) / static_cast<double> (w);

    waveformPath_.startNewSubPath (0.0f, midY);

    // Top edge (positive peaks)
    for (int px = 0; px < w; ++px)
    {
        const int startSample = static_cast<int> (px * samplesPerPixel);
        const int endSample   = juce::jmin (static_cast<int> ((px + 1) * samplesPerPixel),
                                            numSamples);

        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);
            for (int s = startSample; s < endSample; ++s)
                peak = juce::jmax (peak, std::abs (data[s]));
        }

        const float y = midY - peak * halfH;
        if (px == 0)
            waveformPath_.startNewSubPath (0.0f, y);
        else
            waveformPath_.lineTo (static_cast<float> (px), y);
    }

    // Bottom edge (negative peaks, reversed)
    for (int px = w - 1; px >= 0; --px)
    {
        const int startSample = static_cast<int> (px * samplesPerPixel);
        const int endSample   = juce::jmin (static_cast<int> ((px + 1) * samplesPerPixel),
                                            numSamples);

        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);
            for (int s = startSample; s < endSample; ++s)
                peak = juce::jmax (peak, std::abs (data[s]));
        }

        const float y = midY + peak * halfH;
        waveformPath_.lineTo (static_cast<float> (px), y);
    }

    waveformPath_.closeSubPath();

    waveformPathDirty_ = false;
    cachedWidth_  = w;
    cachedHeight_ = h;
}

void WaveformDisplay::drawTimeRuler (juce::Graphics& g)
{
    const auto rulerBounds = getLocalBounds().removeFromTop (kRulerHeight);

    g.setColour (kRulerBg);
    g.fillRect (rulerBounds);

    g.setColour (juce::Colour (0xff333355));
    g.drawLine (0.0f,
                static_cast<float> (kRulerHeight),
                static_cast<float> (getWidth()),
                static_cast<float> (kRulerHeight),
                1.0f);

    const auto& sampleBuf = processor_.getSampleBuffer();
    if (!sampleBuf.hasAudio())
        return;

    const float bpm          = processor_.getBPM();
    const double sampleRate  = sampleBuf.getSampleRate();
    const int    totalSamples = sampleBuf.getNumSamples();
    const double durationSec  = static_cast<double> (totalSamples) / sampleRate;
    const double beatsPerBar  = 4.0;
    const double secPerBeat   = 60.0 / static_cast<double> (bpm);
    const double secPerBar    = secPerBeat * beatsPerBar;

    if (secPerBar <= 0.0 || durationSec <= 0.0)
        return;

    g.setFont (juce::Font (10.0f));
    g.setColour (kRulerText);

    const int numBars = static_cast<int> (durationSec / secPerBar) + 2;
    for (int bar = 0; bar < numBars; ++bar)
    {
        const double normPos = (static_cast<double> (bar) * secPerBar) / durationSec;
        if (normPos > 1.0) break;

        const float x = normToX (normPos);

        // Tick
        g.setColour (juce::Colour (0xff555577));
        g.drawLine (x, 0.0f, x, static_cast<float> (kRulerHeight), 1.0f);

        // Bar number label
        g.setColour (kRulerText);
        g.drawText (juce::String (bar + 1),
                    static_cast<int> (x) + 2, 2,
                    32, kRulerHeight - 4,
                    juce::Justification::centredLeft, false);
    }
}

void WaveformDisplay::drawSliceMarkers (juce::Graphics& g)
{
    const auto& sampleBuf = processor_.getSampleBuffer();
    if (!sampleBuf.hasAudio())
        return;

    const int    totalSamples = sampleBuf.getNumSamples();
    const auto&  slices       = processor_.getSliceEngine().getSlices();
    const float  top          = static_cast<float> (kRulerHeight);
    const float  bottom       = static_cast<float> (getHeight());

    for (int i = 0; i < static_cast<int> (slices.size()); ++i)
    {
        const float x = normToX (static_cast<double> (slices[i].samplePosition)
                                  / static_cast<double> (totalSamples));

        const bool isSelected = (i == selectedSlice_);
        g.setColour (isSelected ? kSliceSelected : kSliceMarker);
        g.drawLine (x, top, x, bottom, isSelected ? 2.0f : 1.0f);

        // Small triangle handle at the top
        juce::Path handle;
        handle.addTriangle (x - 4.0f, top,
                            x + 4.0f, top,
                            x,         top + 7.0f);
        g.fillPath (handle);
    }
}

void WaveformDisplay::drawPlayhead (juce::Graphics& g)
{
    if (playheadPos_ < 0.0 || playheadPos_ > 1.0)
        return;

    const float x      = normToX (playheadPos_);
    const float top    = static_cast<float> (kRulerHeight);
    const float bottom = static_cast<float> (getHeight());

    g.setColour (kPlayhead.withAlpha (0.9f));
    g.drawLine (x, top, x, bottom, 1.5f);

    // Small downward triangle
    juce::Path head;
    head.addTriangle (x - 5.0f, top,
                      x + 5.0f, top,
                      x,         top + 8.0f);
    g.setColour (kPlayhead);
    g.fillPath (head);
}

void WaveformDisplay::drawLoopRegion (juce::Graphics& g)
{
    if (loopStart_ >= loopEnd_)
        return;

    const float x1    = normToX (loopStart_);
    const float x2    = normToX (loopEnd_);
    const float top   = static_cast<float> (kRulerHeight);
    const float h     = static_cast<float> (getHeight()) - top;

    g.setColour (kLoopRegion);
    g.fillRect (x1, top, x2 - x1, h);

    // Left and right edge lines
    g.setColour (juce::Colour (0xff3388cc).withAlpha (0.6f));
    g.drawLine (x1, top, x1, top + h, 1.5f);
    g.drawLine (x2, top, x2, top + h, 1.5f);
}
