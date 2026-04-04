#pragma once

#include "Common.h"

// Audio layer
#include "Audio/SampleBuffer.h"
#include "Audio/SliceEngine.h"
#include "Audio/PadVoice.h"
#include "Audio/PadVoicePool.h"
#include "Audio/Metronome.h"
#include "Audio/PitchShiftEngine.h"
#include "Audio/TimeStretchEngine.h"
#include "Audio/LoopRecorder.h"

// MIDI layer
#include "MIDI/MidiInputManager.h"

// State layer
#include "State/StateManager.h"

// Util layer
#include "Util/BPMDetector.h"
#include "Util/TransientDetector.h"
#include "Util/AudioFileExporter.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/**
 * Main AudioProcessor for the CHOPPR sampler plugin.
 *
 * Responsibilities:
 *   - Owns all sub-system objects (audio, MIDI, state, util layers).
 *   - Drives the real-time audio pipeline in processBlock().
 *   - Provides the editor with read/write access to shared state.
 *   - Handles save/restore via JUCE's getStateInformation / setStateInformation.
 */
class ChopprProcessor : public juce::AudioProcessor
{
public:
    //==========================================================================
    ChopprProcessor();
    ~ChopprProcessor() override;

    //==========================================================================
    // AudioProcessor interface
    //==========================================================================
    const juce::String getName() const override { return "CHOPPR"; }

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    bool acceptsMidi()  const override { return true;  }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return 0.0; }

    //==========================================================================
    int  getNumPrograms()                                    override { return 1; }
    int  getCurrentProgram()                                 override { return 0; }
    void setCurrentProgram (int)                             override {}
    const juce::String getProgramName (int)                  override { return "Default"; }
    void changeProgramName (int, const juce::String&)        override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& dest)           override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Sample loading
    //==========================================================================
    /** Load an audio file into the sample buffer.
     *  @param file   File to load.
     *  @param error  Populated with a description on failure.
     *  @return       true on success.
     */
    bool loadSample (const juce::File& file, juce::String& error);

    /** Unload the current sample and reset slices / pads. */
    void clearSample();

    const SampleBuffer& getSampleBuffer() const { return sampleBuffer_; }

    //==========================================================================
    // Slicing
    //==========================================================================
    SliceEngine&       getSliceEngine()       { return sliceEngine_; }
    const SliceEngine& getSliceEngine() const { return sliceEngine_; }

    /** Re-run slice detection using the given mode and the current BPM. */
    void autoSlice (SliceMode mode);

    /** Re-run slice detection with explicit TransientDetector settings.
     *  @param mode       Detection algorithm.
     *  @param settings   TransientDetector parameters (threshold, minSliceSeconds, etc.).
     *  @param maxSlices  Trim result to at most this many slices (0 = no limit).
     */
    void autoSliceWithSettings (SliceMode                          mode,
                                const TransientDetector::Settings& settings,
                                int                                maxSlices = 0);

    /** Launch background BPM detection for the loaded sample.
     *  When detection finishes, onBpmDetected is called on the message thread. */
    void detectBPM();

    /** Snaps every unlocked slice point to the nearest beat-grid position.
     *  @param subdivisions  1 = beats, 2 = 8th notes, 4 = 16th notes, etc. */
    void warpToGrid (int subdivisions = 1);

    /** Launch background auto-slice.  Returns immediately; progress is reported
     *  via onAutoSliceProgress (false = started, true = done). */
    void autoSliceAsync (SliceMode                          mode,
                         const TransientDetector::Settings& settings,
                         int                                maxSlices = 0);

    /** Optional callback invoked on the message thread during autoSliceAsync.
     *  false = analysis started, true = analysis complete.
     *  Must be cleared (set to nullptr) before the editor is destroyed. */
    std::function<void(bool done)> onAutoSliceProgress;

    //==========================================================================
    // Pad management
    //==========================================================================
    PadSettings&       getPadSettings (int padIndex)       { return padSettings_[padIndex]; }
    const PadSettings& getPadSettings (int padIndex) const { return padSettings_[padIndex]; }

    /** Update the settings for a single pad and notify listeners. */
    void setPadSettings (int padIndex, const PadSettings& settings);

    /** Number of pads that have an assigned slice (sliceIndex >= 0). */
    int getActivePadCount() const;

    GridSize getGridSize() const         { return gridSize_; }
    void     setGridSize (GridSize gs)   { gridSize_ = gs;   }

    //==========================================================================
    // Transport / tempo
    //==========================================================================
    Metronome& getMetronome() { return metronome_; }

    float getBPM()  const { return metronome_.getBPM(); }
    void  setBPM (float bpm);
    void  tapTempo() { metronome_.tapTempo(); }

    bool isPlaying()  const { return isPlaying_; }
    void setPlaying (bool playing);

    // Loop markers (sample positions; -1 = not set)
    void setLoopIn  (int samples) { loopInSamples_  = samples; sendChangeMessage(); }
    void setLoopOut (int samples) { loopOutSamples_ = samples; sendChangeMessage(); }
    int  getLoopIn()  const { return loopInSamples_;  }
    int  getLoopOut() const { return loopOutSamples_; }

    bool isRecording() const { return loopRecorder_.isRecording(); }

    /** Arm the loop recorder.
     *  @param preRollBeats  Count-in before actual capture starts.
     *  @param loopBeats     Length of the recorded loop in beats.
     */
    void startRecording (double preRollBeats = 1.0, double loopBeats = 4.0);
    void stopRecording();

    /** Remove the most recently completed recording layer (Ctrl+Z). */
    void undoLastRecordingLayer();

    /** Enable / disable overdub mode (new recordings layer over existing ones). */
    void setOverdubMode (bool overdub);

    bool isOverdubMode() const noexcept;

    //==========================================================================
    // Pitch / stretch
    //==========================================================================
    PitchShiftEngine&       getPitchShiftEngine()       { return pitchShiftEngine_; }
    TimeStretchEngine&      getTimeStretchEngine()      { return timeStretchEngine_; }

    float getGlobalPitchOffset() const            { return globalPitchOffset_; }
    void  setGlobalPitchOffset (float semitones);

    //==========================================================================
    // Warp subdivision
    //==========================================================================
    int  getWarpSubdivision() const noexcept { return warpSubdivision_; }
    void setWarpSubdivision (int sub) noexcept { warpSubdivision_ = sub; }

    //==========================================================================
    // Loop recorder access
    //==========================================================================
    LoopRecorder& getLoopRecorder() noexcept { return loopRecorder_; }

    //==========================================================================
    // MIDI
    //==========================================================================
    MidiInputManager& getMidiInputManager() { return midiInputManager_; }

    /** Optional callback invoked on the audio thread for every incoming MIDI
     *  message.  The editor sets this to forward events to MidiMonitorPanel.
     *  Must be cleared (set to nullptr) before the editor is destroyed. */
    std::function<void(const juce::MidiMessage&)> onMidiReceived;

    /** Level metering callback — set by editor, called from processBlock */
    std::function<void(float, float)> onLevelUpdate;

    /** Optional callback invoked on the message thread when background BPM
     *  detection completes.  @p valid is false if detection failed.
     *  Must be cleared (set to nullptr) before the editor is destroyed. */
    std::function<void(float bpm, bool valid)> onBpmDetected;

    //==========================================================================
    // Direct pad trigger (from UI grid click or internal routing)
    //==========================================================================
    /** Trigger pad @p padIndex with the given normalised velocity [0..1]. */
    void triggerPad (int padIndex, float velocity);

    /** Stop all voices for pad @p padIndex. */
    void releasePad (int padIndex);

    /** Returns true if any voice is currently playing for the given pad index. */
    bool isVoiceActiveForPad (int padIndex) const noexcept;

    //==========================================================================
    // Change notification (for UI)
    //==========================================================================
    juce::ListenerList<juce::ChangeListener>& getChangeListeners() { return changeListeners_; }

    /** Notify all registered ChangeListeners that plugin state has changed. */
    void sendChangeMessage();

private:
    //==========================================================================
    // Sub-systems
    SampleBuffer      sampleBuffer_;
    SliceEngine       sliceEngine_;
    PadVoicePool      voicePool_;
    Metronome         metronome_;
    PitchShiftEngine  pitchShiftEngine_;
    TimeStretchEngine timeStretchEngine_;
    LoopRecorder      loopRecorder_;
    MidiInputManager  midiInputManager_;
    StateManager      stateManager_;
    BPMDetector       bpmDetector_;

    //==========================================================================
    // State
    std::array<PadSettings, kMaxPads> padSettings_;
    GridSize  gridSize_          { GridSize::Grid4x4 };
    SliceMode currentSliceMode_  { SliceMode::Transient };
    float     globalPitchOffset_ { 0.0f };
    bool      isPlaying_         { false };
    int       loopInSamples_     { -1 };
    int       loopOutSamples_    { -1 };
    int       warpSubdivision_   { 4 };  // default 1/4 note

    //==========================================================================
    // Listener infrastructure
    juce::ListenerList<juce::ChangeListener> changeListeners_;

    //==========================================================================
    // Solo helpers
    /** Returns true if any pad in padSettings_ has solo == true. */
    bool anyPadSoloed() const noexcept;

    /** Builds a bitmask (bit N = pad N) of all soloed pads. */
    uint32_t buildSoloMask() const noexcept;

    //==========================================================================
    // Per-block scratch buffer (heap-allocated once in prepareToPlay)
    std::vector<MidiInputManager::PadEvent> padEventBuffer_;

    //==========================================================================
    // Background BPM analysis thread
    class BpmAnalysisThread : public juce::Thread
    {
    public:
        BpmAnalysisThread (ChopprProcessor& p)
            : juce::Thread ("CHOPPR BPM Analysis"), processor_ (p) {}
        void run() override;
    private:
        ChopprProcessor& processor_;
    };

    std::unique_ptr<BpmAnalysisThread> bpmThread_;
    std::atomic<bool>                  bpmAnalysisPending_ { false };

    //==========================================================================
    // Background auto-slice thread
    class AutoSliceThread : public juce::Thread
    {
    public:
        AutoSliceThread (ChopprProcessor& p)
            : juce::Thread ("CHOPPR AutoSlice"), processor_ (p) {}
        void run() override;

        // Set these before calling startThread()
        SliceMode                    mode     { SliceMode::Transient };
        TransientDetector::Settings  settings;
        int                          maxSlices { 64 };

    private:
        ChopprProcessor& processor_;
    };

    std::unique_ptr<AutoSliceThread> autoSliceThread_;
    std::atomic<bool>                autoSlicePending_ { false };

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChopprProcessor)
};
