#pragma once

#include "PluginProcessor.h"
#include "UI/WaveformDisplay.h"
#include "UI/PadGrid.h"
#include "UI/TransportBar.h"
#include "UI/SliceEditorPanel.h"
#include "UI/PitchPanel.h"
#include "UI/TimeStretchPanel.h"
#include "UI/AudioSetupPanel.h"
#include "UI/ExportPanel.h"
#include "UI/PresetBrowser.h"
#include "UI/MidiMonitorPanel.h"
#include "UI/MasterMeterPanel.h"
#include "UI/AboutPanel.h"
#include "UI/PadFxPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
 * ChopprEditor
 *
 * Main plugin editor window (900 x 720).
 *
 * Layout:
 *   Left column (600 px wide):
 *     - WaveformDisplay    (top 200 px)
 *     - PadGrid            (middle, fills remaining height minus transport)
 *     - TransportBar       (bottom 60 px)
 *
 *   Right panel (280 px wide, full height):
 *     - TabbedComponent with: Slice, Pitch, Stretch, Export, Presets, Audio
 *
 * Drag-and-drop of audio files onto any part of the editor loads the sample.
 */
class ChopprEditor : public juce::AudioProcessorEditor,
                      public juce::ChangeListener,
                      public PadGrid::Listener,
                      public juce::FileDragAndDropTarget
{
public:
    //==========================================================================
    explicit ChopprEditor (ChopprProcessor& processor);
    ~ChopprEditor() override;

    //==========================================================================
    // juce::Component
    void paint      (juce::Graphics& g) override;
    void resized    () override;
    bool keyPressed (const juce::KeyPress& key) override;

    //==========================================================================
    // juce::ChangeListener
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    //==========================================================================
    // PadGrid::Listener
    void padTriggered (int padIndex, float velocity) override;
    void padSelected  (int padIndex) override;

    //==========================================================================
    // juce::FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    //==========================================================================
    /** Synchronise the pad grid's visual selection state to selectedPad_. */
    void updatePadGridSelection();

    /** Add a slice marker at the current metronome playhead position. */
    void dropSliceAtPlayhead();

    //==========================================================================
    ChopprProcessor& processor_;

    // --- Left column ---
    WaveformDisplay  waveformDisplay_;
    PadGrid          padGrid_;
    TransportBar     transportBar_;

    // --- Right panel ---
    juce::TabbedComponent rightPanel_;
    SliceEditorPanel      sliceEditorPanel_;
    PitchPanel            pitchPanel_;
    TimeStretchPanel      timeStretchPanel_;
    ExportPanel           exportPanel_;
    PresetBrowser         presetBrowser_;

    // AudioSetupPanel is shown in a separate tab of rightPanel_ and owns a
    // local AudioDeviceManager that is only used in standalone mode.
    juce::AudioDeviceManager deviceManager_;
    AudioSetupPanel          audioSetupPanel_;

    MidiMonitorPanel         midiMonitor_;
    PadFxPanel               padFxPanel_;

    // --- File loading ---
    juce::TextButton                   loadSampleBtn_ { "Load Sample" };
    std::unique_ptr<juce::FileChooser> fileChooser_;

    // --- Overdub toggle ---
    juce::TextButton overdubBtn_ { "OVERDUB" };

    // --- Master output meter ---
    MasterMeterPanel masterMeter_;

    // --- About button ---
    juce::TextButton aboutBtn_ { "?" };

    // --- BPM detection status ---
    juce::Label bpmStatusLabel_;

    // --- State ---
    int selectedPad_ { -1 };

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChopprEditor)
};
