#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChopprProcessor::ChopprProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Default pad settings: pad i plays slice i
    for (int i = 0; i < kMaxPads; ++i)
    {
        padSettings_[i] = PadSettings{};
        padSettings_[i].sliceIndex = i;
    }

    metronome_.setTempo (120.0f);
    timeStretchEngine_.setProjectBPM (120.0f);
}

//==============================================================================
void ChopprProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    voicePool_.prepare (sampleRate, samplesPerBlock);
    metronome_.prepare (sampleRate, samplesPerBlock);
    loopRecorder_.prepare (sampleRate, samplesPerBlock);
    padEventBuffer_.reserve (64);
}

void ChopprProcessor::releaseResources()
{
    voicePool_.allNotesOff();
}

bool ChopprProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // We only support stereo output; no audio input required
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    return true;
}

//==============================================================================
void ChopprProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // We render entirely from the sample – clear the output first
    buffer.clear();

    if (! sampleBuffer_.hasAudio())
    {
        midiMessages.clear();
        return;
    }

    // --- MIDI -> pad events ---
    padEventBuffer_.clear();
    midiInputManager_.processMidiBuffer (midiMessages, padEventBuffer_);
    midiMessages.clear();

    for (const auto& event : padEventBuffer_)
    {
        if (event.padIndex < 0 || event.padIndex >= kMaxPads)
            continue;

        if (event.isNoteOn)
            triggerPad (event.padIndex, event.velocity);
        else
            releasePad (event.padIndex);
    }

    // --- Voice rendering ---
    if (isPlaying_)
        voicePool_.renderNextBlock (buffer, 0, buffer.getNumSamples(), sampleBuffer_);

    // --- Metronome click ---
    metronome_.processBlock (buffer, buffer.getNumSamples());

    // --- Loop recorder (captures from the rendered output) ---
    loopRecorder_.processBlock (buffer, buffer.getNumSamples(),
                                 metronome_.getBeatPosition());
}

//==============================================================================
juce::AudioProcessorEditor* ChopprProcessor::createEditor()
{
    return new ChopprEditor (*this);
}

//==============================================================================
void ChopprProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    // Build a flat vector of pad settings from the array
    std::vector<PadSettings> padVec (padSettings_.begin(), padSettings_.end());

    // Collect current slice points from the slice engine
    const auto& slicePoints = sliceEngine_.getSlices();

    const auto state = stateManager_.saveState (
        padVec,
        metronome_.getBPM(),
        currentSliceMode_,
        gridSize_,
        timeStretchEngine_.getMode(),
        pitchShiftEngine_.getMode(),
        globalPitchOffset_,
        sampleBuffer_.getSourceFile().getFullPathName(),
        slicePoints);

    stateManager_.getStateInformation (state, dest);
}

void ChopprProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto state = stateManager_.setStateInformation (data, sizeInBytes);
    if (! state.isValid())
        return;

    std::vector<PadSettings> padVec;
    float bpm = 120.0f;
    SliceMode sliceMode = SliceMode::Transient;
    GridSize gs = GridSize::Grid4x4;
    TimeStretchMode stretchMode = TimeStretchMode::None;
    PitchMode pitchMode = PitchMode::Chromatic;
    float globalPitch = 0.0f;
    juce::String filePath;
    std::vector<SlicePoint> slicePoints;

    if (stateManager_.loadState (state, padVec, bpm, sliceMode, gs,
                                  stretchMode, pitchMode, globalPitch,
                                  filePath, slicePoints))
    {
        // Restore pad settings
        for (int i = 0; i < kMaxPads && i < static_cast<int> (padVec.size()); ++i)
            padSettings_[i] = padVec[i];

        currentSliceMode_ = sliceMode;
        gridSize_ = gs;
        globalPitchOffset_ = globalPitch;

        metronome_.setTempo (bpm);
        timeStretchEngine_.setProjectBPM (bpm);
        timeStretchEngine_.setMode (stretchMode);
        pitchShiftEngine_.setMode (pitchMode);
        pitchShiftEngine_.setGlobalOffset (globalPitch);

        sliceEngine_.setSlices (slicePoints);

        // Reload the sample file if it still exists
        if (filePath.isNotEmpty())
        {
            juce::File f (filePath);
            if (f.existsAsFile())
            {
                juce::String err;
                sampleBuffer_.loadFromFile (f, err);
            }
        }

        sendChangeMessage();
    }
}

//==============================================================================
// Sample loading
//==============================================================================
bool ChopprProcessor::loadSample (const juce::File& file, juce::String& error)
{
    if (! sampleBuffer_.loadFromFile (file, error))
        return false;

    detectBPM();
    autoSlice (currentSliceMode_);
    sendChangeMessage();
    return true;
}

void ChopprProcessor::clearSample()
{
    voicePool_.allNotesOff();
    sampleBuffer_.clear();
    sliceEngine_.clearSlices();
    sendChangeMessage();
}

//==============================================================================
// Slicing
//==============================================================================
void ChopprProcessor::autoSlice (SliceMode mode)
{
    if (! sampleBuffer_.hasAudio())
        return;

    currentSliceMode_ = mode;
    sliceEngine_.autoDetectSlices (sampleBuffer_.getBuffer(),
                                    sampleBuffer_.getSampleRate(),
                                    mode,
                                    metronome_.getBPM());
    sendChangeMessage();
}

void ChopprProcessor::detectBPM()
{
    if (! sampleBuffer_.hasAudio())
        return;

    const auto result = bpmDetector_.detectBPM (sampleBuffer_.getBuffer(),
                                                  sampleBuffer_.getSampleRate());
    if (result.valid)
    {
        metronome_.setTempo (result.bpm);
        timeStretchEngine_.setProjectBPM (result.bpm);
        timeStretchEngine_.setSampleBPM (result.bpm);
    }
}

//==============================================================================
// Pad management
//==============================================================================
void ChopprProcessor::setPadSettings (int padIndex, const PadSettings& settings)
{
    jassert (padIndex >= 0 && padIndex < kMaxPads);
    padSettings_[padIndex] = settings;
    sendChangeMessage();
}

int ChopprProcessor::getActivePadCount() const
{
    int count = 0;
    for (const auto& pad : padSettings_)
        if (pad.sliceIndex >= 0)
            ++count;
    return count;
}

//==============================================================================
// Transport
//==============================================================================
void ChopprProcessor::setBPM (float bpm)
{
    metronome_.setTempo (bpm);
    timeStretchEngine_.setProjectBPM (bpm);
}

void ChopprProcessor::setPlaying (bool playing)
{
    isPlaying_ = playing;
    if (! playing)
    {
        voicePool_.allNotesOff();
        metronome_.reset();
    }
}

void ChopprProcessor::startRecording (double preRollBeats, double loopBeats)
{
    loopRecorder_.startRecording (preRollBeats, loopBeats, metronome_.getBPM());
}

void ChopprProcessor::stopRecording()
{
    loopRecorder_.stopRecording();
}

//==============================================================================
// Pitch / stretch
//==============================================================================
void ChopprProcessor::setGlobalPitchOffset (float semitones)
{
    globalPitchOffset_ = semitones;
    pitchShiftEngine_.setGlobalOffset (semitones);
}

//==============================================================================
// Direct pad trigger (from UI)
//==============================================================================
void ChopprProcessor::triggerPad (int padIndex, float velocity)
{
    if (! sampleBuffer_.hasAudio())
        return;

    if (padIndex < 0 || padIndex >= kMaxPads)
        return;

    const auto& settings = padSettings_[padIndex];
    if (settings.mute)
        return;

    const double pitchRate = pitchShiftEngine_.getPlaybackRate (settings.pitchOffsetSemitones);
    const double effectiveRate = timeStretchEngine_.getEffectivePlaybackRate (
        static_cast<float> (pitchRate),
        settings.stretchRatio);

    voicePool_.triggerPad (padIndex, settings, velocity,
                            sampleBuffer_, sliceEngine_, effectiveRate);
}

void ChopprProcessor::releasePad (int padIndex)
{
    voicePool_.stopPad (padIndex);
}

//==============================================================================
// Change notification
//==============================================================================
void ChopprProcessor::sendChangeMessage()
{
    changeListeners_.call (&juce::ChangeListener::changeListenerCallback, nullptr);
}

//==============================================================================
// Plugin factory
//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChopprProcessor();
}
