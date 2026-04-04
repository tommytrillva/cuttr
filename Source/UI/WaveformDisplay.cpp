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

static constexpr int   kRulerHeight = 18;
static constexpr float kSnapPixels  = 8.0f;
static constexpr float kLoopHandleSnapPixels = 6.0f;

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
    const auto bounds  = getLocalBounds();
    const int  rulerH  = kRulerHeight;
    const auto wfArea  = bounds.withTrimmedTop (rulerH);

    // Background
    g.fillAll (kBackground);

    // --- Feature 5: beat/bar grid lines (drawn first, behind everything) ---
    drawBeatGrid (g, wfArea);

    // Loop region (below ruler, above waveform so alpha shading shows through)
    drawLoopRegion (g);

    // Waveform
    const auto& sampleBuf = processor_.getSampleBuffer();
    if (sampleBuf.hasAudio())
    {
        if (waveformPathDirty_ || cachedWidth_ != wfArea.getWidth() || cachedHeight_ != wfArea.getHeight())
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
                    wfArea, juce::Justification::centred, false);
    }

    // Time ruler (drawn on top so it covers grid/waveform at the top edge)
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

    const int totalSamples = sampleBuf.getNumSamples();

    // --- Feature 4: check for loop handle drag ---
    if (!event.mods.isRightButtonDown())
    {
        const float loopInX  = normToX (loopStart_);
        const float loopOutX = normToX (loopEnd_);

        if (std::abs (static_cast<float> (event.x) - loopInX) < kLoopHandleSnapPixels)
        {
            loopDragTarget_ = LoopDragTarget::LoopIn;
            return;
        }
        if (std::abs (static_cast<float> (event.x) - loopOutX) < kLoopHandleSnapPixels)
        {
            loopDragTarget_ = LoopDragTarget::LoopOut;
            return;
        }
    }

    // --- Feature 3: right-click context menu on slice markers ---
    if (event.mods.isRightButtonDown())
    {
        const int sliceIdx = findSliceNear (event.x, kSnapPixels);
        if (sliceIdx >= 0)
        {
            juce::PopupMenu menu;
            menu.addItem (1, "Delete Slice");
            menu.addItem (2, "Set as Loop In");
            menu.addItem (3, "Set as Loop Out");
            menu.addItem (4, "Assign to Pad...");

            menu.showMenuAsync (juce::PopupMenu::Options{}, [this, sliceIdx] (int result)
            {
                if (result == 1)
                {
                    processor_.getSliceEngine().removeSlice (sliceIdx);
                    if (selectedSlice_ == sliceIdx)
                        selectedSlice_ = -1;
                    else if (selectedSlice_ > sliceIdx)
                        --selectedSlice_;
                    processor_.sendChangeMessage();
                }
                if (result == 2)
                {
                    processor_.setLoopIn (getSliceSamplePos (sliceIdx));
                }
                if (result == 3)
                {
                    processor_.setLoopOut (getSliceSamplePos (sliceIdx));
                }
                // result == 4: assign to pad – TODO
                repaint();
            });
            return;
        }
        return; // right-click away from a slice – do nothing
    }

    // Left-click from here on
    const double norm       = xToNorm (static_cast<float> (event.x));
    const int    clickSample = juce::roundToInt (norm * static_cast<double> (totalSamples));

    // Check if an existing slice is near the click
    const auto& slices = processor_.getSliceEngine().getSlices();

    for (int i = 0; i < static_cast<int> (slices.size()); ++i)
    {
        const float sliceX = normToX (static_cast<double> (slices[i].samplePosition)
                                      / static_cast<double> (totalSamples));
        if (std::abs (sliceX - static_cast<float> (event.x)) < kSnapPixels)
        {
            // --- Feature 2: begin drag ---
            draggedSliceIndex_ = i;
            dragStartX_        = event.x;
            selectedSlice_     = i;
            repaint();
            return;
        }
    }

    // No existing slice nearby – add a new one
    processor_.getSliceEngine().addSlice (clickSample);
    processor_.sendChangeMessage();
    repaint();
}

//==============================================================================
// Feature 2: drag slice markers
void WaveformDisplay::mouseDrag (const juce::MouseEvent& event)
{
    // --- Feature 4: drag loop handles ---
    if (loopDragTarget_ != LoopDragTarget::None)
    {
        const int totalSamples = processor_.getSampleBuffer().getNumSamples();
        if (totalSamples > 0)
        {
            const double normalised = juce::jlimit (0.0, 1.0,
                static_cast<double> (event.x) / static_cast<double> (getWidth()));
            const int samplePos = static_cast<int> (normalised * totalSamples);

            if (loopDragTarget_ == LoopDragTarget::LoopIn)
            {
                loopStart_ = normalised;
                processor_.setLoopIn (samplePos);
            }
            else
            {
                loopEnd_ = normalised;
                processor_.setLoopOut (samplePos);
            }
            repaint();
        }
        return;
    }

    // --- Feature 2: drag slice marker ---
    if (draggedSliceIndex_ < 0)
        return;

    const int totalSamples = processor_.getSampleBuffer().getNumSamples();
    if (totalSamples <= 0)
        return;

    const int newSamplePos = juce::jlimit (0, totalSamples - 1,
        static_cast<int> (static_cast<double> (event.x)
                          / static_cast<double> (getWidth())
                          * static_cast<double> (totalSamples)));

    processor_.getSliceEngine().moveSlice (draggedSliceIndex_, newSamplePos);
    repaint();
}

//==============================================================================
void WaveformDisplay::mouseUp (const juce::MouseEvent& /*event*/)
{
    if (draggedSliceIndex_ >= 0)
    {
        // Finalise the drag – notify listeners so state can be saved
        processor_.sendChangeMessage();
        draggedSliceIndex_ = -1;
    }

    loopDragTarget_ = LoopDragTarget::None;
}

//==============================================================================
void WaveformDisplay::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto& sampleBuf = processor_.getSampleBuffer();
    if (!sampleBuf.hasAudio())
        return;

    if (event.y < kRulerHeight)
        return;

    const int   totalSamples = sampleBuf.getNumSamples();
    const auto& slices       = processor_.getSliceEngine().getSlices();

    for (int i = 0; i < static_cast<int> (slices.size()); ++i)
    {
        const float sliceX = normToX (static_cast<double> (slices[i].samplePosition)
                                      / static_cast<double> (totalSamples));
        if (std::abs (sliceX - static_cast<float> (event.x)) < kSnapPixels)
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
    // --- Feature 1: animated playhead ---
    // Sync loop points from processor state
    const auto& sampleBuf = processor_.getSampleBuffer();
    const int   total     = sampleBuf.getNumSamples();

    if (total > 0)
    {
        // Feature 4: keep displayed loop range in sync with processor
        const int loopIn  = processor_.getLoopIn();
        const int loopOut = processor_.getLoopOut();
        if (loopIn  >= 0) loopStart_ = static_cast<double> (loopIn)  / total;
        if (loopOut >= 0) loopEnd_   = static_cast<double> (loopOut) / total;

        if (processor_.isPlaying())
        {
            // Use the metronome's running sample counter as the playhead source.
            // getPlayheadPositionSamples() returns absolute samples elapsed since
            // the last reset(), which mirrors the sample playback position when the
            // plugin is driving the metronome from processBlock.
            const double currentSample = processor_.getMetronome().getPlayheadPositionSamples();
            const double newPos = juce::jlimit (0.0, 1.0,
                currentSample / static_cast<double> (total));

            if (newPos != lastPlayheadPos_)
            {
                playheadPos_     = newPos;
                lastPlayheadPos_ = newPos;
                repaint();
                return; // repaint already queued
            }
        }
    }
    else
    {
        // No sample loaded – keep playhead at 0
        if (playheadPos_ != 0.0)
        {
            playheadPos_     = 0.0;
            lastPlayheadPos_ = 0.0;
            repaint();
            return;
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

int WaveformDisplay::findSliceNear (int pixelX, float snapPx) const
{
    const auto& sampleBuf = processor_.getSampleBuffer();
    if (!sampleBuf.hasAudio())
        return -1;

    const int   totalSamples = sampleBuf.getNumSamples();
    const auto& slices       = processor_.getSliceEngine().getSlices();

    for (int i = 0; i < static_cast<int> (slices.size()); ++i)
    {
        const float sliceX = normToX (static_cast<double> (slices[i].samplePosition)
                                      / static_cast<double> (totalSamples));
        if (std::abs (sliceX - static_cast<float> (pixelX)) < snapPx)
            return i;
    }
    return -1;
}

int WaveformDisplay::getSliceSamplePos (int index) const
{
    const auto& slices = processor_.getSliceEngine().getSlices();
    if (index >= 0 && index < static_cast<int> (slices.size()))
        return slices[index].samplePosition;
    return 0;
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

//==============================================================================
// Feature 5: beat/bar grid lines
void WaveformDisplay::drawBeatGrid (juce::Graphics& g, juce::Rectangle<int> waveformArea)
{
    const auto& sampleBuf = processor_.getSampleBuffer();
    if (!sampleBuf.hasAudio())
        return;

    const float  bpm   = processor_.getBPM();
    const double sr    = sampleBuf.getSampleRate();
    const int    total = sampleBuf.getNumSamples();

    if (bpm <= 0.0f || sr <= 0.0 || total <= 0)
        return;

    const double samplesPerBeat = (sr * 60.0) / static_cast<double> (bpm);
    const double samplesPerBar  = samplesPerBeat * 4.0;
    const float  areaTop        = static_cast<float> (waveformArea.getY());
    const float  areaBottom     = static_cast<float> (waveformArea.getBottom());

    // Beat lines (subtle)
    g.setColour (juce::Colour (0xff2a2a2a));
    for (double s = 0.0; s < static_cast<double> (total); s += samplesPerBeat)
    {
        const int x = static_cast<int> (s / static_cast<double> (total) * getWidth());
        g.drawVerticalLine (x, areaTop, areaBottom);
    }

    // Bar lines (slightly brighter, drawn on top of beat lines)
    g.setColour (juce::Colour (0xff3a3a3a));
    for (double s = 0.0; s < static_cast<double> (total); s += samplesPerBar)
    {
        const int x = static_cast<int> (s / static_cast<double> (total) * getWidth());
        g.drawVerticalLine (x, areaTop, areaBottom);
    }
}

//==============================================================================
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

    const float  bpm          = processor_.getBPM();
    const double sampleRate   = sampleBuf.getSampleRate();
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

    const int   totalSamples = sampleBuf.getNumSamples();
    const auto& slices       = processor_.getSliceEngine().getSlices();
    const float top          = static_cast<float> (kRulerHeight);
    const float bottom       = static_cast<float> (getHeight());

    for (int i = 0; i < static_cast<int> (slices.size()); ++i)
    {
        const float x = normToX (static_cast<double> (slices[i].samplePosition)
                                  / static_cast<double> (totalSamples));

        const bool isSelected = (i == selectedSlice_);
        const bool isDragged  = (i == draggedSliceIndex_);
        g.setColour (isDragged ? juce::Colours::yellow
                               : (isSelected ? kSliceSelected : kSliceMarker));
        g.drawLine (x, top, x, bottom, (isSelected || isDragged) ? 2.0f : 1.0f);

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

    const float x1  = normToX (loopStart_);
    const float x2  = normToX (loopEnd_);
    const float top = static_cast<float> (kRulerHeight);
    const float h   = static_cast<float> (getHeight()) - top;

    g.setColour (kLoopRegion);
    g.fillRect (x1, top, x2 - x1, h);

    // Left (loop-in) edge
    g.setColour (juce::Colour (0xff3388cc).withAlpha (0.6f));
    g.drawLine (x1, top, x1, top + h, 1.5f);

    // Right (loop-out) edge
    g.drawLine (x2, top, x2, top + h, 1.5f);
}
