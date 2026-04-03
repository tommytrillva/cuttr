#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
/**
 * AudioSetupPanel
 *
 * Full-screen overlay panel for audio device configuration.
 * Hosts JUCE's built-in AudioDeviceSelectorComponent.
 *
 * NOTE: This component takes a juce::AudioDeviceManager reference, NOT a
 * ChopprProcessor reference, because device management lives in the host /
 * standalone wrapper layer and must be shared with the JUCE AudioDeviceManager.
 */
class AudioSetupPanel : public juce::Component
{
public:
    //==========================================================================
    /**
     * @param deviceManager  The application-wide AudioDeviceManager to configure.
     * @param numInputChannels   Desired number of input channels to expose.
     * @param numOutputChannels  Desired number of output channels to expose.
     */
    AudioSetupPanel (juce::AudioDeviceManager& deviceManager,
                     int numInputChannels  = 0,
                     int numOutputChannels = 2);
    ~AudioSetupPanel() override = default;

    //==========================================================================
    // juce::Component
    void paint   (juce::Graphics& g) override;
    void resized () override;

private:
    //==========================================================================
    juce::Label titleLabel_;

    /** JUCE's stock audio-device selector UI. */
    juce::AudioDeviceSelectorComponent deviceSelector_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSetupPanel)
};
