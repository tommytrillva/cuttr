#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <array>

class ChopprProcessor;

class MasterMeterPanel : public juce::Component,
                         public juce::Timer
{
public:
    explicit MasterMeterPanel (ChopprProcessor& processor);
    ~MasterMeterPanel() override = default;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    /** Called from audio thread to update peak levels. Thread-safe. */
    void updateLevels (float leftPeak, float rightPeak) noexcept;

private:
    ChopprProcessor& processor_;

    std::atomic<float> leftPeak_  { 0.0f };
    std::atomic<float> rightPeak_ { 0.0f };

    float displayLeft_  { 0.0f };   // smoothed display values
    float displayRight_ { 0.0f };

    float holdLeft_  { 0.0f };     // peak-hold values
    float holdRight_ { 0.0f };
    int   holdTimerLeft_  { 0 };   // countdown frames before peak hold drops
    int   holdTimerRight_ { 0 };

    static constexpr float kDecayPerFrame  = 0.92f;   // per 30Hz frame
    static constexpr float kHoldFrames     = 60;       // 2 seconds at 30Hz
    static constexpr float kClipThreshold  = 0.99f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterMeterPanel)
};
