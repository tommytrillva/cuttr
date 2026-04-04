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
      audioSetupPanel_    (deviceManager_),
      midiMonitor_        (processor),
      masterMeter_        (processor)
{
    setSize (kEditorWidth, kEditorHeight);
    setWantsKeyboardFocus (true);

    // Initialise the audio device manager in standalone mode so that the Audio
    // Setup panel can enumerate and configure output devices.
    if (processor_.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        deviceManager_.initialiseWithDefaultDevices (0, 2); // 0 inputs, 2 outputs
    }

    // ---- Load Sample button ----
    loadSampleBtn_.onClick = [this]
    {
        fileChooser_ = std::make_unique<juce::FileChooser> (
            "Load Sample",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory),
            "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");

        fileChooser_->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto result = fc.getResult();
                if (result.existsAsFile())
                {
                    juce::String error;
                    if (! processor_.loadSample (result, error))
                    {
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::AlertWindow::WarningIcon,
                            "Could not load sample",
                            error.isEmpty() ? "Unknown error." : error);
                    }
                }
            });
    };
    addAndMakeVisible (loadSampleBtn_);

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
    rightPanel_.addTab ("MIDI",     kPanelBg, &midiMonitor_,       false);
    rightPanel_.setCurrentTabIndex (0);
    addAndMakeVisible (rightPanel_);

    // ---- Wire MIDI monitor callback ----
    processor_.onMidiReceived = [this] (const juce::MidiMessage& m)
    {
        midiMonitor_.logMessage (m);
    };

    // ---- BPM status label ----
    bpmStatusLabel_.setText ("", juce::dontSendNotification);
    bpmStatusLabel_.setFont (juce::Font (11.0f));
    bpmStatusLabel_.setColour (juce::Label::textColourId, juce::Colours::yellow);
    addAndMakeVisible (bpmStatusLabel_);

    // ---- Wire BPM detection callback ----
    processor_.onBpmDetected = [this] (float bpm, bool valid)
    {
        if (valid)
            bpmStatusLabel_.setText (juce::String (bpm, 1) + " BPM detected",
                                     juce::dontSendNotification);
        else
            bpmStatusLabel_.setText ("BPM: unknown", juce::dontSendNotification);
    };

    // ---- Overdub toggle button ----
    overdubBtn_.setClickingTogglesState (true);
    overdubBtn_.onClick = [this]
    {
        processor_.setOverdubMode (overdubBtn_.getToggleState());
    };
    addAndMakeVisible (overdubBtn_);

    // ---- Master meter ----
    addAndMakeVisible (masterMeter_);
    processor_.onLevelUpdate = [this](float l, float r){ masterMeter_.updateLevels(l, r); };

    // ---- About button ----
    aboutBtn_.onClick = [this] { AboutPanel::show(this); };
    aboutBtn_.setTooltip("About CHOPPR");
    addAndMakeVisible (aboutBtn_);

    // ---- Listeners ----
    padGrid_.addListener (this);
    processor_.getChangeListeners().add (this);

    // ---- Window properties ----
    setResizable (false, false);
}

ChopprEditor::~ChopprEditor()
{
    processor_.onBpmDetected  = nullptr;
    processor_.onMidiReceived = nullptr;
    processor_.onLevelUpdate  = nullptr;
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

    // Load Sample button: top-left of waveform area, beside the title
    loadSampleBtn_.setBounds (116, 4, 100, 24);

    // BPM status label: sits to the right of the Load Sample button
    bpmStatusLabel_.setBounds (222, 6, 160, 16);

    // Overdub toggle: sits to the right of the BPM status label
    overdubBtn_.setBounds (388, 4, 70, 22);

    // Waveform: top of left column
    waveformDisplay_.setBounds (0, 0, kLeftWidth, kWaveformHeight);

    // Pad grid: middle section of left column
    padGrid_.setBounds (0, kWaveformHeight, kLeftWidth, kPadGridHeight);

    // Transport: bottom strip of left column
    transportBar_.setBounds (0, kWaveformHeight + kPadGridHeight,
                              kLeftWidth, kTransportHeight);

    // Master meter: narrow vertical strip on the right edge of the transport bar
    masterMeter_.setBounds (getWidth() - 28, kWaveformHeight + kPadGridHeight, 24, 80);

    // About button: top-right corner
    aboutBtn_.setBounds (getWidth() - 26, 2, 22, 22);
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
    // Z – Undo last manual slice marker; Ctrl+Z – Undo last recording layer
    if (kc == 'Z' || kc == 'z')
    {
        if (ctrl)
            processor_.undoLastRecordingLayer();
        else
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
