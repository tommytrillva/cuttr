#include "ExportPanel.h"
#include "../PluginProcessor.h"

static const juce::Colour kBg      = juce::Colour (0xff1a1a2e);
static const juce::Colour kSurface = juce::Colour (0xff16213e);
static const juce::Colour kAccent  = juce::Colour (0xffe94560);
static const juce::Colour kText    = juce::Colour (0xffeeeeee);
static const juce::Colour kDim     = juce::Colour (0xff888888);

//==============================================================================
ExportPanel::ExportPanel (ChopprProcessor& processor)
    : processor_ (processor)
{
    // Title
    titleLabel_.setText ("EXPORT", juce::dontSendNotification);
    titleLabel_.setFont (juce::Font (13.0f, juce::Font::bold));
    titleLabel_.setColour (juce::Label::textColourId, kText);
    titleLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel_);

    // Format label + combo
    formatLabel_.setText ("Format", juce::dontSendNotification);
    formatLabel_.setFont (juce::Font (11.0f));
    formatLabel_.setColour (juce::Label::textColourId, kDim);
    formatLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (formatLabel_);

    formatComboBox_.addItem ("WAV",  1);
    formatComboBox_.addItem ("FLAC", 2);
    formatComboBox_.addItem ("MP3",  3);
    formatComboBox_.setSelectedId (1, juce::dontSendNotification);
    formatComboBox_.setColour (juce::ComboBox::backgroundColourId,   kBg);
    formatComboBox_.setColour (juce::ComboBox::textColourId,          kText);
    formatComboBox_.setColour (juce::ComboBox::outlineColourId,       juce::Colour (0xff333355));
    formatComboBox_.setColour (juce::ComboBox::arrowColourId,         kAccent);
    formatComboBox_.setColour (juce::ComboBox::focusedOutlineColourId, kAccent);
    addAndMakeVisible (formatComboBox_);

    // Bit depth label + combo
    bitDepthLabel_.setText ("Bit Depth", juce::dontSendNotification);
    bitDepthLabel_.setFont (juce::Font (11.0f));
    bitDepthLabel_.setColour (juce::Label::textColourId, kDim);
    bitDepthLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (bitDepthLabel_);

    bitDepthComboBox_.addItem ("16-bit", 1);
    bitDepthComboBox_.addItem ("24-bit", 2);
    bitDepthComboBox_.addItem ("32-bit", 3);
    bitDepthComboBox_.setSelectedId (2, juce::dontSendNotification); // default 24-bit
    bitDepthComboBox_.setColour (juce::ComboBox::backgroundColourId,   kBg);
    bitDepthComboBox_.setColour (juce::ComboBox::textColourId,          kText);
    bitDepthComboBox_.setColour (juce::ComboBox::outlineColourId,       juce::Colour (0xff333355));
    bitDepthComboBox_.setColour (juce::ComboBox::arrowColourId,         kAccent);
    bitDepthComboBox_.setColour (juce::ComboBox::focusedOutlineColourId, kAccent);
    addAndMakeVisible (bitDepthComboBox_);

    // Export button
    exportButton_.setColour (juce::TextButton::buttonColourId,  kAccent.darker (0.3f));
    exportButton_.setColour (juce::TextButton::buttonOnColourId, kAccent);
    exportButton_.setColour (juce::TextButton::textColourOffId,  kText);
    exportButton_.setColour (juce::TextButton::textColourOnId,   kText);
    exportButton_.addListener (this);
    addAndMakeVisible (exportButton_);

    // Export Slices button
    exportSlicesButton_.setColour (juce::TextButton::buttonColourId,  kSurface);
    exportSlicesButton_.setColour (juce::TextButton::buttonOnColourId, kAccent);
    exportSlicesButton_.setColour (juce::TextButton::textColourOffId,  kText);
    exportSlicesButton_.setColour (juce::TextButton::textColourOnId,   kText);
    exportSlicesButton_.addListener (this);
    addAndMakeVisible (exportSlicesButton_);
}

//==============================================================================
void ExportPanel::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (juce::Colour (0xff0a0a1a));
    g.drawLine (static_cast<float> (getWidth() - 1), 0.0f,
                static_cast<float> (getWidth() - 1), static_cast<float> (getHeight()),
                1.0f);
}

void ExportPanel::resized()
{
    const int pad   = 10;
    const int w     = getWidth() - pad * 2;
    const int itemH = 26;
    const int gap   = 6;

    int y = pad;

    titleLabel_.setBounds      (pad, y, w, 20); y += 24;
    y += 4;

    formatLabel_.setBounds     (pad, y, w, 14); y += 16;
    formatComboBox_.setBounds  (pad, y, w, itemH); y += itemH + gap;

    bitDepthLabel_.setBounds   (pad, y, w, 14); y += 16;
    bitDepthComboBox_.setBounds(pad, y, w, itemH); y += itemH + gap * 2;

    exportButton_.setBounds      (pad, y, w, itemH); y += itemH + gap;
    exportSlicesButton_.setBounds(pad, y, w, itemH);
}

//==============================================================================
void ExportPanel::buttonClicked (juce::Button* button)
{
    if (button == &exportButton_)
        doExportBuffer();
    else if (button == &exportSlicesButton_)
        doExportSlices();
}

//==============================================================================
AudioFileExporter::ExportSettings ExportPanel::buildSettings (const juce::File& dest) const
{
    AudioFileExporter::ExportSettings settings;

    settings.outputFile  = dest;
    settings.sampleRate  = processor_.getSampleBuffer().getSampleRate();

    // Format
    switch (formatComboBox_.getSelectedId())
    {
        case 2:  settings.format = AudioFileExporter::Format::FLAC; break;
        case 3:  settings.format = AudioFileExporter::Format::MP3;  break;
        default: settings.format = AudioFileExporter::Format::WAV;  break;
    }

    // Bit depth
    switch (bitDepthComboBox_.getSelectedId())
    {
        case 1:  settings.bitDepth = AudioFileExporter::BitDepth::Bits16; break;
        case 3:  settings.bitDepth = AudioFileExporter::BitDepth::Bits32; break;
        default: settings.bitDepth = AudioFileExporter::BitDepth::Bits24; break;
    }

    return settings;
}

void ExportPanel::doExportBuffer()
{
    if (!processor_.getSampleBuffer().hasAudio())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Export",
                                                "No sample loaded.");
        return;
    }

    juce::String ext;
    switch (formatComboBox_.getSelectedId())
    {
        case 2:  ext = "flac"; break;
        case 3:  ext = "mp3";  break;
        default: ext = "wav";  break;
    }

    fileChooser_ = std::make_unique<juce::FileChooser> (
        "Export Sample",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*." + ext);

    fileChooser_->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto result = chooser.getResult();
            if (result == juce::File{}) return;

            juce::String errorMsg;
            const auto settings = buildSettings (result);
            if (!exporter_.exportBuffer (processor_.getSampleBuffer().getBuffer(),
                                          settings, errorMsg))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Export Failed", errorMsg);
            }
        });
}

void ExportPanel::doExportSlices()
{
    if (!processor_.getSampleBuffer().hasAudio())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Export Slices",
                                                "No sample loaded.");
        return;
    }

    if (processor_.getSliceEngine().getNumSlices() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Export Slices",
                                                "No slice points defined.");
        return;
    }

    juce::String ext2;
    switch (formatComboBox_.getSelectedId())
    {
        case 2:  ext2 = "flac"; break;
        case 3:  ext2 = "mp3";  break;
        default: ext2 = "wav";  break;
    }

    fileChooser_ = std::make_unique<juce::FileChooser> (
        "Export Slices – choose output file template",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*." + ext2);

    fileChooser_->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto result = chooser.getResult();
            if (result == juce::File{}) return;

            juce::String errorMsg;
            const auto settings = buildSettings (result);
            const auto& slices  = processor_.getSliceEngine().getSlices();
            if (!exporter_.exportSlices (processor_.getSampleBuffer().getBuffer(),
                                          slices, settings, errorMsg))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Export Slices Failed", errorMsg);
            }
        });
}
