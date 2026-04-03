cmake_minimum_required(VERSION 3.22)
project(CHOPPR VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Source files
set(CHOPPR_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Audio/SampleBuffer.cpp
    Source/Audio/SliceEngine.cpp
    Source/Audio/PadVoice.cpp
    Source/Audio/PadVoicePool.cpp
    Source/Audio/Metronome.cpp
    Source/Audio/PitchShiftEngine.cpp
    Source/Audio/TimeStretchEngine.cpp
    Source/Audio/LoopRecorder.cpp
    Source/MIDI/MidiInputManager.cpp
    Source/MIDI/PadMapper.cpp
    Source/UI/WaveformDisplay.cpp
    Source/UI/PadGrid.cpp
    Source/UI/TransportBar.cpp
    Source/UI/SliceEditorPanel.cpp
    Source/UI/PitchPanel.cpp
    Source/UI/TimeStretchPanel.cpp
    Source/UI/AudioSetupPanel.cpp
    Source/UI/ExportPanel.cpp
    Source/UI/PresetBrowser.cpp
    Source/State/StateManager.cpp
    Source/Util/BPMDetector.cpp
    Source/Util/TransientDetector.cpp
    Source/Util/AudioFileExporter.cpp
)

juce_add_plugin(CHOPPR
    COMPANY_NAME "Choppr"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Chpr
    PLUGIN_CODE Chp1
    FORMATS VST3 Standalone
    PRODUCT_NAME "CHOPPR"
)

target_sources(CHOPPR PRIVATE ${CHOPPR_SOURCES})

target_include_directories(CHOPPR PRIVATE Source)

target_compile_definitions(CHOPPR PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_ASIO=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(CHOPPR
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
