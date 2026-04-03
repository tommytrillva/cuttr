#include "PluginEditor.h"

//==============================================================================
// Layout constants
static constexpr int kEditorWidth      = 900;
static constexpr int kEditorHeight     = 720;
static constexpr int kRightPanelWidth  = 280;
static constexpr int kLeftWidth        = kEditorWidth - kRightPanelWidth; // 620
static constexpr int kWaveformHeight   = 200;
static constexpr int kTransportHeight  = 60;
static constexpr int kPadGridHeight    = kEditorHeight - kWaveformHeight - kTransportHeight; // 460

// Theme colours
static const juce::Colour kBackground  { 0xff1a1a2e };
static const juce::Colour kAccent      { 0xffe94560 };
static const juce::Colour kPanelBg     { 0xff16213e };

//==============================================================================
ChopprEditor::ChopprEditor (ChopprProcessor& processor)
    : AudioProcessorEditor (processor),
      processor_          (processor),
      waveformDisplay_    (processor),
      padGrid_            (processor, processor.getGridSize()),
      transportBar_       (processor),
      rightPanel_         (juce::TabbedButtonBar::TabsAtTop),
      sliceEditorPanel_   (processor),
      pitchPanel_         (processor),
      timeStretchPanel_   (processor),
      exportPanel_        (processor),
      presetBrowser_      (processor),
      audioSetupPanel_    (deviceManager_)
{
    setSize (kEditorWidth, kEditorHeight);
    setWantsKeyboardFocus (true);

    // ---- Left column ----
    addAndMakeVisible (waveformDisplay_);
    addAndMakeVisible (padGrid_);
    addAndMakeVisible (transportBar_);

    // ---- Right panel tabs ----
    rightPanel_.addTab ("Slice",    kPanelBg, &sliceEditorPanel_,  false);
    rightPanel_.addTab ("Pitch",    kPanelBg, &pitchPanel_,        false);
    rightPanel_.addTab ("Stretch",  kPanelBg, &timeStretchPanel_,  false);
    rightPanel_.addTab ("Export",   kPanelBg, &exportPanel_,       false);
    rightPanel_.addTab ("Presets",  kPanelBg, &presetBrowser_,     false);
    rightPanel_.addTab ("Audio",    kPanelBg, &audioSetupPanel_,   false);
    rightPanel_.setCurrentTabIndex (0);
    addAndMakeVisible (rightPanel_);

    // ---- Listeners ----
    padGrid_.addListener (this);
    processor_.getChangeListeners().add (this);

    // ---- Window properties ----
    setResizable (false, false);
}

ChopprEditor::~ChopprEditor()
{
    processor_.getChangeListeners().remove (this);
    padGrid_.removeListener (this);
}

//==============================================================================
void ChopprEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    // Title in the waveform area top-left
    g.setColour (kAccent);
    g.setFont (juce::Font ("Arial", 18.0f, juce::Font::bold));
    g.drawText ("CHOPPR", 8, 4, 120, 24, juce::Justification::centredLeft, false);
}

void ChopprEditor::resized()
{
    // Right panel: full height on the right side
    rightPanel_.setBounds (kLeftWidth, 0, kRightPanelWidth, kEditorHeight);

    // Waveform: top of left column
    waveformDisplay_.setBounds (0, 0, kLeftWidth, kWaveformHeight);

    // Pad grid: middle section of left column
    padGrid_.setBounds (0, kWaveformHeight, kLeftWidth, kPadGridHeight);

    // Transport: bottom strip of left column
    transportBar_.setBounds (0, kWaveformHeight + kPadGridHeight,
                              kLeftWidth, kTransportHeight);
}

//==============================================================================
void ChopprEditor::changeListenerCallback (juce::ChangeBroadcaster* /*source*/)
{
    // WaveformDisplay registers itself with processor_.getChangeListeners() directly,
    // so it will already receive this notification – we just refresh the other panels.
    transportBar_.refresh();
    sliceEditorPanel_.refreshSliceCount();
    padGrid_.setGridSize (processor_.getGridSize());
    updatePadGridSelection();
    repaint();
}

//==============================================================================
void ChopprEditor::padTriggered (int padIndex, float velocity)
{
    processor_.triggerPad (padIndex, velocity);
}

void ChopprEditor::padSelected (int padIndex)
{
    selectedPad_ = padIndex;
    updatePadGridSelection();

    // Switch to Slice tab so the user can edit per-pad settings
    rightPanel_.setCurrentTabIndex (0);
}

//==============================================================================
void ChopprEditor::updatePadGridSelection()
{
    padGrid_.setSelectedPad (selectedPad_);
}

//==============================================================================
bool ChopprEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (files.size() != 1)
        return false;

    const juce::String ext = juce::File (files[0]).getFileExtension().toLowerCase();
    return ext == ".wav"  || ext == ".mp3"  || ext == ".aif"  ||
           ext == ".aiff" || ext == ".flac" || ext == ".ogg";
}

//==============================================================================
bool ChopprEditor::keyPressed (const juce::KeyPress& key)
{
    const bool ctrl  = key.getModifiers().isCommandDown();
    const bool shift = key.getModifiers().isShiftDown();
    const int  kc    = key.getKeyCode();

    // Space – Play / Stop
    if (! ctrl && kc == juce::KeyPress::spaceKey)
    {
        processor_.setPlaying (! processor_.isPlaying());
        transportBar_.refresh();
        return true;
    }
    // R – Toggle recording
    if (! ctrl && (kc == 'R' || kc == 'r'))
    {
        if (processor_.isRecording())  processor_.stopRecording();
        else                           processor_.startRecording();
        transportBar_.refresh();
        return true;
    }
    // M – Drop slice marker at playhead
    if (! ctrl && (kc == 'M' || kc == 'm'))
    {
        dropSliceAtPlayhead();
        return true;
    }
    // T – Tap Tempo
    if (! ctrl && (kc == 'T' || kc == 't'))
    {
        processor_.tapTempo();
        transportBar_.refresh();
        return true;
    }
    // [ – Set loop-in at playhead
    if (! ctrl && kc == '[')
    {
        processor_.setLoopIn ((int) processor_.getMetronome().getPlayheadPositionSamples());
        return true;
    }
    // ] – Set loop-out at playhead
    if (! ctrl && kc == ']')
    {
        processor_.setLoopOut ((int) processor_.getMetronome().getPlayheadPositionSamples());
        return true;
    }
    // Z – Undo last manual slice marker
    if (! ctrl && (kc == 'Z' || kc == 'z'))
    {
        processor_.getSliceEngine().undoLastSlice();
        processor_.sendChangeMessage();
        return true;
    }
    // Ctrl+E – Open Export panel
    if (ctrl && ! shift && (kc == 'E' || kc == 'e'))
    {
        rightPanel_.setCurrentTabIndex (3);
        return true;
    }
    // Ctrl+S – Save preset (overwrite selected, or prompt if none selected)
    if (ctrl && ! shift && (kc == 'S' || kc == 's'))
    {
        presetBrowser_.saveCurrentOrAs();
        return true;
    }
    // Ctrl+Shift+S – Save preset as new
    if (ctrl && shift && (kc == 'S' || kc == 's'))
    {
        presetBrowser_.saveAsNew();
        return true;
    }

    return false;
}

void ChopprEditor::dropSliceAtPlayhead()
{
    if (! processor_.getSampleBuffer().hasAudio())
        return;

    const int posSamples = static_cast<int> (
        processor_.getMetronome().getPlayheadPositionSamples());

    if (posSamples >= 0)
    {
        processor_.getSliceEngine().addSlice (posSamples);
        processor_.sendChangeMessage();
    }
}

//==============================================================================
void ChopprEditor::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    if (files.size() != 1)
        return;

    juce::File f (files[0]);
    juce::String error;

    if (! processor_.loadSample (f, error))
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            "Could not load sample",
            error.isEmpty() ? "Unknown error." : error);
    }
}
