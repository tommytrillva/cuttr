#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common.h"
#include "../Util/AudioFileExporter.h"

class ChopprProcessor;

//==============================================================================
/**
 * ExportPanel
 *
 * Panel for audio export operations:
 *   - Section title: "EXPORT"
 *   - Format selector     (WAV / FLAC)
 *   - Bit-depth selector  (16-bit / 24-bit / 32-bit)
 *   - "Export..."         – write the whole sample to one file
 *   - "Export Slices..."  – write each slice as a separate numbered file
 */
class ExportPanel : public juce::Component,
                    public juce::Button::Listener
{
public:
    //==========================================================================
    explicit ExportPanel (ChopprProcessor& processor);
    ~ExportPanel() override = default;

    //==========================================================================
    // juce::Component
    void paint   (juce::Graphics& g) override;
    void resized () override;

    //==========================================================================
    // juce::Button::Listener
    void buttonClicked (juce::Button* button) override;

private:
    //==========================================================================
    /** Build ExportSettings from the current widget values. */
    AudioFileExporter::ExportSettings buildSettings (const juce::File& dest) const;

    /** Run the export-whole-file flow. */
    void doExportBuffer();

    /** Run the export-slices flow. */
    void doExportSlices();

    /** Show/hide the MP3 bitrate controls based on the selected format. */
    void updateMp3Visibility();

    //==========================================================================
    ChopprProcessor& processor_;

    juce::Label    titleLabel_;
    juce::Label    formatLabel_;
    juce::ComboBox formatComboBox_;
    juce::Label    bitDepthLabel_;
    juce::ComboBox bitDepthComboBox_;

    // ---- Normalise ----
    juce::ToggleButton normaliseToggle_ { "Normalise" };

    // ---- MP3 bitrate ----
    juce::ComboBox mp3BitrateCombo_;
    juce::Label    mp3BitrateLabel_;

    // ---- Sample rate ----
    juce::ComboBox sampleRateCombo_;
    juce::Label    sampleRateLabel_;

    juce::TextButton exportButton_       { "Export..."        };
    juce::TextButton exportSlicesButton_ { "Export Slices..." };

    /** Owned file chooser – must outlive the async callback. */
    std::unique_ptr<juce::FileChooser> fileChooser_;

    /** Exporter instance (stateless apart from the format manager). */
    AudioFileExporter exporter_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportPanel)
};
