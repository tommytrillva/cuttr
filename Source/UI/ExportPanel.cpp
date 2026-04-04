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

    // ---- Normalise toggle ----
    normaliseToggle_.setColour (juce::ToggleButton::textColourId,  kText);
    normaliseToggle_.setColour (juce::ToggleButton::tickColourId,  kAccent);
    normaliseToggle_.setColour (juce::ToggleButton::tickDisabledColourId, kDim);
    addAndMakeVisible (normaliseToggle_);

    // ---- MP3 bitrate ----
    mp3BitrateLabel_.setText ("MP3 Bitrate", juce::dontSendNotification);
    mp3BitrateLabel_.setFont (juce::Font (11.0f));
    mp3BitrateLabel_.setColour (juce::Label::textColourId, kDim);
    mp3BitrateLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (mp3BitrateLabel_);

    mp3BitrateCombo_.addItem ("128 kbps", 128);
    mp3BitrateCombo_.addItem ("192 kbps", 192);
    mp3BitrateCombo_.addItem ("320 kbps", 320);
    mp3BitrateCombo_.setSelectedId (320, juce::dontSendNotification);
    mp3BitrateCombo_.setColour (juce::ComboBox::backgroundColourId,    kBg);
    mp3BitrateCombo_.setColour (juce::ComboBox::textColourId,           kText);
    mp3BitrateCombo_.setColour (juce::ComboBox::outlineColourId,        juce::Colour (0xff333355));
    mp3BitrateCombo_.setColour (juce::ComboBox::arrowColourId,          kAccent);
    mp3BitrateCombo_.setColour (juce::ComboBox::focusedOutlineColourId, kAccent);
    addAndMakeVisible (mp3BitrateCombo_);

    // ---- Sample rate ----
    sampleRateLabel_.setText ("Sample Rate", juce::dontSendNotification);
    sampleRateLabel_.setFont (juce::Font (11.0f));
    sampleRateLabel_.setColour (juce::Label::textColourId, kDim);
    sampleRateLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (sampleRateLabel_);

    sampleRateCombo_.addItem ("Match session", 1);
    sampleRateCombo_.addItem ("44100 Hz",  44100);
    sampleRateCombo_.addItem ("48000 Hz",  48000);
    sampleRateCombo_.addItem ("88200 Hz",  88200);
    sampleRateCombo_.addItem ("96000 Hz",  96000);
    sampleRateCombo_.setSelectedId (1, juce::dontSendNotification);
    sampleRateCombo_.setColour (juce::ComboBox::backgroundColourId,    kBg);
    sampleRateCombo_.setColour (juce::ComboBox::textColourId,           kText);
    sampleRateCombo_.setColour (juce::ComboBox::outlineColourId,        juce::Colour (0xff333355));
    sampleRateCombo_.setColour (juce::ComboBox::arrowColourId,          kAccent);
    sampleRateCombo_.setColour (juce::ComboBox::focusedOutlineColourId, kAccent);
    addAndMakeVisible (sampleRateCombo_);

    // Wire format combo to update MP3 visibility
    formatComboBox_.onChange = [this]{ updateMp3Visibility(); };
    updateMp3Visibility();
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
    const int pad    = 10;
    const int w      = getWidth() - pad * 2;
    const int itemH  = 26;
    const int labelH = 14;
    const int gap    = 4;
    const int rowGap = 6;

    int y = pad;

    titleLabel_.setBounds      (pad, y, w, 20); y += 24;
    y += 4;

    formatLabel_.setBounds     (pad, y, w, labelH); y += labelH + 2;
    formatComboBox_.setBounds  (pad, y, w, itemH);  y += itemH + rowGap;

    bitDepthLabel_.setBounds   (pad, y, w, labelH); y += labelH + 2;
    bitDepthComboBox_.setBounds(pad, y, w, itemH);  y += itemH + rowGap;

    // MP3 bitrate (shown/hidden based on format)
    mp3BitrateLabel_.setBounds  (pad, y, w, labelH); y += labelH + 2;
    mp3BitrateCombo_.setBounds  (pad, y, w, itemH);  y += itemH + rowGap;

    // Sample rate
    sampleRateLabel_.setBounds  (pad, y, w, labelH); y += labelH + 2;
    sampleRateCombo_.setBounds  (pad, y, w, itemH);  y += itemH + rowGap;

    // Normalise toggle
    normaliseToggle_.setBounds  (pad, y, w, itemH);  y += itemH + gap;
    y += gap;

    exportButton_.setBounds       (pad, y, w, itemH); y += itemH + rowGap;
    exportSlicesButton_.setBounds (pad, y, w, itemH);
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

    settings.outputFile = dest;

    // Sample rate
    {
        const int srId = sampleRateCombo_.getSelectedId();
        settings.sampleRate = (srId == 1)
                                  ? processor_.getSampleBuffer().getSampleRate()
                                  : static_cast<double> (srId);
    }

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

    // Normalise
    settings.normalise = normaliseToggle_.getToggleState();

    // MP3 bitrate (only relevant when MP3 format is selected)
    if (formatComboBox_.getSelectedId() == 3)
        settings.mp3Quality = static_cast<AudioFileExporter::Mp3Quality> (
            mp3BitrateCombo_.getSelectedId());

    // ---- WAV metadata --------------------------------------------------------
    // Only meaningful for WAV exports, but safe to populate for all formats
    // (AudioFileExporter ignores these fields for non-WAV outputs).
    settings.bpm           = processor_.getBPM();
    settings.loopInSample  = processor_.getLoopIn();
    settings.loopOutSample = processor_.getLoopOut();

    // Embed slice positions as cue-point markers.
    {
        const auto& slices = processor_.getSliceEngine().getSlices();
        settings.cuePoints.reserve (slices.size());
        for (const auto& s : slices)
            settings.cuePoints.push_back (s.samplePosition);
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

void ExportPanel::updateMp3Visibility()
{
    const bool isMp3 = (formatComboBox_.getSelectedId() == 3);
    mp3BitrateLabel_.setVisible (isMp3);
    mp3BitrateCombo_.setVisible (isMp3);
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
