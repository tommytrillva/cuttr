#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChopprProcessor::~ChopprProcessor()
{
    if (autoSliceThread_ && autoSliceThread_->isThreadRunning())
        autoSliceThread_->stopThread (2000);

    if (bpmThread_ && bpmThread_->isThreadRunning())
        bpmThread_->stopThread (2000);
}

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
    sampleBuffer_.prepareToPlay (sampleRate);   // propagate session rate for resampling on load
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

    // --- Sync BPM from DAW transport (if a play-head is available) ---
    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            if (const auto bpm = pos->getBpm())
                if (*bpm > 0.0)
                    setBPM (static_cast<float> (*bpm));
        }
    }

    // We render entirely from the sample – clear the output first
    buffer.clear();

    if (! sampleBuffer_.hasAudio())
        return;

    // --- MIDI -> pad events ---
    padEventBuffer_.clear();
    midiInputManager_.processMidiBuffer (midiMessages, padEventBuffer_);
    // NOTE: midiMessages is intentionally NOT cleared here so the MIDI stream
    // passes through to the DAW for recording.  The VST3 wrapper forwards it.

    // --- Forward raw MIDI messages to the monitor panel (if wired up) ---
    if (onMidiReceived)
    {
        for (const auto meta : midiMessages)
            onMidiReceived (meta.getMessage());
    }

    for (const auto& event : padEventBuffer_)
    {
        if (event.padIndex < 0 || event.padIndex >= kMaxPads)
            continue;

        if (event.isNoteOn)
            triggerPad (event.padIndex, event.velocity);
        else
            releasePad (event.padIndex);
    }

    // --- Solo gating: inform the voice pool which pads are soloed this block ---
    voicePool_.setSoloMask (buildSoloMask());

    // --- Voice rendering (always active; pads play on MIDI trigger regardless
    //     of transport state) ---
    voicePool_.renderNextBlock (buffer, 0, buffer.getNumSamples(), sampleBuffer_);

    // --- Metronome click (transport-gated) ---
    if (isPlaying_)
        metronome_.processBlock (buffer, buffer.getNumSamples());

    // --- Loop recorder (transport-gated; captures from the rendered output) ---
    if (isPlaying_)
        loopRecorder_.processBlock (buffer, buffer.getNumSamples(),
                                    metronome_.getBeatPosition());

    // --- Output level metering ---
    if (onLevelUpdate)
    {
        float lPeak = 0.0f, rPeak = 0.0f;
        if (buffer.getNumChannels() > 0)
            lPeak = buffer.getMagnitude(0, 0, buffer.getNumSamples());
        if (buffer.getNumChannels() > 1)
            rPeak = buffer.getMagnitude(1, 0, buffer.getNumSamples());
        onLevelUpdate(lPeak, rPeak);
    }
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

    auto state = stateManager_.saveState (
        padVec,
        metronome_.getBPM(),
        currentSliceMode_,
        gridSize_,
        timeStretchEngine_.getMode(),
        pitchShiftEngine_.getMode(),
        globalPitchOffset_,
        sampleBuffer_.getSourceFile().getFullPathName(),
        slicePoints);

    // --- Warp subdivision ---
    state.setProperty ("warpSubdivision", warpSubdivision_, nullptr);

    // --- Loop recording buffer ---
    const auto& recBuf = loopRecorder_.getRecordedBuffer();
    if (recBuf.getNumSamples() > 0)
    {
        const int maxSamples = juce::roundToInt (getSampleRate() * 30.0);
        const int samplesToSave = juce::jmin (recBuf.getNumSamples(), maxSamples);

        juce::MemoryOutputStream memStream;
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wavFormat.createWriterFor (&memStream,
                                       getSampleRate(),
                                       static_cast<unsigned int> (recBuf.getNumChannels()),
                                       16, {}, 0));
        if (writer != nullptr)
        {
            writer->writeFromAudioSampleBuffer (recBuf, 0, samplesToSave);
            writer->flush();
            writer.reset();
            state.setProperty ("recordingData",
                               memStream.getMemoryBlock().toBase64Encoding(),
                               nullptr);
        }
    }

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

        // --- Warp subdivision ---
        warpSubdivision_ = static_cast<int> (state.getProperty ("warpSubdivision", 4));

        // --- Loop recording buffer ---
        const juce::String recData = state.getProperty ("recordingData", "").toString();
        if (recData.isNotEmpty())
        {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding (recData))
            {
                juce::MemoryInputStream memIn (mb, false);
                juce::WavAudioFormat wavFormat;
                std::unique_ptr<juce::AudioFormatReader> reader (
                    wavFormat.createReaderFor (&memIn, false));
                if (reader != nullptr)
                {
                    juce::AudioBuffer<float> loaded (static_cast<int> (reader->numChannels),
                                                     static_cast<int> (reader->lengthInSamples));
                    reader->read (&loaded, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);
                    loopRecorder_.setRecordedBuffer (std::move (loaded));
                }
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

void ChopprProcessor::autoSliceWithSettings (SliceMode                          mode,
                                              const TransientDetector::Settings& settings,
                                              int                                maxSlices)
{
    if (! sampleBuffer_.hasAudio())
        return;

    currentSliceMode_ = mode;

    if (mode == SliceMode::Transient)
    {
        // For transient mode, run the detector directly with the provided settings
        // then build slice points and push them into the engine.
        TransientDetector detector;
        auto positions = detector.detectTransients (sampleBuffer_.getBuffer(),
                                                    sampleBuffer_.getSampleRate(),
                                                    settings);

        // Apply maxSlices cap before building SlicePoint list
        if (maxSlices > 0 && static_cast<int> (positions.size()) > maxSlices)
            positions.resize (static_cast<size_t> (maxSlices));

        std::vector<SlicePoint> slices;
        slices.reserve (positions.size());
        for (int pos : positions)
        {
            SlicePoint sp;
            sp.samplePosition = pos;
            sp.isLocked       = false;
            slices.push_back (sp);
        }
        sliceEngine_.setSlices (std::move (slices));
    }
    else
    {
        // For non-transient modes, fall back to the standard auto-detect
        // and trim afterwards if a maxSlices cap was requested.
        sliceEngine_.autoDetectSlices (sampleBuffer_.getBuffer(),
                                       sampleBuffer_.getSampleRate(),
                                       mode,
                                       metronome_.getBPM());

        if (maxSlices > 0 && sliceEngine_.getNumSlices() > maxSlices)
        {
            auto slices = sliceEngine_.getSlices();
            slices.resize (static_cast<size_t> (maxSlices));
            sliceEngine_.setSlices (std::move (slices));
        }
    }

    sendChangeMessage();
}

void ChopprProcessor::detectBPM()
{
    if (! sampleBuffer_.hasAudio()) return;
    if (bpmAnalysisPending_.exchange (true)) return; // already running

    // Stop a previous thread if it hasn't exited yet
    if (bpmThread_ && bpmThread_->isThreadRunning())
        bpmThread_->stopThread (1000);

    bpmThread_ = std::make_unique<BpmAnalysisThread> (*this);
    bpmThread_->startThread (juce::Thread::Priority::background);
}

void ChopprProcessor::BpmAnalysisThread::run()
{
    if (threadShouldExit()) return;

    // Safe to read: bpmAnalysisPending_ prevents the processor from starting
    // another thread (and thus swapping the buffer) while we are running.
    const auto result = processor_.bpmDetector_.detectBPM (
        processor_.sampleBuffer_.getBuffer(),
        processor_.sampleBuffer_.getSampleRate());

    processor_.bpmAnalysisPending_.store (false);

    if (threadShouldExit())
        return;

    if (result.valid)
    {
        juce::MessageManager::callAsync ([&proc = processor_, bpm = result.bpm]
        {
            proc.setBPM (bpm);
            proc.timeStretchEngine_.setSampleBPM (bpm);
            if (proc.onBpmDetected) proc.onBpmDetected (bpm, true);
        });
    }
    else
    {
        juce::MessageManager::callAsync ([&proc = processor_]
        {
            if (proc.onBpmDetected) proc.onBpmDetected (0.0f, false);
        });
    }
}

void ChopprProcessor::warpToGrid (int subdivisions)
{
    const float  bpm = metronome_.getBPM();
    const double sr  = sampleBuffer_.getSampleRate();
    if (bpm <= 0.0f || sr <= 0.0) return;

    const double samplesPerBeat = (sr * 60.0) / static_cast<double> (bpm);
    sliceEngine_.warpToGrid (samplesPerBeat, subdivisions);
    sendChangeMessage();
}

void ChopprProcessor::autoSliceAsync (SliceMode                          mode,
                                       const TransientDetector::Settings& settings,
                                       int                                maxSlices)
{
    if (autoSlicePending_.exchange (true)) return; // already running

    if (autoSliceThread_ && autoSliceThread_->isThreadRunning())
        autoSliceThread_->stopThread (500);

    autoSliceThread_             = std::make_unique<AutoSliceThread> (*this);
    autoSliceThread_->mode       = mode;
    autoSliceThread_->settings   = settings;
    autoSliceThread_->maxSlices  = maxSlices;
    autoSliceThread_->startThread (juce::Thread::Priority::normal);
}

void ChopprProcessor::AutoSliceThread::run()
{
    juce::MessageManager::callAsync ([&proc = processor_]
    {
        if (proc.onAutoSliceProgress) proc.onAutoSliceProgress (false); // started
    });

    if (! threadShouldExit())
        processor_.autoSliceWithSettings (mode, settings, maxSlices);

    processor_.autoSlicePending_.store (false);

    juce::MessageManager::callAsync ([&proc = processor_]
    {
        if (proc.onAutoSliceProgress) proc.onAutoSliceProgress (true); // done
    });
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
// Solo helpers
//==============================================================================
bool ChopprProcessor::anyPadSoloed() const noexcept
{
    for (const auto& pad : padSettings_)
        if (pad.solo)
            return true;
    return false;
}

uint32_t ChopprProcessor::buildSoloMask() const noexcept
{
    if (! anyPadSoloed())
        return 0u;

    uint32_t mask = 0u;
    for (int i = 0; i < kMaxPads; ++i)
        if (padSettings_[i].solo)
            mask |= (1u << static_cast<uint32_t> (i));
    return mask;
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

void ChopprProcessor::undoLastRecordingLayer()
{
    loopRecorder_.undoLastLayer();
    sendChangeMessage();
}

void ChopprProcessor::setOverdubMode (bool overdub)
{
    loopRecorder_.setOverdubMode (overdub);
}

bool ChopprProcessor::isOverdubMode() const noexcept
{
    return loopRecorder_.isOverdubMode();
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
    voicePool_.padNoteOff (padIndex);
}

bool ChopprProcessor::isVoiceActiveForPad (int padIndex) const noexcept
{
    return voicePool_.isAnyVoiceActiveForPad (padIndex);
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
